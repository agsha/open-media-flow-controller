#!/usr/bin/python2.7
import sys, os
#Take the PYTHONPATH env variable if set 
try:
	sys.path.append(os.environ['PYTHONPATH'])
except KeyError:
	#Default to the MFC python library installation path if env not set
	sys.path.append("/usr/lib/python2.7")
import signal, glob
import logging, time, pickle
import re, header, mfc_accesslog_analyzer, gen_utils
from mdreq import MdReq
from lxml import etree
from datetime import datetime as dt

header_cols={}
expr = r'"[^\\"]*"|\'[^\']+\'|[^ ]+'
comp = re.compile(expr)
fields_old = None
dpiloganalyzer_log = None
TOTAL_COLS = 0
dest_ips={}
cmds_list = []
fire_to_router = ""
frequency = 1

#Generic function to log critical errors.
def log_error(errmsg):
	dpiloganalyzer_log.error(errmsg+"\n")

#Generic function to log critical errors. The script will exit after logging this error
def log_error_exit(errmsg):
	dpiloganalyzer_log.critical(errmsg+"\n")
	sys.exit(0)

#Function to compile a regular expression
def compile_regex(regexp):
        return re.compile(regexp, re.IGNORECASE|re.MULTILINE)

#Function to check if the default action for 'query-string', 'cache-control' and 'pragma' are overridden
def isDefaultRuleOverridden(line, domain_patterns, domain, url, req_cache_control, resp_cache_control, req_pragma, resp_pragma):
	#If the concerned headers dont' have the interested values then don't even proceed just return
	if ('?' not in url) and (req_cache_control != 'no-cache') and (resp_cache_control != 'no-cache') and (req_pragma != 'no-cache') and (resp_pragma != 'no-cache'):
		return 1
	#Walk through the XML Tree and look for the match against the incoming domain
	for domain_pattern in domain_patterns:
		recomp =  compile_regex(domain_pattern.text)
		result = recomp.search(domain)
		#If there is a match then match against the url pattern
		if result:
			urlpatheval = etree.XPathElementEvaluator(domain_pattern.getparent())
			for url_pattern in urlpatheval("uri/uri-pattern"):
				#Compile the url-pattern in the xml file
				recomp = compile_regex(url_pattern.text)
				#Match against the pattern
				res = recomp.search(url)
				if res:
					#If there is a url-pattern match get the interested headers
					elempatheval = etree.XPathElementEvaluator(url_pattern.getparent())
					query_string = elempatheval('querystring-present')
					cc  = elempatheval('cache-control-no-cache')
					pargma_no_cache  = elempatheval('pragma-no-cache')
					#Check if the query-string needs to be cached
					if ('?' in url) and (query_string[0].text == 'cache'):
						#dpiloganalyzer_log.info("The default rule overriden for query-string in the line %s" %line)
						return 1
					#Check if 'cache-conntrol=no-cache' needs to be cached
					if (req_cache_control == 'no-cache' or resp_cache_control == 'no-cache') and (cc[0].text == 'cache'):
						#dpiloganalyzer_log.info("The default rule overriden for cache_control in the line %s" %line)
						return 1
					#Check if 'pragma=no-cache' needs to be cached
					if ((req_pragma == 'no-cache') or (resp_pragma == 'no-cache')) and (pargma_no_cache[0].text == 'cache'):
						#dpiloganalyzer_log.info("The default rule overriden for pragma in the line %s" %line)
						return 1
	return 0

#To determine if an URI is cacheable or not check against the http request header limitations
def checkReqHdrsLimitation(line, list, num_lines):
	req_cache_control = list[header_cols['cs_Cache_Control_']]
	authorization =  list[header_cols['cs_Authorization_']].strip()
	cookie = list[header_cols['cs_Cookie_']].strip()
	req_content_len = list[header_cols['cs_Content_Length_']].strip()
	update = list[header_cols['cs_Update_']].strip()
	req_transfer_encoding =  list[header_cols['cs_Transfer_Encoding_']].strip()
	#If Authorization or Content_Length in http request, Cookie or Update header is present don't cache
	if authorization != '-' or cookie != '"-"' or req_content_len !='-' or update !='-':
		if num_lines == 1:
		    dpiloganalyzer_log.info("Authorization %s cookie %s, req_content_len %s, update %s condition failed in the line %s"% (authorization, cookie, req_content_len, update,line))
		return 0
	#If transfer-encoding is chunked do not cache
	if req_transfer_encoding !='-' and req_transfer_encoding == 'chunked':
		if num_lines == 1:
		    dpiloganalyzer_log.info("Transfer encoding chunked in the line %s"% line)
		return 0
	#If Cache-Control http header has one of the below values don't cache.
	if req_cache_control == 'private' or req_cache_control=='no-store':
		if num_lines == 1:
		    dpiloganalyzer_log.info("req_cache_control private or no-store in the line %s"% line)
		return 0
	return 1

