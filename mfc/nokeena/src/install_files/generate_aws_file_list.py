from xml.dom import minidom
from sys import argv
import os
import re

def get_and_parse_xml(wget_file_object):
	global url_cnt
	regex = re.compile(base_url)
	url_http_resp_cnt = 0
	uri_list_file_object = open(uri_list, "w")
	uri_list_detail_file_object = open(uri_list_detail, "w");
	for line in wget_file_object:
    		url_name = regex.findall(line)
		if (len(url_name)):
			index_filename = line.split()[5]
			index_filename = index_filename[1:-1]

	xmldoc = minidom.parse(index_filename)
	itemlist = xmldoc.getElementsByTagName('Contents')
	for s in itemlist:
		idlist =  s.getElementsByTagName("Key")
		sizelist =  s.getElementsByTagName("Size")
		etaglist = s.getElementsByTagName("ETag")
		lmlist = s.getElementsByTagName("LastModified")
		for id in idlist:
			for size in sizelist:
				value = int(size.childNodes[0].nodeValue)
				if value > 0 :
					url_cnt = url_cnt + 1
					s = id.childNodes[0].nodeValue
					uri = "<url>http://"+base_url+'/'+s+"</url>"
					etag = "<etag>"+etaglist[0].childNodes[0].nodeValue+"</etag>"
					cl = "<cl>"+size.childNodes[0].nodeValue+"</cl>"
					lm = "<lm>"+lmlist[0].childNodes[0].nodeValue+"</lm>"
					print >>uri_list_file_object, uri
					print >>uri_list_detail_file_object, uri+etag+cl+lm

	os.remove(index_filename)


base_url = argv[1]
wget_out_file = argv[2]
uri_list_detail = argv[3]
uri_list = argv[4]
url_cnt = 0
finished = 0
wget_file_object = open(wget_out_file, "r")
regex = re.compile('HTTP\/1\.[0-1]')
regex_finish = re.compile('FINISHED')
http_resp_cnt = 0
url_cnt = 0

for line in wget_file_object:
	list = regex.findall(line)
	if (len(list)):
		http_resp_cnt = http_resp_cnt + 1;
		words = line.split()
		http_respcode = words[1]
	list_finish = regex_finish.findall(line)
	if (len(list_finish)):
		finished = 1

if (http_resp_cnt == 1):
	if (http_respcode == "200"):
		wget_file_object.seek(0)
		get_and_parse_xml(wget_file_object)
		print "WGET_URL_FOUND:"+str(url_cnt)+":"+str(finished)
	elif (http_respcode == "404"):
		print "WGET_URL_NOT_FOUND:0"
	else:
		print "WGET_INVALID_STATUS_CODE_"+http_respcode+":0"
else:
	print ;
