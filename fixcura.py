#!/usr/bin/python

# This adds timelapse codes,
# retracts before each picture, 
# heats the bed & nozzle
# simultaneously, & 
# prints the filename on the screen.  The timelapse
# parameters have to be set in the python file.  It overwrites the gcode
# file. 


# Usage: ./fixcura.py test.gcode


import os
import sys
import fileinput

# operations to perform
DO_TIMELAPSE = False
# probe the bed before printing
DO_LEVELING = True
# change nozzle & bed temperature after the 1st layer
DO_TEMP_CHANGE = False


# temperature changes
# this promotes layer adhesion without destroying the bed
NOZZLE_TEMP2 = 250
# this lowers the bed temperature without causing shrinkage
BED_TEMP2 = 0


# timelapse config
# mm
PARK_X = 250
PARK_Y = 190
# ms
PARK_DELAY = 1000
# retraction speed
RETRACT_SPEED = 2700
RETRACT_DIST = 5



filename = sys.argv[1]
print 'Reading', filename

# read it
file = open(filename, 'r')

# values extracted from file
bedTemp = -1
nozzleTemp = -1
gotTimelapse = False
gotLayer = False
currentLine = 0
# temperature settings
tempLine1 = -1 
tempLine2 = -1
# G28 line must be done before bed leveling
g28Line = -1
# Existing G29 was found
gotLeveling = False
# end of last layer
lastLine = -1
# last head position before timelapse
lastG0 = ""
totalLayers = 0

# output file string
dst = []


def printTimelapse(retract, lastG0):
    dst.append(';TimeLapse Begin\n')
    if retract:
        dst.append('G91 ; Relative movement for retraction.\n')
        dst.append('G1 E-%d F%d ;Retract\n' % (RETRACT_DIST, RETRACT_SPEED))
        dst.append('G90 ; Absolute\n')
    dst.append('G1 F9000 X%d Y%d ;Park print head\n' % (PARK_X, PARK_Y))
    dst.append('M400 ;Wait for moves to finish\n')
    dst.append('M240 ;Snap Photo\n')
    dst.append('G4 P%d ;Wait for camera\n' % PARK_DELAY)
    dst.append(lastG0)
    if retract:
        dst.append('G91 ; Relative movement for retraction.\n')
        dst.append('G1 E%d F%d ; Unretract\n' % (RETRACT_DIST, RETRACT_SPEED))
        dst.append('G90 ; Absolute\n')
    dst.append(';TimeLapse End\n')


def printTemps():
    if nozzleTemp > 0:
        dst.append('M104 S%d ; set nozzle temp\n' % nozzleTemp)
        dst.append('M105\n')
    if bedTemp > 0:
        dst.append('M140 S%d ; set bed temp\n' % bedTemp)
        dst.append('M105\n')
# overwrite 'bed heating'
    dst.append('M117 %s\n' % strippedName)

# wait for bed before leveling
    if bedTemp > 0:
        dst.append('M190 S%d ; wait for bed temp\n' % bedTemp)

# bed leveling code
    if DO_LEVELING:
        dst.append('G28 ; Home all axes\n')
        dst.append('G29 ; bed leveling\n')

# wait for nozzle before printing
    if nozzleTemp > 0:
        dst.append('M109 S%d ; wait for nozzle temp\n' % nozzleTemp)


# 2nd temperature
def printTemps2():
    dst.append('M104 S%d ; set nozzle temp2\n' % NOZZLE_TEMP2)
    dst.append('M105\n')
    dst.append('M140 S%d ; set bed temp2\n' % BED_TEMP2)
    dst.append('M105\n')

# overwrite 'bed heating'
    dst.append('M117 %s\n' % strippedName)

# don't wait for temps before printing because it'll cook the filament





# read through the file & track certain lines
while True:
    line = file.readline()
    if line == "":
        break

    if 'timelapse' in line.lower():
        gotTimelapse = True

    if 'LAYER:' in line:
        gotLayer = True
        totalLayers += 1

# get temperature settings
    if not gotLayer:
        if 'M140' in line:
            bedTemp = int(line.split()[1][1:])
            if tempLine1 < 0:
                tempLine1 = currentLine

        if 'M104' in line:
            nozzleTemp = int(line.split()[1][1:])
            if tempLine1 < 0:
                tempLine1 = currentLine

        if 'G28' in line:
            if g28Line < 0:
                g28Line = currentLine

        if 'G29' in line:
            gotLeveling = True

# end of temperature settings
        if tempLine1 >= 0 and \
            tempLine2 < 0 and \
            'M82' in line:
            tempLine2 = currentLine

# get end of print
    if lastLine < 0 and \
        gotLayer and \
        any(word in line for word in ['M140', 'M104']):
            #print 'end of print', currentLine
            lastLine = currentLine

    currentLine += 1



#print 'gotLayer=', gotLayer

if lastLine < 0:
    print 'Couldn\'t find last line.  Giving up & going to a movie.'
    exit()

if gotTimelapse and DO_TIMELAPSE:
    print 'Already has timelapse code.  Not adding timelapse.'
    DO_TIMELAPSE = False

#if gotLeveling and DO_LEVELING:
#    print 'Already has leveling code.  Not adding leveling.'
#    DO_LEVELING = False


file.seek(0)

filename2 = filename.split('/')
strippedName = filename2[len(filename2) - 1]


print "Operations to be performed:"
print "TIMELAPSE=" + str(DO_TIMELAPSE)
print '    PARK_X=', PARK_X
print '    PARK_Y=', PARK_Y
print '    PARK_DELAY=', PARK_DELAY, 'ms'
print '    RETRACT_DIST=', RETRACT_DIST
print "BED LEVELING=" + str(DO_LEVELING)
print "CHANGE TEMPS=" + str(DO_TEMP_CHANGE)
print '    2nd temperature: bed:', BED_TEMP2, ' nozzle/:', NOZZLE_TEMP2
print '-----------------------------------------------'
print 'displayed name:', strippedName
print 'replacing temperature lines:', tempLine1, ' -', tempLine2
print 'starting temps bed:', bedTemp, ' nozzle:', nozzleTemp
print 'last line:', lastLine
print 'total layers:', totalLayers


value = raw_input('Press ENTER to proceed or ctrl-C to quit.')
if value != '':
    print('Giving up & going to a movie');
    exit()


# copy up to temperature lines
currentLine = 0
while currentLine < tempLine1:
    line = file.readline()
    if line == "":
        print 'unexpected end of file 1'
        exit()
    dst.append(line)
    currentLine += 1

# print new temperature lines & bed leveling
printTemps()

# skip temperature lines
while currentLine < tempLine2:
    line = file.readline()
    if line == "":
        print 'unexpected end of file 2'
        exit()
    currentLine += 1

# display the filename
dst.append('M117 %s\n' % strippedName)


# search for LAYER: lines
layer_number = 0
while True:
    line = file.readline()
    skip = False
    if line == "":
        break

# skip G28's after bed leveling
    if DO_LEVELING and line.startswith('G28'):
        skip = True

    if line.startswith('G0'):
        lastG0 = line

    if DO_TEMP_CHANGE and 'LAYER:' in line:
        if layer_number == 1:
            printTemps2()

    if DO_TIMELAPSE:
        if 'LAYER:' in line:
            if layer_number > 0:
                printTimelapse(True, lastG0)
        elif currentLine == lastLine:
            printTimelapse(False, "")

    if 'LAYER:' in line:
        layer_number += 1


# pass line through
    if not skip:
        dst.append(line)

    currentLine += 1





# write it
file = open(filename, 'w')
for i in dst:
    file.write(i)


print 'Wrote file'
