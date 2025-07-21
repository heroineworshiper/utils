#!/usr/bin/python3
#
# ADB wrapper
# Copyright (C) 2023-2025 Adam Williams <broadcast at earthling dot net>
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
# 
#

# ADBD doesn't work when interfaced by a pipe, so we have to
# run 'adb devices' every time to check for multiple devices, then run the adb command
# on the console.

import os
import select
import signal
import sys
import subprocess
import tty, termios
import fcntl


ADB_EXEC = 'adb'
dry_run = False
do_wait = True

do_del = False
do_devices = False
do_mkdir = False
do_push = False
src_files = []
dst_path = ""

do_get = False
do_shell = False
global break_counter
break_counter = 0
do_list = False
command_start = -1
device_name = ""


def text_to_mode(text):
    global do_push
    global do_get
    global do_shell
    global do_devices
    global do_list
    global do_del
    global do_mkdir

# strip leading path if we got argv[0]
    #print("text=%s" % text)
    if '/' in text:
        offset = text.rindex('/')
        text = text[offset + 1:]

    if text == "push" or text == "pU":
        do_push = True
    elif text == "pull" or text == "get" or text == "gE":
        do_get = True
    elif text == "shell" or text == "sH":
        do_shell = True
    elif text == "del" or text == "dE":
        do_del = True
    elif text == "devices":
        do_devices = True
    elif text == "list" or text == "lI":
        do_list = True
    elif text == "mkdir":
        do_mkdir = True
    return have_mode()

def have_mode():
    return do_push or \
        do_get or \
        do_shell or \
        do_devices or \
        do_list or \
        do_del or \
        do_mkdir

def print_devices():
    subprocess.call([ ADB_EXEC, "devices" ])

def init_console():
    global stdout_settings
    global stdin_settings
#    signal.signal(signal.SIGINT, handler)
    stdout_settings = termios.tcgetattr(sys.stdout.fileno())
    stdin_settings = termios.tcgetattr(sys.stdin.fileno())
    tty.setraw(sys.stdout.fileno())
    tty.setraw(sys.stdin.fileno())
    

def reset_console():
    global stdout_settings
    global stdin_settings
    termios.tcsetattr(sys.stdout.fileno(), termios.TCSADRAIN, stdout_settings)
    termios.tcsetattr(sys.stdin.fileno(), termios.TCSADRAIN, stdin_settings)
    print("reset_console()")
    

def getch():
    if os.name == 'nt':  # Windows
        import msvcrt
        return msvcrt.getch().decode()
    else:  # Linux/Mac
        fd = sys.stdin.fileno()
        old_settings2 = termios.tcgetattr(fd)
        try:
            tty.setraw(fd)
            ch = sys.stdin.read(1)
            #print("ch=%d" % ord(ch))
        finally:
            termios.tcsetattr(fd, termios.TCSADRAIN, old_settings2)

        return ord(ch)



def get_device():
    global device_name
    got_it = False
    text = subprocess.check_output([ADB_EXEC, "devices"])

# expand the device name
    if len(device_name) > 0:
        lines = text.split(b'\n')
        for line in lines:
            #print("%s" % line);
            if len(line) > 0 and \
                not line.startswith(b"List of") and \
                line.startswith(device_name.encode('utf-8')):
                    device_name = line.split(b'\t')[0]
                    print("Using device %s" % device_name)
                    got_it = True
                    break
        if not got_it:
            print("Device %s not found." % device_name)
# assume a valid name was passed & it's not connected yet
    else:
# prompt for the device
        lines = text.strip().splitlines()
        device_ids = []
        for line in lines:
            if not "list of devices" in line.decode("utf-8").lower():
                device_ids.append(line.decode("utf-8").split('\t')[0])
        #print(device_ids)

        if len(device_ids) == 0:
            print("No devices found")
            exit()
        elif len(device_ids) == 1:
            return

        print("Cursor over a device & hit enter or ctrl-c to quit:\r")
        current_item = 0
        first = True
        ch_state = 0

        while True:
            if not first:
                # cursor up
                print("\033[%dA\r" % (len(device_ids) + 1))
            first = False
            for i in range(len(device_ids)):
                if i == current_item:
                    # highlighted item
                    print("\033[07m%s\033[0m\r" % device_ids[i])
                else:
                    # unhighlighted item
                    print("%s\r" % device_ids[i])

            c = getch()
    # ctrl-c
            if c == 3:
                print("Giving up & going to a movie\r")
#                reset_console()
                exit()
    # escape
            if c == 27:
                ch_state = 1
    # enter
            elif c == 13:
                device_name = device_ids[current_item].encode("utf-8")
                break
            elif ch_state == 1:
    # escape code
                if c == 91:
                    ch_state = 2
                else:
                    ch_state = 0
            elif ch_state == 2:
                ch_state = 0
    # escape code
                if c == 65:
                    current_item -= 1
                    if current_item < 0:
                        current_item = len(device_ids) - 1
                elif c == 66:
                    current_item += 1
                    if current_item > len(device_ids) - 1:
                        current_item = 0
    #        print("%d" % c)
        