#To determine if an URI is cacheable or not check against the http response header limitations
def checkRespHdrsLimitation(line, list, num_lines):
	content_len = list[header_cols['sc_bytes_content']].strip()
	transfer_encoding = list[header_cols['sc_Transfer_Encoding_']].strip()
	resp_cache_control = list[header_cols['sc_Cache_Control_']].strip()
	byte_range = list[header_cols['cs_Range_']][1:-1]
	#In the http response if Content-Length is not present or it's value is zero don't cache
	if content_len == '-' or content_len == '0':
		if num_lines == 1:
		    dpiloganalyzer_log.info("content_len not present or zero in the line %s"% line)
		return 0
	#If chuncked encoding with sub-encoding then don't cache
	if transfer_encoding != '-' and 'chunked' in transfer_encoding and ',' in transfer_encoding:
		if num_lines == 1:
		    dpiloganalyzer_log.info("Transfer-encoding with sub-encoding in the line %s"% line)
		return 0
	#If Byte range present in the request and chunked encoding in response then don't cache
	if byte_range !='-' and 'chunked' in transfer_encoding:
		if num_lines == 1:
		    dpiloganalyzer_log.info("Byte range is present and chunked encoding in response in the line %s"% line)
		return 0
	#If Cache-Control has max-age or s-maxage and its value is zero then don't cache
	if '=' in resp_cache_control:
		cc = resp_cache_control.split("=")
		if (cc[0].strip() == 'max-age' or cc[0].strip() == 's-max-age') and (cc[1].strip() == '0'):
			if num_lines == 1:
			   dpiloganalyzer_log.info("max-age is zero in the line %s"% line)
			return 0
	return 1

#Function to check  other limitations to determine of an URI is cacheable or not
def checkOtherLimitations(line, list, domain_patterns, num_lines):
	domain = list[header_cols['cs_Host_']]
	request = list[header_cols['cs_request']][1:-1]
	req_str = request.split()
	request_type = req_str[0]
	url = req_str[1]
	statuscode = list[header_cols['sc_Status_']]
	req_pragma = list[header_cols['cs_Pragma_']].strip()
	resp_pragma = list[header_cols['sc_Pragma_']].strip()
	req_cache_control = list[header_cols['cs_Cache_Control_']].strip()
	resp_cache_control = list[header_cols['sc_Cache_Control_']].strip()
	date = list[header_cols['sc_Date_']].strip()[1:-1]
	expires = list[header_cols['sc_Expires_']].strip()[1:-1]

	#If the response has statuscode and it's value is not 200 or 206 don't cache
	if statuscode != '0' and statuscode != '200' and statuscode != '206':
		if num_lines == 1:
		    dpiloganalyzer_log.info("status code is not equal to 200 or 206 in the line %s"%line)
		return 0
	#If no max-age calculate the expiry time. If expired then don't cache
	if resp_cache_control != '-' and not ('=' in resp_cache_control and resp_cache_control.split("=")[0].strip() == 'max-age') and date !='-' and expires !='-':
		current_date = dt.strptime(date, '%a, %d %b %Y %H:%M:%S %Z')
		expires_date = dt.strptime(expires, '%a, %d %b %Y %H:%M:%S %Z')
		if expires_date < current_date:
			dpiloganalyzer_log.info("Date expired in the line %s"%line)
			return 0
	#If the incoming request is not a GET or HEAD request then don't cache
	if request_type != 'GET' and request_type != 'HEAD':
		if num_lines == 1:
		    dpiloganalyzer_log.info("Http request is not GET or HEAD in the line %s"%line)
		return 0
	#If the url size is > 512 bytes then don't cache
	if len(url) > header.URL_MAX_SIZE:
		if num_lines == 1:
		    dpiloganalyzer_log.info("URL length greater then max-size(512) in the line %s"%line)
		return 0
	#If query-string is present and if it is hex encoded then don't cache
	if '%' in url:
		if num_lines == 1:
		    dpiloganalyzer_log.info("URL is hex-encoded in the line %s"%line)
		return 0
	#Check if the querys-string, cache_control and pragma values are overridden
	if not isDefaultRuleOverridden(line, domain_patterns, domain, url, req_cache_control, resp_cache_control,req_pragma, resp_pragma):
		if num_lines == 1:
		    dpiloganalyzer_log.info("Query-string present in url or Cache-control or Pragma has no-cache in the line %s"%line)
		return 0
	return 1

