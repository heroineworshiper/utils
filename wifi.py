#!/usr/bin/python



# start up wpa_supplicant, wait for a connection, then start up dhclient

# example /etc/wpa_supplicant/wpa_supplicant.conf:
#ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev
#update_config=1
#country=US
#
#network={
#       ssid="OVERPRICED APT"
#       psk="the password"
#}


import os
import sys
import subprocess
import time

DHCLIENT = 'dhclient'
#DEVICE = 'wlan0'
DEVICE = 'wlx000272deb076'

subprocess.call(['killall', '-9', 'wpa_supplicant'])
subprocess.call(['killall', '-9', DHCLIENT])
time.sleep(1)



p = subprocess.Popen(['wpa_supplicant', 
        '-i' + DEVICE, 
        '-c/etc/wpa_supplicant/wpa_supplicant.conf'],
    stdout=subprocess.PIPE)


while True:
    line = p.stdout.readline().decode('utf-8')
    print('Got: ', line)
    if "CTRL-EVENT-CONNECTED" in line:
        # start DHCP
        subprocess.call(['killall', '-9', DHCLIENT])
        time.sleep(1)
        subprocess.call([DHCLIENT, '-v', DEVICE])
        print("Ran dhclient.  DNS:")
        subprocess.call(['cat', '/etc/resolv.conf'])






