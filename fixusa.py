#!/usr/bin/python

# Hacker for prusa slicer
# This stretches the layers to compensate for Ender's squished Z
# Drops the 1st layer to trick it into using variable line width for layer 1.

# Usage: ./fixusa.py test.gcode


import os
import sys
import fileinput

STRETCH_LAYERS = True
# amount to add to layer height.  Must be multiple of .04 on the Ender 3
LAYER_H = 0.04
# number of layers to change, not including the dropped layer
# change 3 to get container panels to be 5mm
LAYER_COUNT = 3

# drop a layer.  
# Enable a raft & enable this to get it to vary the line width in layer 1
DROP_LAYER1 = True



print "Operations to be performed:"
print "STRETCH LAYERS=" + str(STRETCH_LAYERS)
print '    DIFFERENCE=', LAYER_H, ' NUMBER OF LAYERS=', LAYER_COUNT

print "DROP LAYER 1=" + str(DROP_LAYER1)

value = raw_input('Press ENTER to proceed or ctrl-C to quit.')
if value != '':
    print('Giving up & going to a movie');
    exit()
print('\n');



filename = sys.argv[1]
print 'Reading', filename

# read it
file = open(filename, 'r')


# output file string
dst = []




# layer stretching
changed_layers = 0
origZ0 = 0
origZ1 = 0
newZ0 = 0
newZ1 = 0

GET_LAYER_CHANGE = 0
GET_G1 = 1
stretch_state = GET_LAYER_CHANGE

GET_LAYER_CHANGE1 = 0
GET_LAYER_CHANGE2 = 1
DROP_DONE = 2
drop_state = GET_LAYER_CHANGE1

while True:
    line = file.readline()
    skip = False
    if line == "":
        break

    if DROP_LAYER1:
# drop everything from the 1st LAYER_CHANGE to the 2nd LAYER_CHANGE
        if drop_state == GET_LAYER_CHANGE1:
            if ';LAYER_CHANGE' in line:
                drop_state = GET_LAYER_CHANGE2
                skip = True
        elif drop_state == GET_LAYER_CHANGE2:
            if ';LAYER_CHANGE' in line:
                drop_state = DROP_DONE
                dst.append("; Dropped 1st layer\n\n")
            else:
# catch Z of the 1st layer
                if 'G1 Z' in line:
                    words = line.split(' ')
                    zWord = words[1]
                    if zWord.startswith('Z'):
                        origH = origZ0 = float(zWord[1:])
                        print("Dropped layer height=%.2f" % 
                                (origH))
                    else:
                        print("DROP_LAYER1: Didn't get a Z after a G1")
                skip = True


    if STRETCH_LAYERS:
        if not DROP_LAYER1 or drop_state == DROP_DONE:
            if stretch_state == GET_LAYER_CHANGE:
                if ';LAYER_CHANGE' in line:
                    stretch_state = GET_G1
            elif stretch_state == GET_G1:
                if 'G1 Z' in line:
                    stretch_state = GET_LAYER_CHANGE
                    words = line.split(' ')
                    zWord = words[1]
                    if zWord.startswith('Z'):
                        origZ1 = float(zWord[1:])
                        origH = origZ1 - origZ0

                        if changed_layers < LAYER_COUNT:
                            newZ1 += origH + LAYER_H
                            print("Layer %d height=%.2f new height=%.2f" % 
                                (changed_layers, origH, newZ1 - newZ0))
                        else:
                            newZ1 += origH

                        origZ0 = origZ1
                        newZ0 = newZ1
                        line = "G1 Z%.02f " % newZ1
                        line += " ".join(words[2:])
                        changed_layers += 1
                    else:
                        print("STRETCH_LAYERS: Didn't get a Z after a G1")

# pass line through
    if not skip:
        dst.append(line)





# write it
file = open(filename, 'w')
for i in dst:
    file.write(i)


print 'Wrote file'
