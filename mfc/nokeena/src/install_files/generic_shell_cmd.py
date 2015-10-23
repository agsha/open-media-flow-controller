#!/usr/bin/python
#
#Import the library files

import commands
import sys

inpcommand = sys.argv[1]

output = commands.getoutput(inpcommand)

print output