#Function to check if an URI is cacheable or not
#Check for certain conditons in Request,response headers and other conditions as well
def isCacheable(line, list, domain_patterns, num_lines):
	if checkReqHdrsLimitation(line, list, num_lines) and checkRespHdrsLimitation(line, list, num_lines) and checkOtherLimitations(line, list, domain_patterns, num_lines):
		return 1
	return 0

#Function to valiadate if all the mandatory http headers are are present
def isMandatoryHttpHeadersPresent(headers_list):
	http_headers = ['cs(Host)','cs-request','sc(Status)','sc-bytes-content','sc(Age)','cs(Range)','cs(Pragma)','cs(Cache-Control)','sc(Etag)','cs(Referer)', 's-ip','cs(Authorization)','cs(Cookie)','cs(Content-Length)','cs(Update)','cs(Transfer-Encoding)','sc(Transfer-Encoding)','sc(Date)','sc(Expires)','sc(Pragma)','sc(Cache-Control)']
	for items in http_headers:
		if items not in headers_list:
			return 0
	return 1	

#Generate http header column index mapping
def generate_headers_index_mapping(format):
	headers_list = format.split()
	#Emunerate the headers list and generate the column index in the order given in the log file
	for num, lst in enumerate(headers_list,1):
		if lst == '-':
			continue
		field = headers_list[num-1].strip()
		if field[0] == '"' or field[0] == '\'':
			field = field[1:-1]
		#Replace the following fileds in the header name with '_' as they are python keywords
		for ch in ['-', '(', ')']:
			field = field.replace(ch, '_')
		header_cols[field] = num-1

#This function returns 1 if there is a match against domain header pattern
#for the host and the the uri pattern for the url
def is_golden_config_match(golden_config_dict, lst, line):
    domain = lst[header_cols['cs_Host_']].strip()
    request = lst[header_cols['cs_request']].strip()[1:-1]
    if domain == '-' or request == '-':
	return 0
    request_list = request.split()
    try:
	url = request_list[1].strip()
	for namespace_name in sorted(golden_config_dict):
	    pattern_list = golden_config_dict[namespace_name]
	    domain_regex_pattern = pattern_list[0]
	    uri_regex_pattern = pattern_list[1]
	    if domain_regex_pattern.search(domain) and uri_regex_pattern.search(url):
		return 1
    except IndexError:
	    dpiloganalyzer_log.info("The line %s gives raise to index out of range exception", line)
    return 0
	
#result = domain_regex_pattern.search(list[header_cols['cs_Host_']])

