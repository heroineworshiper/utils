#!/usr/bin/python



#
# Tube
# Copyright (C) 2021 Adam Williams <broadcast at earthling dot net>
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
        


EXE = 'youtube-dl'
VCODEC = 'avc1'
# use VP9 only for highest resolution, since it's not seekable
VCODEC_H = 'vp9@'
MAX_RES = 65535

# requested quality
want_res = 1920
want_samplerate = 48000
want_video = True
movies = []

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
    return int(text[0:end])

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
    print('tube l <URL>   download 854x480 quality');
    print('tube m <URL>   download 1920x1080 quality');
    print('tube h <URL>   download best quality');
    print('tube a <URL>   download audio only at 44.1khz');
    exit()

quality = sys.argv[1]
url = sys.argv[2]

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




    for line in lines:
        words = line.split()
        #print('words=%d' % len(words));
    # got a format line
        if len(words) >= 5 and \
            isnumeric(words[0]) and \
            (words[1] == 'mp4' or words[1] == 'm4a' or words[1] == 'webm'):
            #print('format=%s' % line);




    # got audio
            if words[2] == 'audio' and words[3] == 'only':
                note = 5
                if words[4] == 'DASH':
                    note = 6


                new_bitrate = toint(words[note])
    # dumb samplerate estimation
                new_samplerate = 48000
                if words_contain(words[note:], "44100"):
                    new_samplerate = 44100
    # closer to desired format
                #print('audio_code=%d new_samplerate=%d new_bitrate=%d' % \
                #    (toint(words[0]), new_samplerate, new_bitrate))
                if movie.audio_code < 0 or \
                    (movie.got_samplerate != want_samplerate and new_bitrate > movie.audio_bitrate) or \
                    abs(new_samplerate - want_samplerate) <= abs(movie.got_samplerate - want_samplerate) or \
                    (new_samplerate == movie.got_samplerate and new_bitrate > movie.audio_bitrate):
                    movie.audio_code = toint(words[0])
                    movie.audio_bitrate = new_bitrate
                    movie.got_samplerate = new_samplerate
                    #print('audio_code=%d bitrate=%d' % (movie.audio_code, movie.audio_bitrate))
    # got video
            else:
                note = 3
                if words[3] == 'DASH':
                    note = 5
# supported codec based on the desired resolution
                if want_video and (words_contain(words[note:], VCODEC) or \
                    (want_res == MAX_RES and words_contain(words[note:], VCODEC_H))):
# get video resolution
                    new_res = toint(words[2])
                    new_bitrate = toint(words[note])
                    if movie.video_code < 0 or \
                        (new_res <= want_res and new_res >= movie.got_res):
                        movie.video_code = toint(words[0])
                        movie.video_bitrate = new_bitrate
                        movie.got_res = new_res
                        movie.res_text = words[2]
                        #print('video_code=%d video_bitrate=%d res_text=%s' % \
                        #    (movie.video_code, movie.video_bitrate, movie.res_text))


    if movie.audio_code < 0:
        print("No audio format found");
        exit()

    if want_video and movie.video_code < 0:
        print("No video format found");
        exit()

    print("Using audio_code=%d video_code=%d res=%s samplerate=%d" % 
        (movie.audio_code, movie.video_code, movie.res_text, movie.got_samplerate))

# download them
for movie in movies:
    if want_video:
        command = [EXE, 
            '-f',
            str(movie.video_code) + '+' + str(movie.audio_code), 
            '--no-mtime',
            movie.url]
    else:
        command = [EXE, 
            '-f',
            str(movie.audio_code), 
            '--no-mtime',
            movie.url]

    print(' '.join(command))
    subprocess.call(command)


