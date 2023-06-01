#!/usr/bin/python


# what file in the search path contains the 1st argument's contents
# searches for files in a certain directory, matching a regular expression

import os
import stat
import sys
import subprocess




def md5sum(file):
    statbuf = os.stat(file)
    if not stat.S_ISDIR(statbuf.st_mode):
        string = subprocess.check_output(['md5sum', file])
        substrings = string.split(' ')
        return substrings[0]
    else:
        return ''

def test_file(file):
    dst_md5 = md5sum(file)
#    print('%s %s' % (file, dst_md5))

    if dst_md5 == src_md5:
        print('%s matches' % file)







#print('test_file %s\n' % file)






argc = len(sys.argv)
files = []

if argc < 2:
    print('Usage %s <file> <search path> [filename pattern]' % sys.argv[0])
    print('Example: %s lens.stl . \'*.stl\'             # search the *.stl files' % sys.argv[0])
    print('Example: %s lens.stl .                     # search all files' % sys.argv[0])
    sys.exit()


src_file = sys.argv[1]
search_path = sys.argv[2]
pattern = ''
if argc >= 4:
    pattern = sys.argv[3]
src_md5 = md5sum(src_file)
#print('SOURCE: %s %s' % (src_file, src_md5))

if pattern != '':
    text = subprocess.check_output(['find', search_path, '-name', pattern])
else:
    text = subprocess.check_output(['find', search_path])
lines = text.split('\n')

for file in lines:
#    print(file)
    if file != '':
        test_file(file)












