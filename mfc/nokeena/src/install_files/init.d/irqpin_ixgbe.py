#! /usr/bin/env python
import sys
import subprocess
import os
import datetime
import shutil
import time
import syslog

iflist=[]
ifspeedlist=[]
def getiflist():
	global iflist
	global ifspeedlist
	iflist = []
	ifspeedlist = []
	ifconflines = os.popen( "ifconfig | grep \"HWaddr\"", "r" ).read().splitlines()
	for line in ifconflines:
		splits = line.split()
		if splits.count != 0:
			if splits[0].isspace() == False:
				ethtooltext = os.popen( "ethtool " + splits[0] + " | grep \"Speed\"", "r" ).read().splitlines()
				if len( ethtooltext ) != 0 and ethtooltext[0].split().count >= 1 and ethtooltext[0].split()[1].replace("Mb/s", "").isdigit():
					iflist.append(splits[0])
					ifspeedlist.append(ethtooltext[0].split()[1].replace("Mb/s", ""))
	return


cpucount=0
def getcpucount():
	global cpucount
	cpuinflines = os.popen( "cat /proc/cpuinfo | grep processor | wc -l", "r" ).read().splitlines()
	if len( cpuinflines) != 0 and cpuinflines[0].split().count >= 1:
		cpucount = int( cpuinflines[0].split()[0] )
	return

siblings=0
def getsiblings():
	global siblings
	cpuinfout = os.popen( "cat /proc/cpuinfo | grep siblings", "r" )
	cpuinflines = cpuinfout.read().splitlines()
	if len( cpuinflines) != 0:
		if cpuinflines[0].split().count >= 2:
			siblings = int( cpuinflines[0].split()[2] )
	return

cores=0
def getcores():
	global cores
	cpuinflines = os.popen( "cat /proc/cpuinfo | grep \"cpu cores\"", "r" ).read().splitlines()
	if len( cpuinflines) != 0 and cpuinflines[0].split().count >= 3:
		cores = int( cpuinflines[0].split()[3] )
	return

def parseupdateirqsysconfig( configfile, banirqval ):
	confile = open( configfile, "r" )
	tmpfile = open( "/tmp/irq-bal-env-syscon.out", "w" )
	wrote=False
	mytime=datetime.datetime.now()
	while 1:
		line = confile.readline()
		if not line: break
		if line.find( "#IRQBALANCE_BANNED_INTERRUPTS" ) >= 0:
			continue
		elif line.find( "IRQBALANCE_BANNED_INTERRUPTS" ) >= 0 and line.find( "#" ) < 0:
			tmpfile.write( "#" + str(mytime) + "  #" + line )
			tmpfile.write( "IRQBALANCE_BANNED_INTERRUPTS=\"" + banirqval + "\"\n" )
			wrote=True
		else:
			tmpfile.write( line )
	if wrote != True:
		tmpfile.write( "IRQBALANCE_BANNED_INTERRUPTS=\"" + banirqval + "\"\n" )
	tmpfile.close()
	confile.close()
	shutil.copy( "/tmp/irq-bal-env-syscon.out", configfile )
	return
	
def dostartirqbalance(bannedirqs):
	parseupdateirqsysconfig( "/etc/sysconfig/irqbalance", bannedirqs )
	os.environ["IRQBALANCE_BANNED_INTERRUPTS"]=bannedirqs
	syslog.syslog(syslog.LOG_NOTICE, os.popen( "/etc/init.d/irqbalance start", "r" ).read())
	syslog.syslog(syslog.LOG_NOTICE,os.getenv( "IRQBALANCE_BANNED_INTERRUPTS" ))

def isixgbedriver( ifname ):
	driverlines = os.popen( "ethtool -i " + ifname, "r" ).read().splitlines()
	if len( driverlines) != 0 and driverlines[0].split().count >= 1:
		if driverlines[0].split()[1] == "ixgbe":
			return True
	return False
	

