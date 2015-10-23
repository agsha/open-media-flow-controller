#!/usr/bin/python2.7
import logging
import header, gen_utils,sys,os, shutil
from mdreq import MdReq

NODE_DEVICE_MAP_NAME = '/nkn/log_analyzer/config/clear_filter_rules/device_map'

def main():
	err = 0
	#Logger settings
	logging.basicConfig(filename=header.MFC_LOG_FILE, format='%(asctime)s %(message)s',level=logging.DEBUG)
	mdreq = MdReq()
	#Get the device map name
	device_map_name = str(sys.argv[1])
	NODE_DEVICE_MAP_FQDN = '/nkn/device_map/config/' + device_map_name + '/device_info/fqdn'
	NODE_DEVICE_MAP_USERNAME = '/nkn/device_map/config/' + device_map_name + '/device_info/username'
	NODE_DEVICE_MAP_PASSWORD = '/nkn/device_map/config/' + device_map_name + '/device_info/password'
	#Get the fqdn, username and password
	fqdn = mdreq.query(NODE_DEVICE_MAP_FQDN)
	username = mdreq.query(NODE_DEVICE_MAP_USERNAME)
	password = mdreq.queryCleartext(NODE_DEVICE_MAP_PASSWORD)
	#First connect to the MX router
	err = gen_utils.Connect_Router(fqdn,username, password, logging);
	#Check if the connection to the router succeeded
	if  err == 1:
		logging.error("The command 'clear log-analyzer filter-rules %s' failed. Cannot connect to the device %s", device_map_name,fqdn);
	else:
		#Construct the command to delete the pbr from the router
		set_commands = """
		delete policy-options prefix-list redirect-to-proxy
		set policy-options prefix-list redirect-to-proxy
		"""
		#Fire the command to the router
		err = gen_utils.sendCmd(set_commands, "yes", "","", logging)
		#Check if the commit succeeded
		if err == 0:
			#Create a empty file so that the log-analyzer tool knows the 'clear' command has been fired
			open(header.CLEAR_FILTER_RULES_FILE, 'w').close()
			#If the commit is successful remove the persisted file rules file as well
			pickle_file = '/var/log/'+header.DEST_IP_PICKLE
			tmp_pickle_file = '/var/log/'+header.TMP_DEST_IP_PICKLE
			if os.path.isfile(pickle_file):
				shutil.copyfile(pickle_file, tmp_pickle_file)
				os.remove(pickle_file)
			logging.error("The command 'clear log-analyzer filter-rules %s' succeeded", device_map_name);
		else:
			logging.error("The command 'clear log-analyzer filter-rules %s' failed. Commit failed in the device %s", device_map_name, fqdn);
	return err
#Main function
main()
