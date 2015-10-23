#!/usr/bin/python
#
#       File: pacifica_resiliency.py
#
#       Description: The script monitors hardware errors in pacifica and
#                    logs the events to RE. In case of un-correctable errors,
#                    the script triggers a fpc restart
#
#       Created On : 21 November, 2014
#       Author: Jeya ganesh babu J
#
#       Copyright (c) Juniper Networks Inc., 2014
#


import os
import pdb
import sys
import time
import re
import string
import socket
import subprocess
import fnmatch
import struct
import shutil

global syslog_server
syslog_server = '128.0.0.1'
global syslog_port
syslog_port = 6333
global syslog_socket
global prv_ce_count
prv_ce_count = 0
global prv_ue_count
prv_ue_count = 0
global HZ
HZ = 30
global ONEHR
ONEHR = 3600
global DIMM_ECC_INTERVAL
DIMM_ECC_INTERVAL = 1000
global DIMM_ECC_CE_THRESHOLD
DIMM_ECC_CE_THRESHOLD = 10000
global DIMM_ECC_CE_PER_HR_THRESHOLD
DIMM_ECC_CE_PER_HR_THRESHOLD = 1000
global DIMM_ECC_UE_THRESHOLD
DIMM_ECC_UE_THRESHOLD = 1
global cur_time_diff
cur_time_diff = 0
global CPU_TEMP_LIMIT
CPU_TEMP_LIMIT = 103
global I2C_CLK_BUF_ADDR
I2C_CLK_BUF_ADDR = 0x69
global CLK_BUF_DEV_ADDR
CLK_BUF_DEV_ADDR = 0x89
global CLK_BUF_VEN_ADDR
CLK_BUF_VEN_ADDR = 0x87
global I2C_CLK_BUF_DEV_ID
I2C_CLK_BUF_DEV_ID = '0x17'
global I2C_CLK_BUF_VEN_ID
I2C_CLK_BUF_VEN_ID = '0x31'
global I2C_ERROR_THRESHOLD
I2C_ERROR_THRESHOLD = 100
global CAVE_CREEK_DEV
CAVE_CREEK_DEV = '0016'
global BRCM1_DEV
BRCM1_DEV = '0100'
global BRCM2_DEV
BRCM2_DEV = '8200'
global MARVEL1_DEV
MARVEL1_DEV = '0500'
global MARVEL2_DEV
MARVEL2_DEV = '8700'
global PCI_NON_FATAL_ERROR_THRESHOLD
PCI_NON_FATAL_ERROR_THRESHOLD = 1000
global i2c_fail_count
i2c_fail_count = 0
global cc_ce_count
cc_ce_count = 0
global cc_ue_count
cc_ue_count = 0
global cc_fatal_count
cc_fatal_count = 0
global brcm1_ce_count
brcm1_ce_count = 0
global brcm1_ue_count
brcm1_ue_count = 0
global brcm1_fatal_count
brcm1_fatal_count = 0
global brcm2_ce_count
brcm2_ce_count = 0
global brcm2_ue_count
brcm2_ue_count = 0
global brcm2_fatal_count
brcm2_fatal_count = 0
global marv1_ce_count
marv1_ce_count = 0
global marv1_ue_count
marv1_ue_count = 0
global marv1_fatal_count
marv1_fatal_count = 0
global marv2_ce_count
marv2_ce_count = 0
global marv2_ue_count
marv2_ue_count = 0
global marv2_fatal_count
marv2_fatal_count = 0
global eth10_ce_count
eth10_ce_count = 0
global eth10_ue_count
eth10_ue_count = 0
global eth11_ce_count
eth11_ce_count = 0
global eth11_ue_count
eth11_ue_count = 0
global eth12_ce_count
eth12_ce_count = 0
global eth12_ue_count
eth12_ue_count = 0
global eth13_ce_count
eth13_ce_count = 0
global eth13_ue_count
eth13_ue_count = 0
global eth20_ce_count
eth20_ce_count = 0
global eth20_ue_count
eth20_ue_count = 0
global eth21_ce_count
eth21_ce_count = 0
global eth21_ue_count
eth21_ue_count = 0
global eth22_ce_count
eth22_ce_count = 0
global eth22_ue_count
eth22_ue_count = 0
global eth23_ce_count
eth23_ce_count = 0
global eth23_ue_count
eth23_ue_count = 0
global sata_dimm_ce_error
sata_dimm_ce_error = 0
global sata_dimm_ue_error
sata_dimm_ue_error = 0