irqlist=[]
def didirqchange():
	getiflist()
	global irqlist
	tmplist = []
	counter=0
	for key in iflist:
		if counter <= len(ifspeedlist):
			speed=ifspeedlist[counter]
			if int(speed) >= 10000:
				intlist = os.popen( "cat /proc/interrupts | grep "  + key, "r" ).read().splitlines()
				for item in intlist:
					val = item.split()[0]
					val = val.strip(":")
					tmplist.append(val)
		counter = counter + 1
	if( irqlist != tmplist ):
		irqlist = tmplist
		return True
	else:
		return False


def balanceirq16():
	sock0CpuList=[8,1,9,2,10,3,11]
	sock1CpuList=[12,5,13,6,14,7,15]
	numsockets = cpucount / siblings
	counter=0			
	socketno=0;
	cpuno=1
	bannedirqs=""
	if cpucount != 16 or siblings != 8:
		syslog.syslog(syslog.LOG_NOTICE, "Non VXA system. Exiting without balancing!")
		return
	syslog.syslog(syslog.LOG_NOTICE, os.popen( "/etc/init.d/irqbalance stop", "r" ).read())
	for key in iflist:
		syslog.syslog(syslog.LOG_NOTICE, key)
		if counter <= len(ifspeedlist):
			speed=ifspeedlist[counter]
			if int(speed) >= 10000 and isixgbedriver( key ) == True:
				intlist = os.popen( "cat /proc/interrupts | grep "  + key, "r" ).read().splitlines()
				if socketno >= numsockets:
					socketno=0
				intcount = 0
				for item in intlist:
					val = item.split()[0]
					val = val.strip(":")
					bannedirqs = bannedirqs + val + " "
					if item.find("lsc") >= 0:
						hexcpu = "FFFF"
						cmd=os.popen( "echo " + hexcpu  + " > /proc/irq/" + val + "/smp_affinity", "r")
						syslog.syslog(syslog.LOG_NOTICE, "echo " + hexcpu  + " > /proc/irq/" + val + "/smp_affinity #lsc")
					else:
						if item.find("tx-0") >= 0:
							syslog.syslog(syslog.LOG_NOTICE, "tx-0")
							if socketno == 0: 
								cpuno = 1
							elif socketno == 1:
								cpuno = 2**4
						else:
							choice = 1
							#print socketno
							if( socketno == 0 ):
								if( intcount >= len(sock0CpuList)):
									intcount=0
								choice = int(sock0CpuList[intcount])
								#syslog.syslog( syslog.LOG_NOTICE,str(choice))
								#print choice
							else:
								if( intcount >= len(sock1CpuList)):
									intcount=0
								choice = int(sock1CpuList[intcount])
								#syslog.syslog(syslog.LOG_NOTICE,str(choice))
								#print choice
							cpuno = 2 ** choice
							intcount = intcount + 1
						hexcpu = hex(cpuno).replace("0x","")
						cmd=os.popen( "echo " + hexcpu + " > /proc/irq/" + val + "/smp_affinity", "r")
						syslog.syslog(syslog.LOG_NOTICE, "echo " + hexcpu  + " > /proc/irq/" + val + "/smp_affinity")
				socketno = socketno + 1
			counter = counter + 1
	try:
		dostartirqbalance( bannedirqs )
	except IOError:
		syslog.syslog(syslog.LOG_NOTICE, "IOError, hence remounting root filesystem to read-write")
		syslog.syslog(syslog.LOG_NOTICE, os.popen( "mount -o remount,rw /", "r" ).read())
		dostartirqbalance(bannedirqs)
		syslog.syslog(syslog.LOG_NOTICE, os.popen( "mount -o remount,ro /", "r" ).read())
		syslog.syslog(syslog.LOG_NOTICE, "Done!")
	return


