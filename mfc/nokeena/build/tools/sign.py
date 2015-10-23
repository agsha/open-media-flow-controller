#!/usr/bin/env python

from __future__ import print_function

"""
A simple client for SignServer

We contact a signing server, and for each file to be signed, send a
digest (actually the hex formatted digest) to be signed along with
user name and the path of the file that we hashed.
This limits the traffic over the network, and since the server sends
back a PEM encoded signature, the entire transaction is thus ascii.

We pay attention to the name we were invoked as, and look for
myname.cf to pickup defaults for the url to contact and which
hash to use.

Options:
    -c "config"
        load config from "config"

    -d  bump the debug level

    -u "url"
        connect to the given "url" (host:port)

    -h "hash"
        use "hash" to compute the digest we send to the server.
        The default (sha256) is sufficient for any reasonable
        signature method, though it is perhaps best to use the same
        method as the server.
        So if signing with RSA+SHA1, use sha1 as the digest to send.
        This is especially important if the server will use the digest
        directly.  If we send the wrong input it will return an error.

    -o "ofile"
        controls the file we will save the signature in.
        The default is '.', which means append the extension provided
        by the server to the file that was signed.
        Thus if signing '/some/file' and the server tells us to use
        '.sig' we will save the signature in '/some/file.sig'
        If "ofile" is '-' the signature will be written to stdout.

    --help
        Provilde a complete set of possible command-line options.	

The output of the --help command-line option provides the following:

usage: sign.py [--help] [-C CERTFILE] [-c CONFIG] [-d]
               [-h {sha256,sha384,sha512,sha224,sha1}] [-o OFILE] [-r TRIES]
               [-u URL] [-T FILESFROM]
               [File [File ...]]

positional arguments:
  File                  File to be signed

optional arguments:
  --help                show this message
  -C CERTFILE           Certificate of public key
  -c CONFIG             Configuration file with defaults
  -d                    Add debugging output.
  -h {sha256,sha384,sha512,sha224,sha1}
                        Hashing algorithm to be used (default sha256).
  -o OFILE              Print sign output to ofile (default='.').
  -r TRIES              Number of times to retry server connections
  -u URL                Connect to the given "url" (host:port)
  -T FILESFROM, --files-from FILESFROM
                        get names to hash from FILE
"""

"""
RCSid:
	$Id: sign.py,v 1.15 2015/03/16 20:36:20 sjg Exp $

	@(#) Copyright (c) 2012 Simon J. Gerraty
	@(#) Copyright (c) 2014, Juniper Networks, Inc.

	This file is provided in the hope that it will
	be of use.  There is absolutely NO WARRANTY.
	Permission to copy, redistribute or otherwise
	use this file is hereby granted provided that
	the above copyright notice and this notice are
	left intact.

	Please send copies of changes and bug-fixes to:
	sjg@crufty.net

"""

import sys
import os.path
import socket
import time
import errno

# Python version 2 and version 3 compatibility functions
if sys.version_info[0] == 2:
    # b'str' and 'str' are equivalent
    # but not u'str'
    def b(s):
        if isinstance(s, unicode):
            return s.encode("latin-1")
        return s

    def s(b):
        if isinstance(b, unicode):
            return b.encode("latin-1")
        return b

else:
    # u'str' and 'str' are equivalent
    # but not b'str'
    def b(s):
        if isinstance(s, bytes):
            return s
        return s.encode("latin-1")

    def s(b):
        if isinstance(b, str):
            return b
        return b.decode("latin-1")

# Useful utility functions
def basename(path, ext=None):
    """Return the basename of the path"""
    b = os.path.basename(path)
    if ext:
        x = b.rfind(ext)
        if x > 0:
            return b[0:x]
    return b

def getConfig(conf, key, default=None):
    """return value of key if present in config, else default"""
    if key in conf:
        return conf[key]
    return default

def loadConfig(name, conf={}):
    """Add key value pairs into conf"""
    cf = open(name, 'r')
    for line in cf:
        if line.startswith('#'):
            continue
        if line.find('=') > 0:
            k,v = line.split('=', 1)
            conf[k.strip()] = v.strip()
    return conf

def url2sock_address(url):
    """parse url and return appropriate socket family and address"""
    if url.startswith('/'):
        sock_family = socket.AF_UNIX
        connect_to = url
    else:
        sock_family = socket.AF_INET
        host = None
        if url.find(':') >= 0:
            (host,port) = url.split(':')
            port = int(port)
        else:
            port = int(url)
        if port < 0 or port > 65535:
            raise ValueError('0 <= port <= 65535')
        if not host:
            host = 'localhost'

        connect_to = (host,port)
    return (sock_family,connect_to)

