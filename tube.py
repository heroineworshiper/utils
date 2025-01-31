#!/usr/bin/python



#
# Tube
# Copyright (C) 2021-2025 Adam Williams <broadcast at earthling dot net>
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

# Wrapper for youtube-dl which automates the format selection

import os
import sys
import subprocess

class Movie:
    def __init__(self, url):
        self.url = url
        self.title = ""
        self.audio_code = -1
        self.video_code = -1
        self.audio_bitrate = -1
        self.video_bitrate = -1
        self.got_res = -1
        self.got_samplerate = -1
        self.res_text = ""
        


#EXE = 'youtube-dl'
EXE = '/root/yt-dlp/yt-dlp.sh'
#EXE = '/usr/local/bin/yt-dlp'
HACK1 = '--rm-cache-dir'
HACK2 = '--no-mtime'
HACK3 = '--remux-video'
HACK4 = 'mp4'
VCODEC = 'avc1'
# use VP9 only for highest resolution, since it's not seekable
VCODEC_H = 'vp9@'
MAX_RES = 65535

# requested quality
want_res = 1920
want_samplerate = 48000
want_video = True
movies = []

# read text from a given offset in a string backwards to the previous space
def getbackwards(text, start):
    if start > len(text):  # Ensure offset is within bounds
        start = len(text)
    start -= 1
# skip the trailing spaces
    while start >= 0 and text[start] == ' ':
        start -= 1
    if start >= 0 and text[start] != ' ':
        start += 1
    offset = start - 1
# get the leading space
    while offset >= 0 and text[offset] != ' ':
        offset -= 1
    return text[offset + 1:start]

def isnumeric(text):
    for c in text:
        if ord(c) < ord('0') or ord(c) > ord('9'):
            return False
    return True

# convert to integer until the 1st non numeric character
def toint(text):
    end = len(text)
    for i in range(0, len(text)):
        c = ord(text[i])
        if c < ord('0') or c > ord('9'):
            end = i
            break
    if end > 0:
        return int(text[0:end])
    else:
        return -1

# is text in the array of words?
def words_contain(array, text):
    for i in array:
        #print("words_contain %s" % i)
        if text in i:
            return True
    return False


argc = len(sys.argv)
if argc < 3:
    print('Usage: tube <lmha> <URL>')
    print('tube s <URL>   download 426x240 quality');
    print('tube l <URL>   download 854x480 quality');
    print('tube m <URL>   download 1920x1080 quality');
    print('tube h <URL>   download best quality');
    print('tube a <URL>   download audio only at 44.1khz');
    exit()

quality = sys.argv[1]
url = sys.argv[2]

if quality == 's':
    want_res = 426
if quality == 'l':
    want_res = 854
elif quality == 'm':
    want_res = 1920
elif quality == 'h':
    want_res = MAX_RES
elif quality == 'a':
    want_samplerate = 44100
    want_video = False

# extract all the movies from a playlist
if "&list=" in url:
    print("Got playlist");
    text = subprocess.check_output([EXE, '-J', url])
    words = text.split()
    for i in range(0, len(words)):
        if words[i] == "\"display_id\":":
# strip quotes & comma
            len_ = len(words[i + 1])
            code = words[i + 1][1:len_ - 2]
            movie = Movie("https://www.youtube.com/watch?v=" + code)
            movies.append(movie)
            print movie.url

else:
# single movie
    movie = Movie(url)
    movies.append(movie)


# get format codes for each movie
for movie in movies:
    movie.title = subprocess.check_output([EXE, '-e', movie.url])
    print(movie.title.strip("\n"))
    text = subprocess.check_output([EXE, '-F', movie.url])

    #print('text=\n%s' % text)

    lines = text.split('\n')

    #print('lines=%d' % len(lines));

    gotColumns = False


    for line in lines:
        if not gotColumns:
            if line.startswith('ID '):
#                print('getting columns: %s' % line);
# discover start & end of the data fields
                col1 = dict([
                    ('ID', line.find('ID ')),
                    ('EXT', line.find('EXT ')),
                    ('RESOLUTION', line.find('RESOLUTION ')),
                    ('BITRATE', line.find('TBR ')),
                    ('SAMPLERATE', line.find('ASR ')),
                    ('VCODEC', line.find('VCODEC ')),
                    ('INFO', line.find('MORE '))])
                col2 = dict([
                    ('ID', line.find('EXT ')),
                    ('EXT', line.find('RESOLUTION ')),
                    ('RESOLUTION', line.find('FPS ')),
                    ('BITRATE', line.find('PROTO ')),
                    ('SAMPLERATE', line.find('MORE ')),
                    ('VCODEC', line.find('VBR ')),
                    ('INFO', len(line))])