#Go through the dpilog entry and fire the acl's to the router
def read_dpi_logs(domain_patterns, golden_config_dict, analyzer_conf_dict, mdreq, device_map_name):
	headers_list = []
	global cmds_list
	global header_cols
	global comp
	global fields_old
	global TOTAL_COLS
	num_lines = 0
	filename = None
	try:
	    fire_pbr_for_cacheable_url =  analyzer_conf_dict['fire_pbr_for_cacheable_url'].strip().lower()
	except KeyError as e:
	    fire_pbr_for_cacheable_url = 'no'
	MAX_FILTER_RULES_NODE = '/nkn/device_map/config/'+device_map_name+'/filter_config/max_rules'
	try:
		max_filter_rules = mdreq.query(MAX_FILTER_RULES_NODE)
	except KeyError as e:
		dpiloganalyzer_log.error("The filter_config/max_rules node is not available. Defauling to max-rules of 10000")
		max_filter_rules =  10000
		
	#Look for files starting with dpilog_ keyword and with a number as extension 
	#Sort the accesslog files based on the timestamp. Latest files will be accessed first
	dpi_logfiles = sorted(glob.glob('dpilog_*.[0-9]*'), key=lambda x: os.path.getmtime(x), reverse=True)
	for filename in dpi_logfiles:
		#print filename
		with open(filename, 'a+') as fp:
			#Check if the file has already been processed by looking for the signature in the last line
			fp.seek(-9, os.SEEK_END)
			if fp.read().strip() != header.SIGNATURE:
				dpiloganalyzer_log.info("Reading the file %s"%filename)
				#New file so go to the begining of the file
				fp.seek(0, 0)
				#Read the fields format line
				line = fp.readline()
				#Loop through till we hit the 'Fields' line in the log file
				while line!= None and not '#Fields' in line:
					line = fp.readline()
				#If the '#Fields' is not present in the log file then skip that file
				if line == None:
					dpiloganalyzer_log.info("The '#Fields' line not present in the accesslog.Skipping the file")
					continue
				#Generate the http header to column mapping
				fields_new = line
				#Check if the format has changed while reading the files
				if fields_new != fields_old:
					fields_old = fields_new
					#If the accesslog file does not have all the mandatroy http  headers then skip that file
					http_headers = line[8:].strip()
					for items in http_headers.split():
					    if items[0] == '"' or items[0] == '\'':
						    headers_list.append(items[1:-1])
					    else:
						    headers_list.append(items)
					if not isMandatoryHttpHeadersPresent(headers_list):
						dpiloganalyzer_log.info("One or more of the mandatory http headers is missing in the dpilog")
						continue
					#Generate the http header to column mapping
					header_cols = gen_utils.generate_headers_column_mapping(headers_list)
					TOTAL_COLS = len(headers_list)
					del headers_list[:]
				num_lines = 0
				for line in fp:
					list =  comp.findall(line)
					#If the total columns in the log don't match the TOTAL_COLS just continue
					if len(list) != TOTAL_COLS:
						continue
					#If the Host header is not there in the log skip that line
					host = list[header_cols['cs_Host_']]
					if host == '-':
					    continue
					#If the host header matches with the domain pattern and the uri matches the uri pattern
					#of the golden config then the pbr should be fired
					#result = domain_regex_pattern.search(list[header_cols['cs_Host_']])
					result =  is_golden_config_match(golden_config_dict,list, line)
					num_lines += 1
					if result or (fire_pbr_for_cacheable_url =='yes' and isCacheable(line, list, domain_patterns, num_lines)):
						#Get the dest-ip from the log
						destip = list[header_cols['s_ip']]
						#Append only the unique dest-ip to the list
						if not destip in dest_ips:
							#This way of appending in list is faster than the call to 'append'
							dest_ips[destip] = 1
							set_str = 'set policy-options prefix-list redirect-to-proxy %s' %(destip)
							cmds_list.append(set_str)
						else:
							dest_ips[destip] += 1
				fp.write(header.SIGNATURE)
				try:
				    if len(cmds_list) >= int(max_filter_rules):
					sendPbrs(cmds_list)
				except ValueError as e:
				    dpiloganalyzer_log.info("Integer value has to be specified for max_filter_rules in the analyzer.conf. Defaulting to 10000")
				    max_filter_rules = 10000
				    if len(cmds_list) >= int(max_filter_rules):
					sendPbrs(cmds_list)
			else:
				continue
		fp.close()

#Function for post 'clear log-analyzer filter-rules' command.
#Take the delta of the dest-ip's and remve the 'clear_filter_rules.txt' file and
#the temporary dest pickle file
def restart_after_clear_filter_rules():
	saved_destips={}
	delta_dict = {}
	#Check if you are restarting the tool after the 'clear filter_rules' command has been fired
	#If so dump only the delta
	if os.path.isfile(header.TMP_DEST_IP_PICKLE):
		if os.stat(header.TMP_DEST_IP_PICKLE).st_size > 0:
			with open(header.TMP_DEST_IP_PICKLE, 'rb') as f:
				saved_destips =  pickle.load(f)
				#This gives the delta as a 'set'
				for key in dest_ips:
					if not key in saved_destips:
						delta_dict[key] = 1
		#Remove the temporary pickle file.
		os.remove(header.TMP_DEST_IP_PICKLE)
	#Remove the clear_filter_rules.txt file
	if os.path.isfile(header.CLEAR_FILTER_RULES_FILE):
		os.remove(header.CLEAR_FILTER_RULES_FILE)
	#Convert the 'set' to 'dictionary' before dumping it
	if len(delta_dict) > 0:
		with open(header.DEST_IP_PICKLE, 'wb') as f:
			pickle.dump(delta_dict, f)
	delta_dict.clear()

