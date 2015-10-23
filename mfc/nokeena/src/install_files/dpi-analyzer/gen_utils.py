import ncclient, zlib, gzip, io, glob, os
from jnpr.junos import Device
from jnpr.junos.utils.config import Config
from logging.handlers import RotatingFileHandler
from lxml import etree
import mfc_header
from paramiko import SSHException

cu ='None'

#Get the generic namespace name from the specified config file
def get_generic_namespace_name(filename):
	generic_namespace_name = None
	found = 0
	with open(filename, 'r') as fp:
		for line in fp:
			#Get the namespace name with configuration  'domain any'
			if 'domain any' in line:
				generic_namespace_name = line.split()[1]
			#If the namespace has uri '/' and if its domain matches 'any' then it
			#is the generic namespace name
			if 'uri /' in line:
				if line.split()[1] == generic_namespace_name:
				    found = 1	
				    break
	if found == 1:
	    return generic_namespace_name
	return None

#Custom log rotation handler. Python 2.x does not support compression as part of log rotation
#This class is inherited from RotatingFileHandler. Basically uses the python functionality
#of rotating the log files based on the FileSize
class NewRotatingFileHandler(RotatingFileHandler):
    def doRollover(self):
	#Python log rotation does not work with .gz files. So unzip all the .gz files
	basefile_name = mfc_header.LOG_FILE.split('/')[3].strip()
	logfilepattern = basefile_name + '*.gz'
	logfiles = glob.glob(logfilepattern)
	for filename in logfiles:
	    with gzip.open(filename, 'rb') as zpfile:
		with open(filename[:-3], "wb") as unzpfile:
		    unzpfile.write(zpfile.read())
	#Call the base class RotatingFileHandler doRollover function to roll over the log files
	super(NewRotatingFileHandler, self).doRollover()
	#Code to actually gzip the the rotated log files and delete the actual file 
	logfilepattern = basefile_name + '*[0-9]'
	logfiles = glob.glob(logfilepattern)
	for filename in logfiles:
	    #gzip it only if the filesize is greater than 0
	    if os.path.getsize(filename) > 0:
		with open(filename) as log:
		    with gzip.open(filename + '.gz', 'wb') as comp_log:
			comp_log.writelines(log)
		os.remove(filename)

#Connect the give MX router ip
def Connect_Router(device_fqdn, username, password, dpiloganalyzer_log):
	#Connect to the MX router using netconf. PyEZ netconf library is used here
	try:
		jdev = Device(host=device_fqdn, user=username, password=password)
		jdev.open()
	except ncclient.transport.AuthenticationError, err:
		dpiloganalyzer_log.error(err)
		return 1
	except ncclient.transport.SSHError, err:
		dpiloganalyzer_log.error(err)
		return 1
	except SSHException, err:
                dpiloganalyzer_log.error(err)
                return 1
	global cu
	cu = Config(jdev)
	return 0

#Dump it into a file or fire the command to the MX router and issue commit
#depending on what the user has configured in the analyzer.conf file
def sendCmd(set_cmds, firecmd_to_router, filename, openmode, dpilog):
	commit_failed = 0
	if firecmd_to_router == 'yes':
		try:
			#Fire the commands using netconf
			cu.load(set_cmds, format='set')
			cu.commit()
		except Exception as err:
			print "CMD:"
			print etree.dump(err.cmd)
			print "RESPONSE:"
			print etree.dump(err.rsp)
			commit_failed = 1
	else:
			#Dump the output into a file
			result = open(filename, openmode)
			result.write(set_cmds)
			result.write("\n")
			result.close()
	return commit_failed

class zlib_file():
	def __init__(self, buffer_size=1024*1024*8):
		self.dobj = zlib.decompressobj(16+zlib.MAX_WBITS) #16+zlib.MAX_WBITS -> zlib can decompress gzip
		self.decomp = []
		self.lines = []
		self.buffer_size = buffer_size
 
	def open(self, filename):
		self.fhwnd = io.open(filename, "rb")
		self.eof = False
 
	def close(self):
		self.fhwnd.close()
		self.dobj.flush()
		self.decomp = []

	def decompress(self):
		raw = self.fhwnd.read(self.buffer_size)
		if not raw:
			self.eof = True
		    	self.decomp.insert(0, self.dobj.flush())
		else:
		 	self.decomp.insert(0, self.dobj.decompress(raw))
 
	def readline(self):
		#split
		out_str = []

		while True:
	    		if len(self.lines) > 0:
				return self.lines.pop() + "\n"

			elif len(self.decomp) > 0:
				out = self.decomp.pop()
				arr = out.split("\n")

				if len(arr) == 1:
					out_str.append(arr[0])
				else:
					self.decomp.append(arr.pop())
					arr.reverse()
					out_str.append(arr.pop())
					self.lines.extend(arr)
					out_str.append("\n")
					return "".join(out_str)
			else:
				if self.eof: break
				self.decompress()

		if len(out_str) > 0:
			return "".join(out_str)
 
	def readlines(self):
		lines = []
		while True:
			line = self.readline()
			if not line: break
			lines.append(line)
		return lines


#Generate http header column number mapping
def generate_headers_column_mapping(headers_list):
	header_cols = {}
	#Emunerate the headers list and generate the column number in the order given in the log file
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
	return header_cols
