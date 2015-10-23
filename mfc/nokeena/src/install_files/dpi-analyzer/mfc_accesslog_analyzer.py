import os, glob, sys, logging
import re, time, pickle, gzip, hashlib
import gen_utils, mfc_header
import logging.handlers as handlers

expr = r'"[^"]+"|\'[^\']+\'|[^ ]+'
comp = re.compile(expr)
TOTAL_COLS = 0
fields_old = None
checksum_dict = {}
header_cols={}
mfc_result_file=""
mfc_pbr_file=""
num_files_read = 0
mfcloganalyzer_log = None

#This function reads the last line of the gziped file to check for the signature
#If the signature is present we know that this file has already been read and so return TRUE
def isFileAlreadyProcessessed(filename):
	#Get the content of the file
	with gzip.open(filename, 'r') as stream:
		content = stream.read()
	#Calculate the md5 checksum of the content
	md5 = hashlib.md5(content).hexdigest()
	#Check if the file has already been processed by comparing the calculated checksum with the
        #existing checksum in the dictionary
        #For new files update the checksum and save it using pickle
	if filename not in checksum_dict or checksum_dict[filename] !=  md5:
		checksum_dict[filename] = md5
		return 0
	return 1

#Fire the commands to the router after the number of files to be read limit is reached
def send_cmd_on_threshold_limit(namespace_dict):
	master_iplist = []
	total_ips = 0
	result = None
	cmdstr =""
	#Loop through the dictionary and log it in mfc_accesslog_analyzer.log
	for items in sorted(namespace_dict):
		iplist = namespace_dict[items]
		#put this list in a unique aggregated list for generating the command that is going to be dumped to a file
		master_iplist.extend(x for x in iplist if x not in master_iplist)
		if result != None:
			result += "Namespace: %s  No of server ips: %d"%(items, len(iplist))
		else:
			result = "Namespace: %s  No of server ips: %d"%(items, len(iplist))
		total_ips += len(iplist)
		ipstr = "\n".join(sorted(iplist, reverse=True))
		if result != None:
			result += "\n"+ipstr + "\n"
	if result !=  None:
		result += "\n" + "TOTAL NO OF IP'S: %d"% total_ips + "\n"
		fp_log =  open(mfc_result_file, 'w')
		fp_log.write(result)
		fp_log.close()
	#Go through the sorted aggreaged list of dest ip's to generate the MX router command
	for item in sorted(master_iplist, reverse=True):
		cmdstr += "set policy-options prefix-list redirect-to-proxy %s\n"% item
	#Send the cmd to be dumped in a file
	if len(master_iplist) > 0:
		del master_iplist[:]
		mfcloganalyzer_log.info("Firing/Saving PBR rules to router/file")
		#print cmdstr
		gen_utils.sendCmd(cmdstr, "no", mfc_pbr_file, 'w')
	else:
		mfcloganalyzer_log.info("No PBR rules available in the processed log files")

#Function to valiadate if all the mandatory http headers are are present
def isMandatoryHttpHeadersPresent(headers_list):
	http_headers = ['x-namespace', 's-ip']
	for items in http_headers:
		if items not in headers_list:
			return 0
	return 1	