def urlconnect(sock_family,connect_to,tries=1):
    for i in range(tries):
        try:
            em = ''
            sock = socket.socket(sock_family, socket.SOCK_STREAM)
            sock.connect(connect_to)
            break
        except Exception as e:
            m = ': {0}: {1}: {2}'.format(e.errno, connect_to, e.strerror)
            em = 'ERROR'+m
            if isinstance(e, socket.gaierror):
                if not e.errno in [2]:          # most not worth retrying
                    break
            elif isinstance(e, socket.herror):
                if not e.errno in [2]:          # most not worth retrying
                    break
            secs = i % 8
            if secs > 0:
                time.sleep(secs)
            if secs == 0 and i + 1 < tries:
                sys.stderr.write('WARNING'+m+'. retrying...\n')
            sock.close()
            sock = None
    if em:
        if sock:
            sock.close()
            sock = None
    return (sock,em)

def timestamp(fmt='@ %s [%Y-%m-%d %T]'):
    """format current time"""
    return time.strftime(fmt)

def error(msg, exit=1):
    """Print an error message to stderr and exit"""
    p = 'ERROR: '
    if msg.startswith('ERROR'):
        p = ''
    sys.stderr.write('{} {}{}\n'.format(timestamp(), p, msg))
    sys.exit(exit)

def juniper_hack(filename):
    """Return True to keep the file or False to prune it"""

    # JUNOS hack: make it easier to migrate old/weird branches.
    # where folk may pass us things like OPENSSL=/path/to/openssl
    if filename.find('=') > 0 or filename.endswith('_key.pem'):
        return False
    return True

def parse_options(argv, xoptf=None, conf={}, filterfileargs=None, debug=0):
    """Parse the command-line options
    Usage:
      script [-C certs][-c config][-d][-h hash][-o outfile]
             [-r tries][-u url][--help][file [file]...]
    """
    import argparse

    Myname = basename(sys.argv[0], '.py')
    Mydir = os.path.dirname(sys.argv[0])

    myconf = Myname + '.cf'
    for d in [Mydir, '.']:
        cf = d + '/' + myconf
        if os.path.exists(cf):
            conf = loadConfig(cf, conf)

    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--help', action="help", help="show this message")
    parser.add_argument('-C', dest='certfile',
                        help="Certificate of public key")
    parser.add_argument('-c', dest='config', default=None,
                        help="Configuration file with defaults")
    parser.add_argument('-d', dest='debug', action='count',
                        default=argparse.SUPPRESS,
                        help="Add debugging output.")
    parser.add_argument('-h', dest='hash', default='sha256',
                        choices=['sha256', 'sha384', 'sha512', 'sha224',
                                 'sha1'],
                        help="Hashing algorithm to be used (default sha256).")
    parser.add_argument('-o', dest='ofile', default='.',
                        help="Print sign output to ofile (default='.').")
    parser.add_argument('-r', dest='tries', default=10,
                        help="Number of times to retry server connections")
    parser.add_argument('-u', dest='url',
                        help='Connect to the given "url" (host:port)')
    if not 'user' in conf:
        parser.add_argument('-x', action='store_true',
                            help="Username is sent with signing request")
    parser.add_argument('files', metavar='File',
                        help="File to be signed", nargs='*')
    parser.add_argument('-T', '--files-from', dest='filesfrom',
                        help="get names to hash from FILE")

    # Add additional parser options
    if xoptf:
        xoptf(parser)

    args = parser.parse_args(argv[1:])
    
    d = vars(args)

    # Gather the filenames to be hashed from the command-line
    if d['files']:
        if filterfileargs:
            files = []
            for f in d['files']:
                if filterfileargs(f):
                    files.append(f)
        else:
            files = d['files']
        del d['files']
    else:
        files = []

    # Load a config file
    if d['config']:
        conf = loadConfig(d['config'], conf)
        del d['config']

    # Command-line overrides a loaded configuration
    for k in d:
        if d[k]:
            conf[k] = d[k]

    # If there are a huge number of files to be signed, it is useful
    # to read the list from either stdin or a file rather than on the
    # command line.
    if 'certfile' in conf and 'filesfrom' in conf:
        error('incompatible arguments: -C {} -T {}'.format(d['certfile'], d['filesfrom']))
    elif 'certfile' in conf and len(files) > 0:
        error('incompatible arguments: -C {} and files: {}'.format(d['certfile'], files))
    elif 'filesfrom' in conf:
        if conf['filesfrom'] == '-':
            # read stdin as a list of files one per line
            readfiles = sys.stdin
        else:
            # read files to hash from provided file
            readfiles = open(conf['filesfrom'], 'rb')
        for line in readfiles:
            # Note: We are explicitly NOT checking these files with
            # filterfileargs() callback. We would expect the user to
            # use grep or some other stream filter for that case.
            files.append(line.strip())
        readfiles.close()

    # Potentially increase the current debug level
    if 'debug' in conf and conf['debug'] > debug:
        debug = conf['debug']

    if debug:
        print("==== begin config =====", file=sys.stderr)
        for k,v in conf.items():
            print("conf[%s]=%s" % (k, v), file=sys.stderr)
        print("==== end config =====", file=sys.stderr)

    if debug:
        print("===== begin argument list =====", file=sys.stderr)
        for f in files:
            print('{}'.format(f), file=sys.stderr)
        print('===== end argument list =====', file=sys.stderr)

    return (conf, files)

