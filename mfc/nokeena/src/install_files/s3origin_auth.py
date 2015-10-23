#!/usr/bin/python2.6
import hashlib,hmac,base64,time,sys,datetime

curtime= time.gmtime()
amazons3time = time.strftime('%a, %d %b %Y %H:%M:%S GMT', curtime)

url = str(sys.argv[1])

AWSAccessKeyId = str(sys.argv[2])
AWSSecretAccessKey = str(sys.argv[3])
HTTPVerb = "GET"
ContentMD5 = ""
ContentType = ""
CanonicalizedAmzHeaders = ""
CanonicalizedResource = url
string_to_sign = HTTPVerb + "\n" +  ContentMD5 + "\n" +  ContentType + "\n" + amazons3time + "\n" + CanonicalizedAmzHeaders + CanonicalizedResource
sig = base64.b64encode(hmac.new(AWSSecretAccessKey, string_to_sign, hashlib.sha1).digest())

result = sig +"#" + amazons3time
print result