def do_init_syslog():
    global syslog_socket
    syslog_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    return

def local(arg):
    ret = subprocess.Popen('%s' %(arg), shell=True,
                            stdout=subprocess.PIPE).stdout.read()
    ret = ret[:(len(ret) - 1)]
    return ret

def do_re_log(arg):
    message = '<162>%s' %(arg)
    local('/usr/bin/logger -s -p user.alert %s' %(arg))
    local('echo %s > /dev/kmsg' %(arg))
    time.sleep(1)
    syslog_socket.sendto(message, (syslog_server, syslog_port))
    return

def do_trigger_fpc_restart(arg):
    message = '<161>%s' %(arg)
    local('/usr/bin/logger -s -p user.emerg %s' %(arg))
    local('echo %s > /dev/kmsg' %(arg))
    time.sleep(1)
    syslog_socket.sendto(message, (syslog_server, syslog_port))
    time.sleep(1)
    os.system('/sbin/reboot')
    return

def do_trigger_fpc_halt(arg):
    message = '<161>%s' %(arg)
    local('/usr/bin/logger -s -p user.emerg %s' %(arg))
    local('echo %s > /dev/kmsg' %(arg))
    time.sleep(1)
    syslog_socket.sendto(message, (syslog_server, syslog_port))
    time.sleep(1)
    os.system('/sbin/halt')
    return

def do_smbus_monitor():
    global i2c_fail_count

    dev_id = local('i2cget -y 0 %s %s b' %(I2C_CLK_BUF_ADDR, CLK_BUF_DEV_ADDR))
    ven_id = local('i2cget -y 0 %s %s b' %(I2C_CLK_BUF_ADDR, CLK_BUF_VEN_ADDR))
    if dev_id != I2C_CLK_BUF_DEV_ID or ven_id != I2C_CLK_BUF_VEN_ID:
        do_re_log('SMBUS: Not able to read from clock buffer')
        i2c_fail_count += 1
        if i2c_fail_count > I2C_ERROR_THRESHOLD:
            do_trigger_fpc_restart('SMBUS: Not able to read from clock buffer')
    return

def do_pci_get_aer_ce_error_count(dev_id):
    count = int(local('dmesg | grep "^%s" | grep "Corrected" | wc -l'))
    return count

def do_pci_get_aer_ue_error_count(dev_id):
    count = int(local('dmesg | grep "^%s" | grep "Uncorrected (Non-Fatal)" | wc -l'))
    return count

def do_pci_get_aer_fatal_error_count(dev_id):
    count = int(local('dmesg | grep "^%s" | grep "Uncorrected (Fatal)" | wc -l'))
    return count