def write_signature(sig, fname, ofile, ext, debug=0):
    """Write a signature to the output file"""

    if debug > 1:
        print("\n==> write_signature: fname='{}', ofile='{}', ext='{}')\n".format(fname, ofile, ext), file=sys.stderr)

    if ofile == '-':
        print(s(sig), end='')
    else:
        if ofile == '.':
            if not ext:
                error("No signature extention defined.")
            sfile = fname + ext
        elif ofile.startswith('.'):
            sfile = fname + ofile
        else:
            sfile = ofile
        open(sfile, 'wb').write(sig)

        if debug:
            print("wrote {} bytes into {}".format(len(sig), sfile), file=sys.stderr)

class SignClient(object):
    """base Sign Client"""

    def __init__(self, conf={}, debug=0):
        """save setup"""
        self.conf = conf
        port = getConfig(conf, 'ListenPort', 6161)
        self.url = getConfig(conf, 'url', 'localhost:{}'.format(port))
        hname = getConfig(conf, 'hash', 'sha256')
        m = __import__('hashlib', fromlist=[hname])
        self.hashfunc = getattr(m, hname)
        self.hname = hname
        if 'debug' in conf and conf['debug'] > debug:
            self.debug = conf['debug']
        else:
            self.debug = debug
        self.tries = int(getConfig(conf, 'tries', 10))
        self.sock = None
        self.peername = None

    def hash_file(self, file):
        """hash file using the indicated method
        If file.$hash exists and is up to date,
        we will use its content.
        """
        h = self.hashfunc()
        hfile = '{}.{}'.format(file, h.name)
        try:
            sth = os.stat(hfile)
            # size of hfile needs to be what we expect
            if sth.st_size == (1 + 2 * h.digest_size):
                # and it needs to be up to date
                stf = os.stat(file)
                if sth.st_mtime >= stf.st_mtime:
                    f = open(hfile, 'rb')
                    x = f.read()
                    f.close()
                    return x
        except:
            pass
        f = open(file, 'rb')
        for line in f:
            h.update(line)
        f.close()
        return h.hexdigest()+'\n'

    def server_name(self):
        if self.peername:
            return '{} [{}]'.format(self.url, self.peername[0])
        else:
            return self.url

    def send_command_try(self, msg, debug=0):
        """Attempt to send a command to the server and return the response

        We should be called within try block
        """

        sock = self.sock
        if not sock:
            sock_family,connect_to = url2sock_address(self.url)
            sock,response = urlconnect(sock_family, connect_to, self.tries)
            if sock:
                self.peername = sock.getpeername()
            else:
                return response
        sock.send(b(msg))

        # Read from the server and accumulate
        # Using loop through sock.recv can take
        # a very long time.
        if msg.startswith('certs:'):
            # certificate hierarchy may not fit in one response
            # This is the very slow way to get a signature, so
            # only loop for certificate commands.
            r = []
            response = None
            while True:
                result = sock.recv(4096)
                if not result:
                    break
                result = s(result)
                r.append(result)
            response = ''.join(r)
        else:
            # maximum signature is a PEM signature of a 4096
            # bit key used by Microsoft UEFI signer. In base64
            # this will need around 16 lines of around 65
            # characters, less than 2K bytes. Double it just
            # because this is the python suggested default.
            response = sock.recv(4096)
            response = s(response)

        self.sock = sock
        return response

    def send_command(self, msg, debug=0):
        """Send a command to the server and return the response

        Calls send_command_try to do the actual deed so we can capture
        exceptions and retry as needed.
        """

        if self.debug > debug:
            debug = self.debug
        if debug:
            print('sending cmd: {}'.format(msg), file=sys.stderr)

        tries = self.tries
        for i in range(tries):
            try:
                response = self.send_command_try(msg, debug)
                if response:
                    break
            except ValueError as e:
                response = 'ERROR: {0}: {1}'.format(self.server_name(), e.message)
                break
            except Exception as e:
                # XXX might need a list or errno's for which
                # retry is pointless
                if hasattr(e, 'errno'):
                    response = 'ERROR: {0}: {1}: {2}'.format(e.errno, self.server_name(), e.strerror)
                else:
                    response = 'ERROR: {0}: {1}'.format(self.server_name(), e.message)
                    break
                secs = i % 8
                if secs > 0:
                    time.sleep(secs)
                if secs == 0 and i + 1 < tries:
                    sys.stderr.write(response + '. retrying...\n')
                if self.sock:
                    self.sock.close()
                    self.sock = None

        if not response:
            # server may have rejected us due to acl
            response = 'ERROR: no response from: {} for {}'.format(self.server_name(), msg)

        if response.startswith('ERROR'):
            error(response)

        return response

    def cleanup(self):
        """cleanup client state"""
        if self.sock:
            self.sock.close()
            self.sock = None

    def get_certs(self):
        """fetch the certs file and write it out"""
        if not self.conf['certfile']:
            return

        certsFile = None
        if self.conf['certfile'] == '-':
            certsFile = sys.stdout
        else:
            certsFile = open(self.conf['certfile'], 'wb')

        response = self.send_command('certs:')
        if self.conf['certfile'] != '-':
            response = b(response)
        certsFile.write(response)
        certsFile.close()
        self.cleanup()

    def sign(self, path, myhash, debug=0):
        """Sign the hash."""
        msg = ''
        if 'user' in self.conf:
            msg += 'user={}'.format(self.conf['user'])
        msg = '{} path={} hash={}'.format(msg, path, myhash)
        signature = response = self.send_command(msg, debug)

        # The signature will potentially have a set of atribute-value
        # pairs. Seprate them
        avl = {}
        if response.startswith('#set:'):
            knobs,signature = response.split('\n', 1)
            knobs = s(knobs)
            for knob in knobs.split()[1:]:
                k,v = knob.split('=', 1)
                avl[k] = v

        return (avl, signature)