#Function to dump the pbr to a file or to a router
def sendPbrs(cmds_list):
	err = 0
	if len(cmds_list) > 0:
		#Get the commands to be fired to the router
		cmdstr = "\n".join(cmds_list)
		#print cmdstr
		#dpiloganalyzer_log.info("The transaction to be fired is \n%s\n", cmdstr)
		if fire_to_router.lower() == 'yes':
			dpiloganalyzer_log.info("FIRING THE FILTER-RULES TO THE ROUTER\n")
		else:
			dpiloganalyzer_log.info("FIRING THE FILTER-RULES TO THE FILE\n")
		#Fire the transaction to the router or to a file
		err = gen_utils.sendCmd(cmdstr, fire_to_router, header.DPI_ANALYZER_RESULT_FILE, 'a+', dpiloganalyzer_log)
		if err == 0:
			#Persist with the dictionary of destination-ip's
			with open(header.DEST_IP_PICKLE, 'wb') as f:
				if not os.path.isfile(header.CLEAR_FILTER_RULES_FILE):
					pickle.dump(dest_ips, f)
			del cmds_list[:]
			#Clear the dictionary if it has more than 25000 entries
			if len(dest_ips) > header.MAX_DICT_SIZE:
				dest_ips.clear()
		else:
			dpiloganalyzer_log.error("The 'commit' of the filter-rules failed in the device")
	else:
		dpiloganalyzer_log.info("No unique pbr's available to be fired to the router")

#Signal handler function for SIGALRM
#Send the pbr to the router at the specified time interval
def sendCmdToDevice(signum, frame):
    #Whatever files have been read so far send the pbr for it to the router or file
    sendPbrs(cmds_list)
    #Set the sigalarm again
    signal.alarm(frequency)

#Signal handler function. Exit on 'pm process log-analyzer terminate' or CTRL-C
def exit_gracefully(signum, frame):
	#Check if this is a restart after the 'clear_filter_rules' command.
	if os.path.isfile(header.CLEAR_FILTER_RULES_FILE):
		restart_after_clear_filter_rules()
	#Whatever files have been read so far send the pbr for it to the router or file
	sendPbrs(cmds_list)
	#Perform any cleanup actions in the logging system
	logging.shutdown()
	#Disable the alarm
	signal.alarm(0)
	sys.exit(0)

#Parse the override_rules.xml file and return the domain_patterns list
def parse_override_rules_xml():
	tree = etree.parse(header.OVERRIDE_RULES_XML)
	xpatheval = etree.XPathEvaluator(tree)
	return xpatheval("domain/domain-pattern")


#Parse the domin_regex in the mfc-config, group them and compile it in one shot
#Return the compiled regular expression
def parse_golden_config(mdreq, device_map_name):
	golden_config_dict = {}
	namespace_name=""
	tunnel_all_namespace=""
	#Get the generic namespace name from the specified config file
	CONFIG_FILE_NODE = '/nkn/log_analyzer/config/config_file/url'
	try:
		filename = mdreq.query(CONFIG_FILE_NODE)
	except KeyError as e:
		dpiloganalyzer_log.info("Not able to query the config-file/log-analyzer node. Defauling to local")
		filename = 'local'
	if filename == 'local':
		filename = header.DEFAULT_GOLDEN_CONFIG
	else:
		filename =  header.MFC_CONFIG
	generic_namespace_name = gen_utils.get_generic_namespace_name(filename)
	with open(filename, 'r') as fp:
		for line in fp:
			if 'delivery protocol http client-request tunnel-all' in line:
				tunnel_all_namespace = line.split()[1].strip()
			if ' domain ' in line:
				lst = line.split()
				namespace_name = lst[1].strip()
				#Skip the generic namespace and the default one
				if namespace_name == 'default' or namespace_name == generic_namespace_name or					namespace_name ==  tunnel_all_namespace:
					continue
				if lst[3] == 'regex':
				    domain_pattern = lst[4][1:-1].strip()
				else:
				    if lst[3].strip() == 'any':
					domain_pattern = '.*'
				    else:
					domain_pattern = lst[3].strip()
				golden_config_dict[namespace_name] = [compile_regex(domain_pattern)]
			if 'match uri' in line:
				if namespace_name == 'default' or namespace_name == generic_namespace_name or 					namespace_name == tunnel_all_namespace:
				    continue
				lst = line.split()
				if lst[4].strip() == 'regex':
				    uri_pattern = lst[5][1:-1].strip()
				else:
				    uri_pattern = "^" + lst[4].strip() + '.*'
				pattern_list = golden_config_dict[namespace_name]
				pattern_list.append(compile_regex(uri_pattern))
	return golden_config_dict

	