def do_dimm_ecc_monitor():
    global prv_ce_count
    global prv_ue_count
    global cur_time_diff

    cur_0_ce_count = int(local('cat /sys/devices/system/edac/mc/mc0/ce_count'))
    cur_1_ce_count = int(local('cat /sys/devices/system/edac/mc/mc1/ce_count'))
    cur_ce_count = cur_0_ce_count + cur_1_ce_count
    cur_0_ue_count = int(local('cat /sys/devices/system/edac/mc/mc0/ue_count'))
    cur_1_ue_count = int(local('cat /sys/devices/system/edac/mc/mc1/ue_count'))
    cur_ue_count = cur_0_ue_count + cur_1_ue_count
    # For correctable errors, just log it.
    if cur_ce_count > prv_ce_count:
        if cur_ce_count > DIMM_ECC_CE_THRESHOLD:
            do_re_log('DIMM ECC: Correctable ECC errors detected, num errors = %s'
                        %(cur_ce_count))
        if (cur_ce_count - prv_ce_count) > (DIMM_ECC_CE_PER_HR_THRESHOLD):
            do_re_log('DIMM ECC: Correctable ECC errors detected, num errors = %s'
                        %(cur_ce_count))
            do_trigger_fpc_restart('DIMM ECC: correctable ECC errors exceeded\
                                    threshold, restarting fpc')
    cur_time_diff += 1
    if cur_time_diff/HZ > ONEHR:
        cur_time_diff = 0
        prv_ce_count = cur_ce_count

    if cur_ue_count > prv_ue_count:
        do_re_log('DIMM ECC: UnCorrectable ECC errors detected, num errors = %s'
                    %(cur_ue_count))
        if cur_ue_count > DIMM_ECC_UE_THRESHOLD:
            do_trigger_fpc_restart('DIMM ECC: Uncorrectable ECC errors exceeded\
                                    threshold, restarting fpc')
    prv_ce_count = cur_ce_count
    prv_ue_count = cur_ue_count
    return

def do_sata_dimm_monitor():
    global sata_dimm_ce_error
    global sata_dimm_ue_error

    total_err = local('smartctl --all /dev/sda | grep -w Raw_Read_Error_Rate | \
                        awk \'{print $10}\'')
    if total_err != '0':
        ce_count1 = local('smartctl --all /dev/sda | \
                        grep -w Hardware_ECC_Recovered | awk \'{print $10}\'')
        ce_count2 = local('smartctl --all /dev/sda | \
                        grep -w Soft_Read_Error_Rate | awk \'{print $10}\'')
        ce_count3 = local('smartctl --all /dev/sda | \
                        grep -w Soft_ECC_Correction | awk \'{print $10}\'')
        ce_count = int(ce_count1) + int(ce_count2) + int(ce_count3)
        if ce_count != sata_dimm_ce_error:
            do_re_log('SATA DIMM: Correctable errors detected, num errors = %s'
                        %(ce_count))
            sata_dimm_ce_error = ce_count

        ue_count1 = local('smartctl --all /dev/sda | \
                        grep -w Offline_Uncorrectable | awk \'{print $10}\'')
        ue_count2 = local('smartctl --all /dev/sda | \
                        grep -w Reported_Uncorrect | awk \'{print $10}\'')
        ue_count = int(ue_count1) + int(ue_count2) + int(ue_count3)
        if ue_count != sata_dimm_ue_error:
            do_re_log('SATA DIMM: Correctable errors detected, num errors = %s'
                        %(ue_count))
            sata_dimm_ue_error = ue_count

    return

def do_temp_monitor():
    cpu_temp_str = local('sensors | grep "Physical id 0"|awk \'{print $4}\'')
    cpu_temp_0 = float(cpu_temp_str)
    cpu_temp_str = local('sensors | grep "Physical id 1"|awk \'{print $4}\'')
    cpu_temp_1 = float(cpu_temp_str)
    if cpu_temp_0 > CPU_TEMP_LIMIT or cpu_temp_1 > CPU_TEMP_LIMIT:
        do_re_log('CPU TEMP: CPU temperature exceeded critical limit %f %f'
                    %(cpu_temp_0, cpu_temp_1))
        do_trigger_fpc_restart('CPU TEMP: CPU temperature exceeded limit, \
                                restarting fpc')
    return

def do_bios_monitor():
    bios_av = local('dmidecode -t bios | grep Vendor  | \
                    grep "American Megatrends Inc." | wc -l')
    if bios_av == '0':
        do_re_log('BIOS Monitor: Bios not found')
    return


