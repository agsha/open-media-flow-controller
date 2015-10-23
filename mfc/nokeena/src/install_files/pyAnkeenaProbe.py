from urllib2 import Request, urlopen, URLError, HTTPError
import sys
try:
    url = sys.argv[1]
    domain = sys.argv[2]
    url = url.lstrip()
except IndexError, e:
    print 'URL argument not passed'
    sys.exit(1)
if len(url) == 0:  
    print 'URL not given'
    sys.exit(1)
headers = {'HOST':domain}
req = Request(url,None,headers)
try:
    response = urlopen(req)
except HTTPError, e:
    print 'Server Error:'
    print 'Error Info: ', e.code ,e.msg
except URLError, e:
    print 'URL Error:'
    print 'Reason: ', e.reason
else:
    print 'Success'