#                print('column1:')
#                for key, value in col1.items():
#                    print(key, ":", value)
#                print('column2:')
#                for key, value in col2.items():
#                    print(key, ":", value)
                gotColumns = True
            else:
                continue


        #words = line.split()
# extract strings for each field
        words = dict([
            ('ID', line[col1['ID']:col2['ID']].strip()),  
            ('EXT', line[col1['EXT']:col2['EXT']].strip()), 
            ('RESOLUTION', line[col1['RESOLUTION']:col2['RESOLUTION']].strip()), 
            ('BITRATE', getbackwards(line, col2['BITRATE'])),
            ('SAMPLERATE', getbackwards(line, col2['SAMPLERATE']).strip()),
            ('VCODEC', line[col1['VCODEC']:col2['VCODEC']].strip()),
            ('INFO', line[col1['INFO']:len(line)].strip())])
# strip trailing -dash
#       id = toint(words['ID'])
# 1/31/25: dash ID's are now required but presence of a valid ID is still 
# discovered by the presence of a number
        id = words['ID']

# got a format line
#        print('line=%s' % (line))
#        print('id=%s EXT=%s BITRATE=%s SAMPLERATE=%s INFO=%s' % \
#            (id, words['EXT'], words['BITRATE'], words['SAMPLERATE'], words['INFO']))
        if toint(id) >= 0 and \
            (words['EXT'] == 'mp4' or words['EXT'] == 'm4a' or words['EXT'] == 'webm'):
            #print('ID=%d resolution=%s' % (id, words['RESOLUTION']))
            #print('vcodec=%s' % words['VCODEC'])


# got audio
            if words['RESOLUTION'] == 'audio only':
                new_bitrate = toint(words['BITRATE'])
                #print('ID=%d BITRATE=%s new_bitrate=%s' % \
                #    (id, words['BITRATE'], new_bitrate))
# dumb samplerate estimation
                new_samplerate = 48000
                if words['SAMPLERATE'] == '44k':
                    new_samplerate = 44100
# closer to desired format
#                print('ID=%s best id=%s new_samplerate=%d want_samplerate=%d new_bitrate=%d best bitrate=%d' % \
#                    (id, movie.audio_code, new_samplerate, want_samplerate, new_bitrate, movie.audio_bitrate))
# this is picking the last of the multi language audio
# TODO: need to rank English in words['INFO'] as highest
                if movie.audio_code < 0 or \
                    (movie.got_samplerate != want_samplerate and new_bitrate > movie.audio_bitrate) or \
                    abs(new_samplerate - want_samplerate) <= abs(movie.got_samplerate - want_samplerate) or \
                    (new_samplerate == movie.got_samplerate and new_bitrate > movie.audio_bitrate):
                    movie.audio_code = id
                    movie.audio_bitrate = new_bitrate
                    movie.got_samplerate = new_samplerate
                    #print('audio_code=%d bitrate=%d' % \
                    #    (movie.audio_code, movie.audio_bitrate))
# got video
            else:
                #print('want_video=%s vcodec=%s VCODEC=%s' % 
                #    (want_video, words['VCODEC'], VCODEC))

# supported codec based on the desired resolution
                if want_video and (VCODEC in words['VCODEC']or \
                    want_res == MAX_RES):
                    #(want_res == MAX_RES and words_contain(words['VCODEC'], VCODEC_H))):
# get video resolution
                    new_res = toint(words['RESOLUTION'])
                    new_bitrate = toint(words['BITRATE'])
                    #print('new_res=%d new_bitrate=%d' % (new_res, new_bitrate))

                    if movie.video_code < 0 or \
                        (new_res <= want_res and new_res >= movie.got_res):
                        movie.video_code = id
                        movie.video_bitrate = new_bitrate
                        movie.got_res = new_res
                        movie.res_text = words['RESOLUTION']
                        #print('video_code=%d video_bitrate=%d res_text=%s' % \
                        #    (movie.video_code, movie.video_bitrate, movie.res_text))


    if movie.audio_code < 0:
        print("No audio format found");
        exit()

    if want_video and movie.video_code < 0:
        print("No video format found");
        exit()

    print("Using audio_code=%s video_code=%s res=%s samplerate=%d" % 
        (movie.audio_code, movie.video_code, movie.res_text, movie.got_samplerate))

# DEBUG
#exit()

# download them
for movie in movies:
    if want_video:
        command = [EXE, 
            HACK1,
            '-f',
            str(movie.video_code) + '+' + str(movie.audio_code), 
            HACK2,
#            HACK3,
#            HACK4,
            movie.url]
    else:
        command = [EXE, 
            HACK1,
            '-f',
            str(movie.audio_code), 
            HACK2,
            movie.url]

    print(' '.join(command))
    subprocess.call(command)