def do_cave_creek_monitor():
    global cc_ce_count
    global cc_ue_count
    global cc_fatal_count

    count = do_pci_get_aer_ce_error_count(CAVE_CREEK_DEV)
    if count != cc_ce_count:
        cc_ce_count = count
        do_re_log('Cavecreek PCIE: Correctable errors detected - %s'
                    %(cc_ce_count))
    count = do_pci_get_aer_ue_error_count(CAVE_CREEK_DEV)
    if count != cc_ue_count:
        cc_ue_count = count
        do_re_log('Cavecreek PCIE: Uncorrectable non-fatal errors detected - %s'
                    %(cc_ue_count))
        if cc_ue_count > PCI_NON_FATAL_ERROR_THRESHOLD:
            do_trigger_fpc_restart('Cavecreek PCIE: Uncorrectable non-fatal \
                                    errors detected')
    count = do_pci_get_aer_fatal_error_count(CAVE_CREEK_DEV)
    if count != cc_fatal_count:
        cc_fatal_count = count
        do_re_log('Cavecreek PCIE: Uncorrectable fatal errors detected - %s'
                    %(cc_fatal_count))
        do_trigger_fpc_restart('Cavecreek PCIE: Uncorrectable fatal \
                                    errors detected')
    return

def do_brcm_pcie_monitor():
    global brcm1_ce_count
    global brcm1_ue_count
    global brcm1_fatal_count
    global brcm2_ce_count
    global brcm2_ue_count
    global brcm2_fatal_count

    count = do_pci_get_aer_ce_error_count(BRCM1_DEV)
    if count != brcm1_ce_count:
        brcm1_ce_count = count
        do_re_log('Broadcom PCIE: Correctable errors detected - %s'
                    %(brcm1_ce_count))
    count = do_pci_get_aer_ue_error_count(BRCM1_DEV)
    if count != brcm1_ue_count:
        brcm1_ue_count = count
        do_re_log('Broadcom PCIE: Uncorrectable non-fatal errors detected - %s'
                    %(brcm1_ue_count))
        if brcm1_ue_count > PCI_NON_FATAL_ERROR_THRESHOLD:
            do_trigger_fpc_restart('Broadcom PCIE: Uncorrectable non-fatal \
                                    errors detected')
    count = do_pci_get_aer_fatal_error_count(BRCM1_DEV)
    if count != brcm1_fatal_count:
        brcm1_fatal_count = count
        do_re_log('Broadcom PCIE: Uncorrectable fatal errors detected - %s'
                    %(brcm1_fatal_count))
        do_trigger_fpc_restart('Broadcom PCIE: Uncorrectable fatal \
                                    errors detected')

    count = do_pci_get_aer_ce_error_count(BRCM2_DEV)
    if count != brcm2_ce_count:
        brcm2_ce_count = count
        do_re_log('Broadcom PCIE: Correctable errors detected - %s'
                    %(brcm2_ce_count))
    count = do_pci_get_aer_ue_error_count(BRCM2_DEV)
    if count != brcm2_ue_count:
        brcm2_ue_count = count
        do_re_log('Broadcom PCIE: Uncorrectable non-fatal errors detected - %s'
                    %(brcm2_ue_count))
        if brcm2_ue_count > PCI_NON_FATAL_ERROR_THRESHOLD:
            do_trigger_fpc_restart('Broadcom PCIE: Uncorrectable non-fatal \
                                    errors detected')
    count = do_pci_get_aer_fatal_error_count(BRCM2_DEV)
    if count != brcm2_fatal_count:
        brcm2_fatal_count = count
        do_re_log('Broadcom PCIE: Uncorrectable fatal errors detected - %s'
                    %(brcm2_fatal_count))
        do_trigger_fpc_restart('Broadcom PCIE: Uncorrectable fatal \
                                    errors detected')
    return