#Parse the analyzer.conf file and populate the dictionary with (key, value) pairs
def parse_analyzer_conf():
	mfc_mode= 0
	config_path=""
	#Get the mode in which the script is runnig. Need for validating the analyzer.conf file entries
	if (len(sys.argv) == 5 or len(sys.argv) == 3) and (str(sys.argv[1]) == '--mode' or str(sys.argv[1]) == '-m'):
		mfc_mode = 1
	##Check if valid config-path is specified by the user
	if len(sys.argv) == 3 and (str(sys.argv[1]) == '--config-path' or str(sys.argv[1]) == '-C'):
		config_path = str(sys.argv[2]).strip()
	elif len(sys.argv) == 5 and (str(sys.argv[3]) == '--config-path' or str(sys.argv[3]) == '-C'):
		config_path = str(sys.argv[4]).strip()
	if len(config_path) > 0:
		#If the user does not end the directory path with '/' then append it
		if config_path.endswith('/'):
			config_path += header.ANALYZER_CONF
		else:
			config_path += "/"+header.ANALYZER_CONF
		if not os.path.isfile(config_path):
			log_error("The configuration files do not exist in the specified path\n")
	else:
		config_path = header.DEFAULT_CONF_PATH + header.ANALYZER_CONF
		if not os.path.isfile(config_path):
			log_error("The configuration files do not exist in the default path\n")

	analyzer_conf_dict={}
	with open(config_path, 'r') as fp:
		for line in fp:
			#Ignore comments and blank space lines in the file
			if '#' in line or not line.strip():
				continue
			lst = line.split('=')
			analyzer_conf_dict[lst[0].strip()] =	lst[1].strip()
	#Validate the analyzer.conf file for valid entries
	#validate_analyzer_conf(mfc_mode, analyzer_conf_dict)
	return analyzer_conf_dict

#Fire mirror configuration to the router
def fireMirrorConfiguration(analyzer_conf_dict):
	try:
	    fpc_slot = analyzer_conf_dict['fpc_slot']
	    output_interface_name = analyzer_conf_dict['output_interface_name']
	    next_hop_ip = analyzer_conf_dict['next_hop_ip']
	    output_interface_name = analyzer_conf_dict['output_interface_name']
	    output_interface_ip_mask = analyzer_conf_dict['output_interface_ip_mask']
	    mfc_mirror_interface_ip = analyzer_conf_dict['mfc_mirror_interface_ip']
	    mac_address = analyzer_conf_dict['mac_address']
	except KeyError as e:
	    dpiloganalyzer_log.info("Mandatory mirror configuration entry %s is missing in the analyzer.conf file. Mirror configuration will not be fired", e)
	    return
	#Validate if all the MX router mirror configuration entries are entered in the router.
	#If not given a message will be logged and the mirror configuration will not be configured in the router
	if fpc_slot == "" or output_interface_name == "" or next_hop_ip == "" or output_interface_name == "" or output_interface_ip_mask == "" or mfc_mirror_interface_ip == "" or mac_address == "":
	    dpiloganalyzer_log.info("Please make sure all the router mirror configuration entries are given in the analyzer.conf file. No configuration done in the router")
	    return
	#MX router mirror configuration commands
	cmd_list = ['set firewall family inet filter media_flow term HTTP_mirror from port http',
		    'set firewall family inet filter media_flow term HTTP_mirror then port-mirror-instance one',
		    'set firewall family inet filter media_flow term media_flow_default then accept',
		    'set chassis fpc %s port-mirror-instance one'% fpc_slot,
		    'set forwarding-options port-mirroring instance one input rate 1',
		    'set forwarding-options port-mirroring instance one family inet output interface %s next-hop %s' %(output_interface_name, next_hop_ip),
		    'set interface %s description mirror-destination-interface unit 0 family inet address %s arp %s mac %s'% (output_interface_name, output_interface_ip_mask, mfc_mirror_interface_ip, mac_address)
		]
	cmdstr = "\n".join(cmd_list)
	#Fire the transaction to the router
	gen_utils.sendCmd(cmdstr, "yes", "", "")

