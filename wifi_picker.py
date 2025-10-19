#!/usr/bin/python3




# pick the channel with the least usage
# store it in a file that you can append to hostapd.conf to select the channel


# 2.4Ghz:
# wifi_picker.py wlan0 2

# 5Ghz:
# wifi_picker.py wlan0 5

import subprocess
import os
import re
import sys

OUTPUT = '/tmp/the_channel'

def channel_to_index(channel):
    for i in range(len(channels)):
        if channels[i] == channel:
            return i
    return -1


if len(sys.argv) < 3:
    print("Usage: wifi_picker <interface> <band>")

INTERFACE = sys.argv[1]
band = int(sys.argv[2])

if band == 2:
    channels = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]

if band == 5:
    # only channels supported by cheap phone
    channels = [36, 40, 44, 48 ]

print("channels: %s" % channels)

usage = [0] * len(channels)

result = subprocess.check_output(['iwlist', INTERFACE, 'scan'])

for line in result.decode('utf-8').split('\n'):
    if 'Frequency' in line:
        #print("Got line %s" % line)
        words = line.split(' ')
        for i in range(len(words)):
            if 'Channel' in words[i]:
                channel = int(re.sub(r'[()]', '', words[i + 1]))
                #print("Got channel %d" % channel)
                index = channel_to_index(channel)
# ignore channels not in our band
                if index >= 0:
                    usage[channel_to_index(channel)] += 1
                break

# DEBUG multiple open channels with equal spacing:
#usage = [ 0, 11, 0, 1, 0 ]

print("Channel usage: %s" % usage)


# get least used channel
least_index = -1
least_number = 99
for i in range(len(channels)):
    if usage[i] < least_number:
        least_number = usage[i];
        least_index = i

#print("least_index=%d least_number=%d" % (least_index, least_number))




# get the spacing of each occurance of least_number from its neighbors
spacing = [ -1 ] * len(channels)
max_space = -1
max_space_index = -1
for i in range(len(channels)):
    if usage[i] == least_number:
# get space to the left
        left_space = 0
        j = i
        if j == 0:
# on the minimum channel, ignore left space
            left_space = 99
        else:
            while j > 0 and usage[j] == least_number:
                left_space += 1
                j -= 1
# get space to the right
        right_space = 0
        j = i
        if j == len(channels) - 1:
# on the maximum channel, ignore right space
            right_space = 99
        else:
            while j < len(channels) and usage[j] == least_number:
                right_space += 1
                j += 1
# get least of left & right spacing
        spacing[i] = min([left_space, right_space])
        if spacing[i] > max_space:
            max_space = spacing[i]
            max_space_index = i

print("spacing: %s" % spacing)





# if multiple channels have the minimum usage & maximum spacing, 
# pick the least number of neighbors
neighbors = [ 0 ] * len(channels)
min_neighbors = 99
min_neighbors_index = -1
for i in range(len(spacing)):
    if spacing[i] == max_space:
        if i > 0:
            neighbors[i] += usage[i - 1]
        if i < len(channels) - 1:
            neighbors[i] += usage[i + 1]
        if neighbors[i] < min_neighbors:
            min_neighbors = neighbors[i]
            min_neighbors_index = i

print("neighbors: %s" % neighbors)


print("optimal channel: %d" % channels[min_neighbors_index])
print("writing to %s" % OUTPUT)
try:
    with open(OUTPUT, 'w') as f:
        f.write("channel=%d\n" % channels[min_neighbors_index])
except IOError as e:
    print(f"Error writing to file: {e}")