def do_brcm_link_monitor():
    global eth10_ce_count
    global eth10_ue_count
    global eth11_ce_count
    global eth11_ue_count
    global eth12_ce_count
    global eth12_ue_count
    global eth13_ce_count
    global eth13_ue_count
    global eth20_ce_count
    global eth20_ue_count
    global eth21_ce_count
    global eth21_ue_count
    global eth22_ce_count
    global eth22_ue_count
    global eth23_ce_count
    global eth23_ue_count

    count = int(local('ethtool -S eth10 | grep -w recoverable_errors | \
                        awk \'{print $2}\''))
    if count != eth10_ce_count:
        do_re_log('Broadcom ethernet eth10 : Correctable errors detected - %s'
                    %(eth10_ce_count))
        eth10_ce_count = count

    count = int(local('ethtool -S eth10 | grep -w unrecoverable_errors | \
                        awk \'{print $2}\''))
    if count != eth10_ue_count:
        do_re_log('Broadcom ethernet eth10 : Uncorrectable errors detected - %s'
                    %(eth10_ue_count))
        eth10_ue_count = count

    count = int(local('ethtool -S eth11 | grep -w recoverable_errors | \
                        awk \'{print $2}\''))
    if count != eth11_ce_count:
        do_re_log('Broadcom ethernet eth11 : Correctable errors detected - %s'
                    %(eth11_ce_count))
        eth11_ce_count = count

    count = int(local('ethtool -S eth11 | grep -w unrecoverable_errors | \
                        awk \'{print $2}\''))
    if count != eth11_ue_count:
        do_re_log('Broadcom ethernet eth11 : Uncorrectable errors detected - %s'
                    %(eth11_ue_count))
        eth11_ue_count = count

    count = int(local('ethtool -S eth12 | grep -w recoverable_errors | \
                        awk \'{print $2}\''))
    if count != eth12_ce_count:
        do_re_log('Broadcom ethernet eth12 : Correctable errors detected - %s'
                    %(eth12_ce_count))
        eth12_ce_count = count

    count = int(local('ethtool -S eth12 | grep -w unrecoverable_errors | \
                        awk \'{print $2}\''))
    if count != eth12_ue_count:
        do_re_log('Broadcom ethernet eth12 : Uncorrectable errors detected - %s'
                    %(eth12_ue_count))
        eth12_ue_count = count

    count = int(local('ethtool -S eth13 | grep -w recoverable_errors | \
                        awk \'{print $2}\''))
    if count != eth13_ce_count:
        do_re_log('Broadcom ethernet eth13 : Correctable errors detected - %s'
                    %(eth13_ce_count))
        eth13_ce_count = count

    count = int(local('ethtool -S eth13 | grep -w unrecoverable_errors | \
                        awk \'{print $2}\''))
    if count != eth13_ue_count:
        do_re_log('Broadcom ethernet eth13 : Uncorrectable errors detected - %s'
                    %(eth13_ue_count))
        eth13_ue_count = count

    count = int(local('ethtool -S eth20 | grep -w recoverable_errors | \
                        awk \'{print $2}\''))
    if count != eth20_ce_count:
        do_re_log('Broadcom ethernet eth20 : Correctable errors detected - %s'
                    %(eth20_ce_count))
        eth20_ce_count = count

    count = int(local('ethtool -S eth20 | grep -w unrecoverable_errors | \
                        awk \'{print $2}\''))
    if count != eth20_ue_count:
        do_re_log('Broadcom ethernet eth20 : Uncorrectable errors detected - %s'
                    %(eth20_ue_count))
        eth20_ue_count = count

    count = int(local('ethtool -S eth21 | grep -w recoverable_errors | \
                        awk \'{print $2}\''))
    if count != eth21_ce_count:
        do_re_log('Broadcom ethernet eth21 : Correctable errors detected - %s'
                    %(eth21_ce_count))
        eth21_ce_count = count

    count = int(local('ethtool -S eth21 | grep -w unrecoverable_errors | \
                        awk \'{print $2}\''))
    if count != eth21_ue_count:
        do_re_log('Broadcom ethernet eth21 : Uncorrectable errors detected - %s'
                    %(eth21_ue_count))
        eth21_ue_count = count

    count = int(local('ethtool -S eth22 | grep -w recoverable_errors | \
                        awk \'{print $2}\''))
    if count != eth22_ce_count:
        do_re_log('Broadcom ethernet eth22 : Correctable errors detected - %s'
                    %(eth22_ce_count))
        eth22_ce_count = count

    count = int(local('ethtool -S eth22 | grep -w unrecoverable_errors | \
                        awk \'{print $2}\''))
    if count != eth22_ue_count:
        do_re_log('Broadcom ethernet eth22 : Uncorrectable errors detected - %s'
                    %(eth22_ue_count))
        eth22_ue_count = count

    count = int(local('ethtool -S eth23 | grep -w recoverable_errors | \
                        awk \'{print $2}\''))
    if count != eth23_ce_count:
        do_re_log('Broadcom ethernet eth23 : Correctable errors detected - %s'
                    %(eth23_ce_count))
        eth23_ce_count = count

    count = int(local('ethtool -S eth23 | grep -w unrecoverable_errors | \
                        awk \'{print $2}\''))
    if count != eth23_ue_count:
        do_re_log('Broadcom ethernet eth23 : Uncorrectable errors detected - %s'
                    %(eth23_ue_count))
        eth23_ue_count = count

    return