#Fire One time config into the router for setting up the pbr
def fireOneTimeCommands(analyzer_conf_dict):
	try:
	    mfc_interface_ip = analyzer_conf_dict['mfc_interface_ip']
	    mx_client_interface = analyzer_conf_dict['mx_client_interface']
	    mx_server_interface = analyzer_conf_dict['mx_server_interface']
	    mx_mfc_interface = analyzer_conf_dict['mx_mfc_interface']
	    mx_mfc_interface_ip_mask = analyzer_conf_dict['mx_mfc_interface_ip_mask']
	except KeyError as e:
	    dpiloganalyzer_log.info("Mandatory onetime configuration entry %s is missing in the analyzer.conf file. Nothing will be configured in the router", e)
	    return
	#Validate if all the MX router mirror configuration entries are entered in the router.
	#If not given a message will be logged and the one time configuration will not be configured in the router
	if mfc_interface_ip == "" or mx_client_interface == "" or mx_server_interface == "" or mx_mfc_interface == "" or mx_mfc_interface_ip_mask == "":
	    dpiloganalyzer_log.info("Please make sure all the router one time configuration entries for applying the PBR are given in the analyzer.conf file. One time configuration is not configured in the router now")
	    return
	cmd_list = ['set firewall family inet filter media_flow term to_mfc from prefix-list redirect-to-proxy',
		    'set firewall family inet filter media_flow term to_mfc then routing-instance media_flow',
		    'set policy-options prefix-list redirect-to-proxy',
		    'set routing-instances media_flow instance-type forwarding',
		    'set routing-options interface-routes rib-group inet dpi',
		    'set routing-options rib-groups dpi import-rib inet.0',
		    'set routing-options rib-groups dpi import-rib media_flow.inet.0',
		    'set routing-instances media_flow routing-options static route 0.0.0.0/0 next-hop %s' % mfc_interface_ip,
		    'set interfaces %s description client-interface unit 0 family inet filter input-list media_flow' %mx_client_interface,
		    'set interfaces %s description server-interface unit 0 family inet filter input-list media_flow' %mx_server_interface,
		    'set interfaces %s description mfc_tproxy_interface unit 0 family inet address %s' % (mx_mfc_interface, mx_mfc_interface_ip_mask)
	]
	cmdstr = "\n".join(cmd_list)
	#Fire the transaction to the router
	gen_utils.sendCmd(cmdstr, "yes", "", "")
	
	
#Do the common initializations here
def init():
	#Stop the script on CTRL-C
	signal.signal(signal.SIGINT, exit_gracefully)
	signal.signal(signal.SIGTERM, exit_gracefully)
	signal.signal(signal.SIGALRM, sendCmdToDevice)

#Print the usage and exit
def print_usage():
	print "Invalid format"
	print "Usage: python analyzer_pbr.py [[--mode|-m] mfc][[--config-path|-C] <Path of the config file>]"
	sys.exit(0)

