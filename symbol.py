#!/usr/bin/python


# what library in the current directory defines or uses the symbol?
# python edition


import os
import sys
import subprocess





def ends_with(string, text):
    i = len(string) - 1
    while i >= 0:
        if string[i] == ' ' or string[i] == '\n':
            i -= 1
        else:
            break
    got_it = True
    if i >= len(text) - 1:
        i -= len(text) - 1
#        print('ends_with string=%s i=%d len=%d' % (string, i, len(string)))
        for j in range(len(text)):
            if string[i + j] != text[j]:
                got_it = False
                break
    else:
        got_it = False
#    print('ends_with string=%s got_it=%s' % (string, got_it))

    return got_it



def dump_lines(file, filename, got_lines):
    if len(got_lines) > 0 and filename:
        if file:
            print(file + ':')
        print('    ' + filename)
        for i in got_lines:
            print('        ' + i)






argc = len(sys.argv)

if argc < 3:
    print('Usage %s [-u] <.o or .so files> <symbol>' % sys.argv[0])
    print('Example: %s *.so timer_create         what file defines the symbol' % sys.argv[0])
    print('Example: %s -u *.so timer_create      include files which use the symbol' % sys.argv[0])
    exit

want_usage = False
files = []
symbol = ''
EXE = 'nm'

# search the object files
for i in range(1, len(sys.argv)):
    if sys.argv[i] == '-u':
        want_usage = True
    elif i < len(sys.argv) - 1:
        files.append(sys.argv[i])
    else:
        symbol = sys.argv[i]


for file in files:
    text = subprocess.check_output([EXE, file])
    lines = text.split('\n')
    filename = ''
    got_lines = []
    file_ = file
    for line in lines:
        if ends_with(line, '.o:'):
            dump_lines(file_, filename, got_lines)
            if len(got_lines) > 0:
                file_ = ''
            filename = line
            got_lines = []

        if symbol in line:
            words = line.split()
            word_end = len(words)
            #print('%s' % words)
            if (want_usage and words[word_end - 2].lower() == 'u') or \
                words[word_end - 2].lower() == 't':
                got_lines.append(words[word_end - 2] + ' ' + words[word_end - 1])

    dump_lines(file_, filename, got_lines)