# handle multiple device error by forwarding stderr & testing it
def do_it(command):
    command2 = [ ADB_EXEC ]
# insert the device option
    if len(device_name) > 0:
        command2.append("-s")
        command2.append(device_name.decode("utf-8"))
# insert the wait option.  must come after -s
    if do_wait:
        command2.append("wait-for-usb-device")
    for i in command:
        command2.append(i)

    print(" ".join(command2) + "\r")
    try:
        subprocess.run(command2)
    except OSError as e:
#        reset_console()
        exit()

    




def push_file(src, dst, is_top):
    print("push_file src=%s dst=%s cwd=%s\r" % (src, dst, os.getcwd()))


    if os.path.isdir(src):
# for the top level, change the current working directory to 1 above
# the src
        if is_top:
            top_dir = os.getcwd()
            src = src.rstrip('/')
            offset = src.rfind('/')
            if offset > 0:
                src2 = src[0:offset]
                print("changing to " + src2)
                os.chdir(src2)
                src = src[offset + 1:]

# make the directory
        args = []
        new_dst = dst + "/" + src
        args.append("shell")
        args.append("mkdir")
        args.append("-p")
        args.append(new_dst)
        if not dry_run:
            do_it(args)

# descend into directory
        print("descending into %s" % src);
        subdir = os.listdir(src)
        subdir = sorted(subdir)
        subdir2 = []
        for i in subdir:
            full_name = src + '/' + i
            subdir2.append(full_name)
#            print("%s" % full_name)
            push_file(full_name, new_dst, False)

# return to the starting point
        if is_top:
            print("returning to %s" % top_dir);
            os.chdir(top_dir)
# not a dir
    else:
        args = []
        args.append("push")
        args.append(src)
        args.append(dst)
        if not dry_run:
            do_it(args)
    return


def handler(signum, frame):
    print("\nYou pressed Ctrl+C!")




# ==========================================================================
# begin here
#print("argv[0]=%s" % sys.argv[0])

if len(sys.argv) < 2 and not text_to_mode(sys.argv[0]):
    print("Wrapper for common ADB functions")
    print("The program may be run as adbwrap with the command as an argument or")
    print("    pU, gE, lI, sH, dE")
    print("    Aliases don't work.  Only symbolically linking the alternate name to adbwrap works")
    print("Example: shell [-s device hint] [command]")
    print("Example: %s [-s device hint] shell [command]" % sys.argv[0])
    print("List devices: %s devices" % sys.argv[0])
    print("    It automatically does this if it errors out, so dE means delete.")
    print("Push mode: push [-n] [-s device hint] [files] [dst directory]")
    print("    Push multiple files to a directory")
    print("    -n dry run")
    print("    device hint may be the minimum number of characters to match a device ID")
    print("Example: push -s Z -n satriani spear tori /sdcard/Music")
    print("Example: %s -s Z push satriani spear tori /sdcard/Music" % sys.argv[0])
    exit()

text_to_mode(sys.argv[0])

first = True
skip = False
for i in range(1, len(sys.argv)):
# convert a device name hint to a device name
    if skip:
        skip = False
        pass
    elif sys.argv[i] == '-n':
        dry_run = True
    elif sys.argv[i] == '-s':
        i += 1
        skip = True
        device_name = sys.argv[i]
    elif first and not have_mode():
        text_to_mode(sys.argv[i])
        #print("Mode %s do_push=%s do_get=%s do_shell=%s" % \
        #    (sys.argv[i], do_push, do_get, do_shell))
        first = False
    elif do_push:
        if i < len(sys.argv) - 1:
            src_files.append(sys.argv[i])
        else:
            dst_path = sys.argv[i]
    elif do_shell or do_list:
        command_start = i
        break

if do_devices:
    print_devices()

if do_list or do_del or do_mkdir or do_push or do_get or do_shell:
    get_device()

if do_get:
    pass

if do_push:
# trap ctrl-C
#    init_console()
    #for i in range(len(src_files)):
    #    print("src=%s " % src_files[i])
    #print("dst_path=%s" % dst_path)
# this calls do_it later
    for i in range(len(src_files)):
        push_file(src_files[i], dst_path, True)
#    reset_console()
    exit()

if do_shell:
# trap ctrl-C
#    init_console()
    args = []
    args.append("shell")
    if command_start > 0:
        args += sys.argv[command_start:]
    if not dry_run: 
        do_it(args)
#    reset_console()
    exit()

if do_list:
#    init_console()
    args = []
    args.append("shell")
    args.append("ls")
    args.append("-al")
    if command_start > 0:
        args += sys.argv[command_start:]
    if not dry_run:
        do_it(args)
#    reset_console()
    exit()