#Parse the mfc accesslog and dump the namespace name and the list of dest-ip'
def parse_mfc_accesslog(generic_namespace_name, analyzer_conf_dict, mfc_accesslog_path):
	namespace_dict =dict()
	filename = None
	global fields_old
	global num_files_read
	global header_cols
	#Sort the accesslog files based on the timestamp. Latest files will be read first
	accesslog_files = sorted(glob.glob('*access.log*.gz'), key=lambda x: os.path.getmtime(x), reverse=True)
	#Loop through all the accesslog files
	for filename in accesslog_files:
		#print filename
		#Check if the accesslog file is already processessed
		if isFileAlreadyProcessessed(filename):
			continue
		mfcloganalyzer_log.info("Reading the file %s"%filename)
		fp = gen_utils.zlib_file()
		start_time = time.time()
		fp.open(filename)
		line = fp.readline()
		#Loop through till we hit the 'Fields' line in the log file
		while line!= None and not '#Fields' in line:
			line = fp.readline()
		#If the '#Fields' is not present in the log file then skip that file
		if line == None:
			mfcloganalyzer_log.info("The '#Fields' line not present in the accesslog.Skipping the file")
			continue
		#Generate the http header to column mapping
		fields_new = line
		if fields_new != fields_old:
			fields_old = fields_new
			#If the accesslog file does not have all the mandatroy http  headers then skip that file
			http_headers = line[8:].strip()
			headers_list =  http_headers.split()
			if not isMandatoryHttpHeadersPresent(headers_list):
				mfcloganalyzer_log.info("One of the mandatory http headers namespace or server-ip is missing in the accesslog")
				continue
			header_cols = gen_utils.generate_headers_column_mapping(headers_list)
			TOTAL_COLS = len(headers_list)
		num_files_read += 1
		num_lines = 0
		#Now read the rest of the mfc accesslog file line by line
		while line:
			line = fp.readline()
			#The line is empty, just skip it.
			if line == None:
				continue
			line = line.strip()
			#The line is commented, just skip it.
			if len(line) > 0 and line[0] == '#':
				continue
			#Find all the words with Space as delimiter
			#All words with whitespace within double or single quotes  are consisdered one word
			lst =  comp.findall(line)
			#Skip the tunneled data
			if len(lst) == 0 or 'Tunnel' in lst[0]:
				continue
			#If the number of columns logged don't match the number of http headers skip that line
			#Log only for the first line as you don't want to fill the log files
			if len(lst) !=  TOTAL_COLS:
				num_lines += 1
				if num_lines == 1:
					mfcloganalyzer_log.info("%s", line)
					mfcloganalyzer_log.info("# of hdrs in 'Fields' doesn't match # of hdrs generated, hdrs with whitespaces needs to be quoted")
				continue
			namespace = lst[header_cols['x_namespace']]
			dest_ip = lst[header_cols['s_ip']]
			#If the namespace name is not equal to the generic namespace then store the namespace name
			#as a key and the list of server-ip's as the values in a dictionary
			if namespace != generic_namespace_name and namespace != '-':
				#Check if the namespace does not already exist in the dictionary
				if namespace in namespace_dict:
					server_ip = namespace_dict[namespace]
					#If the dest ip is not already there in the list then append it
					if dest_ip not in server_ip:
						server_ip.append(dest_ip)
				else:
					#New namespace so just add it and create a list of dest ip's
					namespace_dict[namespace] = [dest_ip]
		fp.close()
		if str(num_files_read) == analyzer_conf_dict['no_of_accesslog_files_read']:
			#print "SENDING THE FILE"
			num_files_read = 0
			send_cmd_on_threshold_limit(namespace_dict)
		#Persist with the checksum dictionary entries
		with open('log_analyzer_checksum_dict.pickle', 'wb') as f:
			pickle.dump(checksum_dict, f)
		#mfcloganalyzer_log.info("%s seconds", (time.time() - start_time))
		#print time.time() - start_time, "seconds"
	return 0

#Function for the reading the MFC accesslog file and
#generating the namespace name with the list of dest-ip's
def read_mfc_accesslog(analyzer_conf_dict):
	global checksum_dict
	global mfc_result_file
	global mfc_pbr_file
	global mfcloganalyzer_log
	mfc_result_file = mfc_header.RESULT_FILE
	mfc_pbr_file = mfc_header.PBR_FILE
	##TEMP FIX. Generate pbr and analyzer log files with timestamp appended to it's name
	if os.path.isfile(mfc_header.RESULT_FILE):
		mfc_result_file = mfc_header.RESULT_FILE + "_" + str(time.time())
	if os.path.isfile(mfc_header.PBR_FILE):
		mfc_pbr_file = mfc_header.PBR_FILE + "_" + str(time.time())
	#Logger settings
	logformatter = logging.Formatter('%(asctime)s;%(message)s')
	#Use the custom log rotation handler which gzip's the old rotated log file
	handler = gen_utils.NewRotatingFileHandler(mfc_header.LOG_FILE, maxBytes=mfc_header.MAXLOG_SIZE, backupCount=mfc_header.LOGBACKP_COUNT)
	handler.setFormatter(logformatter)
	mfcloganalyzer_log = logging.getLogger('__name__')
	mfcloganalyzer_log.setLevel(logging.DEBUG)
	#Point to the custom log handler
	mfcloganalyzer_log.addHandler(handler)
	#Get the generic namespace name from the specified config file
	filename = analyzer_conf_dict['mfc_config_file']
	generic_namespace_name = gen_utils.get_generic_namespace_name(filename)
	if  generic_namespace_name == None:
		    mfcloganalyzer_log.info("No generic namespace name found in the mfc configuration file")
	#Load the saved checksum_dict dictionary data
	mfc_accesslog_path = analyzer_conf_dict['mfc_accesslog_path']
	os.chdir(mfc_accesslog_path)
	if os.path.isfile('log_analyzer_checksum_dict.pickle'):
		with open('log_analyzer_checksum_dict.pickle', 'rb') as f:
			checksum_dict =  pickle.load(f)
	#Parse the mfc accesslog and dump the namespace name and the list of dest-ip' in a file
	while True:
		parse_mfc_accesslog(generic_namespace_name, analyzer_conf_dict, mfc_accesslog_path)
		time.sleep(3)
	return 0