#Main function	
def main():
	global dpiloganalyzer_log
	global dest_ips
	global frequency
	global fire_to_router
	domain_patterns=""
	device_map_name=""
	#Basic common initialization
	init()
	#Parse the command-line arguments
	#If the number of arguments is greater than 1 make sure the arguments values are correct
	if len(sys.argv) > 1:
		if len(sys.argv) ==3 and ((str(sys.argv[1]) == '--mode' or str(sys.argv[1]) == '-m') and str(sys.argv[2]) != 'mfc'): 
			print_usage()
		elif len(sys.argv) == 3 and str(sys.argv[1]) != '--config-path' and str(sys.argv[1]) != '-C' and str(sys.argv[1]) != '--mode' and str(sys.argv[1]) != '-m':
			print_usage()
		elif len(sys.argv) == 5 and ((str(sys.argv[1]) != '--mode' and str(sys.argv[1]) != '-m') or str(sys.argv[2]) != 'mfc' or (str(sys.argv[3]) != '--config-path' and str(sys.argv[3]) != '-C')):
			print_usage()
		elif len(sys.argv) == 2 or len(sys.argv) == 4:
			print_usage()
		
	#If the script is launched in mfc mode then read the MFC accesslog files
	if len(sys.argv) > 1 and ((str(sys.argv[1]) == '--mode' or str(sys.argv[1]) == '-m') and str(sys.argv[2]) == 'mfc'):
		#Parse the analyzer.conf file and populate the values in a dictionary
		analyzer_conf_dict =  parse_analyzer_conf()
		mfc_accesslog_analyzer.read_mfc_accesslog(analyzer_conf_dict)
	else:
		#Logger settings
		logformatter = logging.Formatter('%(asctime)s;%(message)s')
		#Use the custom log rotation handler which gzip's the old rotated log file
		handler = gen_utils.NewRotatingFileHandler(header.LOG_FILE, maxBytes=header.MAXLOG_SIZE, backupCount=header.LOGBACKP_COUNT)
		handler.setFormatter(logformatter)
		dpiloganalyzer_log = logging.getLogger('__name__')
		dpiloganalyzer_log.setLevel(logging.DEBUG)
		#Point to the custom log handler
		dpiloganalyzer_log.addHandler(handler)
		#Get the handle to the instance of the class that enables us to query the mgmtd database
		mdreq = MdReq()
		#Get the device-map name bound to the log-analyzer
		dmap = mdreq.queryIterate("/nkn/log_analyzer/config/device_map")
		for key,val in dmap.iteritems():
			device_map_name = val['value']
		if device_map_name != "":
			#Get the mode in which the log-analyzer tool has to be run
			CMDMODE_NODE = '/nkn/log_analyzer/config/device_map/' + device_map_name + '/command_mode/inline'
			mode = mdreq.query(CMDMODE_NODE)
			if mode.lower() == 'true':
				fire_to_router = 'yes'
			else:
				fire_to_router = 'no'
			FREQUENCY_NODE = header.DEVICE_MAP_COMMON + device_map_name + '/filter_config/frequency'
			frequency = mdreq.query(FREQUENCY_NODE)
			#Convert the frequency in seconds as the SIGALRM requires it to be in seconds
		    	frequency = int(frequency) * 60
		else:
			dpiloganalyzer_log.error("The log-analyzer is not associated to a device. The commands will be pushed to a file")
			fire_to_router = "no"
			#Default to 2hrs frequency to dump into the file
			frequency = 7200 
		#Parse the analyzer.conf file and populate the values in a dictionary
		analyzer_conf_dict =  parse_analyzer_conf()
		signal.alarm(frequency)
		#Parse Override_rules.xml only if you want to determine the cacheability of url
		try:
		    if analyzer_conf_dict['fire_pbr_for_cacheable_url'].strip().lower()  == 'yes':
			domain_patterns =parse_override_rules_xml()
		except KeyError as e:
		    dpiloganalyzer_log.info("The analyzer.conf file is missing the entry %s. Defaulting to 'no' and cacheability check will not be performed", e)
		#Parse the golden-config.txt file and get the list of domain-regex patterns
		golden_config_dict =  parse_golden_config(mdreq, device_map_name)
		#Check if the user wants the commands to be fired to the router
		if fire_to_router == 'yes':
			#Get the device details
			FQDN_NODE = header.DEVICE_MAP_COMMON + device_map_name + '/device_info/fqdn'
			USERNAME_NODE = header.DEVICE_MAP_COMMON + device_map_name + '/device_info/username'
			PASSWD_NODE = header.DEVICE_MAP_COMMON + device_map_name + '/device_info/password'
			device_fqdn =  mdreq.query(FQDN_NODE)
			device_username =  mdreq.query(USERNAME_NODE)
			device_passwd =  mdreq.queryCleartext(PASSWD_NODE)
			#First connect to the MX router
			err = gen_utils.Connect_Router(device_fqdn, device_username, device_passwd, dpiloganalyzer_log);
			if err == 1:
				dpiloganalyzer_log.error("Unable to connect to the router %s. The filter-rules will be written to a file.", device_fqdn)
				fire_to_router = "no"
			else:
				if analyzer_conf_dict['fire_one_time_config'].lower() == 'yes':
				    #Fire the one time commands into the router
				    fireOneTimeCommands(analyzer_conf_dict)
				    #Fire the mirror configuration commands into the router
				    fireMirrorConfiguration(analyzer_conf_dict)
		#Load the dest-ip pickle into the dictionary
		os.chdir("/var/log")
		if os.path.isfile(header.DEST_IP_PICKLE) and os.stat(header.DEST_IP_PICKLE).st_size > 0:
			with open(header.DEST_IP_PICKLE, 'rb') as f:
				dest_ips =  pickle.load(f)
		#Read the dpilogs genereated by the dpi-tool
		while True:
			read_dpi_logs(domain_patterns, golden_config_dict, analyzer_conf_dict,mdreq, device_map_name);
			time.sleep(2)

#Main function
main()
