#!/usr/bin/python3

# auto restart the public wifi with VPN connection

import subprocess
import time
import os
import psutil


INTERFACE = "wlan0"
TARGET_SSID = "xfinitywifi"

def is_process_running(process_name):
    for proc in psutil.process_iter(['name']):
        if process_name.lower() in proc.info['name'].lower():
            return True
    return False

def kill_process(process_name):
    subprocess.run(['pkill', '-f', process_name])

def killall():
    if is_process_running('dhclient'):
        kill_process('dhclient')
    if is_process_running('openvpn'):
        kill_process('openvpn')




subprocess.run(['ifconfig', INTERFACE, 'up'])
while True:
    configured = False
    associated = True
    got_address = False


# query the interface
    result = subprocess.run(['iwconfig', INTERFACE], capture_output=True, text=True)

    for line in result.stdout.split('\n'):
        if 'ESSID' in line and TARGET_SSID in line:
            configured = True
        if 'Access Point:' in line and 'Not-Associated' in line:
            associated = False


    if not configured:
        print(f"Dropped SSID")
        killall()

        print(f"Setting SSID to {TARGET_SSID}")
        subprocess.run(['iwconfig', INTERFACE, 'essid', TARGET_SSID])
        time.sleep(1)
        continue

# waiting for it to associate
    if not associated:
        print(f"Not associated")
        killall()
        time.sleep(1)
        continue

# start dhclient
    if not is_process_running('dhclient'):
        subprocess.run(['dhclient', INTERFACE])
        print("Started dhclient")
        time.sleep(1)
        continue

# test for address
    result = subprocess.run(['ifconfig', INTERFACE], capture_output=True, text=True)
    for line in result.stdout.split('\n'):
        if 'netmask' in line and 'inet' in line:
            got_address = True

# waiting for IP address
    if not got_address:
        print("Waiting for address")
        time.sleep(1)
        continue

# start the VPN
    if not is_process_running('openvpn'):
        print("Reached openvpn");
# throttled login attempts
        if False:
            subprocess.run(['openvpn',
                '--script-security', '2', 
                '--config', '/root/node-us-142.protonvpn.net.udp.ovpn',
                '--auth-user-pass', '/root/userpass', 
                '--up-delay', 
                '--up', '/root/sinon', 
                '--down-pre',
                '--down', '/root/sinoff'])
            print("Started openvpn")

# idle
    time.sleep(1)




