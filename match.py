#!/usr/bin/python


# what file in the search path contains the 1st argument's contents

import os
import stat
import sys
import subprocess






def test_file(file):
    statbuf = os.stat(file)
    
    if stat.S_ISDIR(statbuf.st_mode):
#        print(file)
        text = subprocess.check_output(['find', file])
        lines = text.split('\n')
        for line in lines:
            if line != file and line != '':
                test_file(line)
    else:
        dst_md5 = subprocess.check_output(['md5sum', file])

        if dst_md5 == src_md5:
            print('%s matches\n' % file)







#print('test_file %s\n' % file)






argc = len(sys.argv)
files = []

if argc < 3:
    print('Usage %s <file> <search path>' % sys.argv[0])
    print('Example: %s lens.stl *' % sys.argv[0])
    exit

for i in range(1, argc):
    if i == 1:
        src_md5 = subprocess.check_output(['md5sum', sys.argv[i]])
    else:
        files.append(sys.argv[i])

for file in files:
    test_file(file)












