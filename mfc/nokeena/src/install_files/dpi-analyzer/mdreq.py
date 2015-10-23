import re
import subprocess



class MdReq:

    prog = '/opt/tms/bin/mdreq'
    regex = re.compile("(?P<key>.*?) = (?P<value>.*\(?) (?P<type>.*)", re.IGNORECASE)



    def __query__(self, node, type = 'get', mode = '-'):
	keys = {}
	args = [self.prog, '-s', 'query', type, mode, node]

	try:
	    p = subprocess.Popen(args, stdout = subprocess.PIPE)
	except OSError (errno, strerror):
	    print "OS Error ({0}) : {1}".format(errno, strerror)
	except ValueError:
	    print "Bad arguments to popen in class MdReq"
	else:
	    bindings = p.communicate()
	    for binding in bindings[0].split('\n'):
		if not binding == None:
		    r = self.regex.search(binding)
		    if not r == None:
			keys[r.groupdict()['key']] = r.groupdict()

	    return keys



    def query(self, node):
	return self.__query__(node)[node]['value']

    def queryCleartext(self, node):
	return self.__query__(node, type= 'get', mode = 'cleartext')[node]['value']

    def queryIterate(self, node):
	return self.__query__(node, type = 'iterate')

    def querySubtree(self, node):
	return self.__query__(node, type = 'iterate', mode = 'subtree');


    def action(self, node, bindings):
	keys = {}
	args = [self.prog,  '-s', 'action', node]
	args.extend(bindings)

	try:
	    p = subprocess.Popen(args, stdout = subprocess.PIPE)
	except OSError (errno, strerror):
	    print "OS Error ({0}) : {1}".format(errno, strerror)
	except ValueError:
	    print "Bad arguments to popen in class MdReq"
	else:
	    bindings = p.communicate()
	    for binding in bindings[0].split('\n'):
		if not binding == None:
		    r = self.regex.search(binding)
		    if not r == None:
			keys[r.groupdict()['key']] = r.groupdict()

	    return keys



class NCount:

    prog = '/opt/nkn/bin/nkncnt'
    counters = {}

    def __init__(self, name):
	args = [self.prog, '-s', name, '-t', '0']

	try:
	    p = subprocess.Popen(args, stdout = subprocess.PIPE)
	except OSError (errno, strerror):
	    print "OS Error ({0}) : {1}".format(errno, strerror)
	except ValueError:
	    print "Bad arguments to popen in class NCount"
	else:
	    regex = re.compile("(?P<key>.*?)[ ]+ = (?P<value>.*?) .*", re.IGNORECASE)
	    out = p.communicate()
	    for item in out[0].split('\n'):
		if not item == None:
		    r = regex.search(item)
		    if not r == None:
			self.counters[r.groupdict()['key']] = r.groupdict()['value']

    def getCounter(self, name):
	return self.counters[name]

    def searchCounter(self, regex):
	rets = {}
	r = re.compile(regex)
	for k in self.counters.keys():
	    n = r.search(k)
	    if not n == None:
		rets[k] = self.counters[k]

	return rets




if __name__ == '__main__':
    mdreq = MdReq()
    k = mdreq.querySubtree('/nkn/accesslog/config')

    for i,j in k.iteritems():
	print i, " -----> ", j['value']

    k = NCount("dc_").searchCounter(".+free.+")
    print k

    k = NCount("dc_1.dm2_free").getCounter("dc_1.dm2_free_pages")
    print k

    k = MdReq().queryIterate("/nkn/nvsd/diskcache/config/disk_id")
    for i,j in k.iteritems():
	print i, " -----> ", j['value']

    k = MdReq().queryIterate("/nkn/nvd")
    print k

    k = MdReq().query("/nkn/accesslog/config/syslog")
    print k
