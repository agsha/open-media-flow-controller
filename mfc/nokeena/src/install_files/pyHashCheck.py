'''py_HashCheck.py - Python source to prototype '''
'''plug-in functionality for SSP'''

import md5;

def checkMD5(hostData, hostLen, uriData, queryHash):
    scrtKey = "a98z9Gv9H3l"
    m = md5.new()
    m.update(scrtKey)
    m.update("http://")

    hostPortion = hostData[:hostLen]
#    print "Host Portion is ", hostPortion, "len is ", len(hostPortion)
    m.update(hostPortion)

    hashPortion = uriData[:uriData.find("&h")]
#    print "Hash portion is ", hashPortion
    m.update(hashPortion)

    calHash = m.hexdigest()
    print "Cal Hash is ", calHash, "len is ", len(calHash)
    print "Qry Hash is ", queryHash, "len is ", len(queryHash)
    if calHash == queryHash:
        return 0
    else:
        return -1