def main(klass=SignClient, xoptf=None, conf={}, filterfileargs=None, debug=0):
    """Simple driver for class SignClient or derivatives.

    """
    
    conf, args = parse_options(sys.argv, xoptf, conf, filterfileargs, debug=0)
    client = klass(conf)
    if 'certfile' in conf and conf['certfile']:
        # ignore the argument list
        client.get_certs()
        if args:
            print('Warning: Processed certs request. - ignoring files:')
            print(*args, sep=' ')
    else:
        if 'debug' in conf and conf['debug'] > debug:
            debug = conf['debug']
        ofile = getConfig(conf, 'ofile', '.')
        # Default to letting the main or the server tell us the suffix
        # to use.
        sig_ext = getConfig(conf, 'sig_ext', None)
    
        # Some hashes may take a long time and it is better to fully
        # occupy a worker thread rather than make it wait for hashing
        # operations on the client side. So, loop through the files to
        # hash them and then loop through the files to sign them.
        if debug:
            print('===== begin hashing {} files ====='.format(len(args)), file=sys.stderr)
        hashes = {}
        for f in args:
            hashes[f] = client.hash_file(f)
        if debug:
            print('===== end hashing {} files ====='.format(len(args)), file=sys.stderr)
            print('===== begin signing hashes =====', file=sys.stderr)

        for f in args:
            knobs, signature = client.sign(f, hashes[f], debug)
    
            # This will generally never set the signature extension
            if ofile == '.' and not sig_ext:
                # the only one we care about
                if 'sig_ext' in knobs:
                    sig_ext = knobs['sig_ext']
    
            signature = b(signature)
            write_signature(signature, f, ofile, sig_ext, debug)

        if debug:
            print('===== end signing hashes =====', file=sys.stderr)
    pass

if __name__ == '__main__':
    import pwd
    conf = {}
    conf['user'] = pwd.getpwuid(os.getuid())[0]
    # It is better to let the server determine if we are a .sig or .esig
    # suffix. However, to change the default, uncomment the next line.
    # conf['sig_ext'] = '.sig'
    try:
        main(SignClient, None, conf, filterfileargs=juniper_hack, debug=0)
    except SystemExit:
        raise
    except:
        # yes, this goes to stdout
        print("ERROR: {}".format(sys.exc_info()[1]))
        raise
    sys.exit(0)