def balanceirq32():
	sock0CpuList=[16,1,17,2,18,3,19,4,20,5,21,6,22,7,23]
	sock1CpuList=[24,9,25,10,26,11,27,12,28,13,29,14,30,15,31]
	numsockets = cpucount / siblings
	counter=0			
	socketno=0;
	cpuno=1
	bannedirqs=""
	if cpucount != 32:
		syslog.syslog(syslog.LOG_NOTICE, "Non Pacifica system. Exiting without balancing!")
		return
	syslog.syslog(syslog.LOG_NOTICE, os.popen( "/etc/init.d/irqbalance stop", "r" ).read())
	for key in iflist:
		syslog.syslog(syslog.LOG_NOTICE, key)
		if counter <= len(ifspeedlist):
			speed=ifspeedlist[counter]
			if int(speed) >= 10000:
				intlist = os.popen( "cat /proc/interrupts | grep "  + key, "r" ).read().splitlines()
				if socketno >= numsockets:
					socketno=0
				intcount = 0
				for item in intlist:
					val = item.split()[0]
					val = val.strip(":")
					bannedirqs = bannedirqs + val + " "
					if item.find("-fp-") >= 0:
						choice = 1
						#print socketno
						if( socketno == 0 ):
							if( intcount >= len(sock0CpuList)):
								intcount=0
							choice = int(sock0CpuList[intcount])
							#syslog.syslog( syslog.LOG_NOTICE,str(choice))
							#print choice
						else:
							if( intcount >= len(sock1CpuList)):
								intcount=0
							choice = int(sock1CpuList[intcount])
							#syslog.syslog(syslog.LOG_NOTICE,str(choice))
							#print choice
						cpuno = 2 ** choice
						intcount = intcount + 1
						hexcpu = hex(cpuno).replace("0x","")
						cmd=os.popen( "echo " + hexcpu + " > /proc/irq/" + val + "/smp_affinity", "r")
						syslog.syslog(syslog.LOG_NOTICE, "echo " + hexcpu  + " > /proc/irq/" + val + "/smp_affinity")
					else:
						hexcpu = "FFFFFFFF"
						cmd=os.popen( "echo " + hexcpu  + " > /proc/irq/" + val + "/smp_affinity", "r")
						syslog.syslog(syslog.LOG_NOTICE, "echo " + hexcpu  + " > /proc/irq/" + val + "/smp_affinity #lsc")
				socketno = socketno + 1
			counter = counter + 1
	try:
		dostartirqbalance( bannedirqs )
	except IOError:
		syslog.syslog(syslog.LOG_NOTICE, "IOError, hence remounting root filesystem to read-write")
		syslog.syslog(syslog.LOG_NOTICE, os.popen( "mount -o remount,rw /", "r" ).read())
		dostartirqbalance(bannedirqs)
		syslog.syslog(syslog.LOG_NOTICE, os.popen( "mount -o remount,ro /", "r" ).read())
		syslog.syslog(syslog.LOG_NOTICE, "Done!")
	return

def pollandbalance():
	while True:
		if( didirqchange() == True ):
			syslog.syslog(syslog.LOG_NOTICE, "IRQs changed")
			#balanceirq16()
			#balanceirq32()
			syslog.syslog(syslog.LOG_NOTICE, os.popen( "/etc/init.d/irqbalance stop", "r" ).read())
			bannedirqs=""
			scriptout = os.popen("/opt/nkn/bin/set_irq_affinity.sh")
			while 1:
                		line = scriptout.readline()
                		if not line: break
                		if line.find( "BANNEDIRQ" ) >= 0:
					syslog.syslog(syslog.LOG_NOTICE, line)
					bannedirqs=line.split(":")[1]
                		else:
					syslog.syslog(syslog.LOG_DEBUG, line)
                        		continue
			bannedirqs=bannedirqs.lstrip(" ")
			bannedirqs=bannedirqs.rstrip("\n")
			syslog.syslog(syslog.LOG_NOTICE, bannedirqs)
			try:
				dostartirqbalance( bannedirqs )
			except IOError:
				syslog.syslog(syslog.LOG_NOTICE, "IOError, hence remounting root filesystem to read-write")
				syslog.syslog(syslog.LOG_NOTICE, os.popen( "mount -o remount,rw /", "r" ).read())
				dostartirqbalance(bannedirqs)
				syslog.syslog(syslog.LOG_NOTICE, os.popen( "mount -o remount,ro /", "r" ).read())
				syslog.syslog(syslog.LOG_NOTICE, "Done!")
		time.sleep(60)
#Call the methods.
getiflist()
getcpucount()
getsiblings()
getcores()
pollandbalance()
