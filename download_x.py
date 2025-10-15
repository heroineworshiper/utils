#!/usr/bin/python

# hacky way to download X broadcasts

# step 1: download the .m3u8 playlist by loading the broadcast URL in a browser
# play the highest resolution, which it currently does automatically

# step 2: inspect the network log for a .m3u8 file & download it.

# step 2: inspect the network log for a .ts file

# step 3: copy the URL of the .ts file
# This URL determines the resolution of the output

# step 4: run this program
# download_x [URL with the .ts file] [.m3u8 file]

# step 5: cat the .ts files into 1





import os
import sys
import fileinput
import subprocess


#outpath = sys.argv[1]
ts_url = sys.argv[1]
playlist_path = sys.argv[2]

print 'Reading', playlist_path
file = open(playlist_path, 'r')

playlist_files = []
while True:
    line = file.readline()
# end of file
    if line == "":
        break
    line = line.strip()
# ignore comment
    if line.startswith('#'):
        continue
# playlist file
    playlist_files.append(line)

print("Got %d chunks" % len(playlist_files))

#for i in range(len(playlist_files)):
#    print("playlist: %s" % playlist_files[i])

# extract auth token
i = ts_url.rfind('/')
auth_url = ts_url[0:i]

print("Auth URL=%s" % auth_url)

for i in range(len(playlist_files)):
    new_url = auth_url + '/' + playlist_files[i]
    print("Getting %s" % new_url)
    text = subprocess.check_output(["wget", "--no-check-certificate", new_url])
    