def do_marvel_pcie_monitor():
    global marv1_ce_count
    global marv1_ue_count
    global marv1_fatal_count
    global marv2_ce_count
    global marv2_ue_count
    global marv2_fatal_count

    count = do_pci_get_aer_ce_error_count(MARVEL1_DEV)
    if count != marv1_ce_count:
        marv1_ce_count = count
        do_re_log('Marvell PCIE: Correctable errors detected - %s'
                    %(marv1_ce_count))
    count = do_pci_get_aer_ue_error_count(MARVEL1_DEV)
    if count != marv1_ue_count:
        marv1_ue_count = count
        do_re_log('Marvell PCIE: Uncorrectable non-fatal errors detected - %s'
                    %(marv1_ue_count))
        if marv1_ue_count > PCI_NON_FATAL_ERROR_THRESHOLD:
            do_trigger_fpc_restart('Marvell PCIE: Uncorrectable non-fatal \
                                    errors detected')
    count = do_pci_get_aer_fatal_error_count(MARVEL1_DEV)
    if count != marv1_fatal_count:
        marv1_fatal_count = count
        do_re_log('Marvell PCIE: Uncorrectable fatal errors detected - %s'
                    %(marv1_fatal_count))
        do_trigger_fpc_restart('Marvell PCIE: Uncorrectable fatal \
                                    errors detected')

    count = do_pci_get_aer_ce_error_count(MARVEL2_DEV)
    if count != marv2_ce_count:
        marv2_ce_count = count
        do_re_log('Marvell PCIE: Correctable errors detected - %s'
                    %(marv2_ce_count))
    count = do_pci_get_aer_ue_error_count(MARVEL2_DEV)
    if count != marv2_ue_count:
        marv2_ue_count = count
        do_re_log('Marvell PCIE: Uncorrectable non-fatal errors detected - %s'
                    %(marv2_ue_count))
        if marv2_ue_count > PCI_NON_FATAL_ERROR_THRESHOLD:
            do_trigger_fpc_restart('Marvell PCIE: Uncorrectable non-fatal \
                                    errors detected')
    count = do_pci_get_aer_fatal_error_count(MARVEL2_DEV)
    if count != marv2_fatal_count:
        marv2_fatal_count = count
        do_re_log('Marvell PCIE: Uncorrectable fatal errors detected - %s'
                    %(marv2_fatal_count))
        do_trigger_fpc_restart('Marvell PCIE: Uncorrectable fatal \
                                    errors detected')
    return

def pacifica_resiliency():
    do_init_syslog()
    while True:
        do_smbus_monitor()
        do_dimm_ecc_monitor()
        do_sata_dimm_monitor()
        do_temp_monitor()
        do_bios_monitor()
        do_cave_creek_monitor()
        do_brcm_pcie_monitor()
        do_marvel_pcie_monitor()
        do_brcm_link_monitor()
        time.sleep(HZ)
    return

pacifica_resiliency()
