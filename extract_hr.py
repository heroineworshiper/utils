#!/usr/bin/python
# simple heartrate data extractor from grok
# extract heartrate from GPX file


import xml.etree.ElementTree as ET
import sys

def extract_heart_rates(gpx_file_path):
    # Define the namespace for Garmin's TrackPointExtension
    namespaces = {
        'gpx': 'http://www.topografix.com/GPX/1/1',
        'gpxtpx': 'http://www.garmin.com/xmlschemas/TrackPointExtension/v1'
    }
    
    # Parse the GPX file
    tree = ET.parse(gpx_file_path)
    root = tree.getroot()
    
    # List to store heart rate values
    heart_rates = []
    
    # Find all track points
    for trkpt in root.findall('.//gpx:trkpt', namespaces):
        # Look for the heart rate in the extensions
        hr = trkpt.find('.//gpxtpx:hr', namespaces)
        if hr is not None:
            try:
                heart_rates.append(int(hr.text))  # Convert to integer
            except ValueError:
                print("Invalid heart rate value: {%s}" % hr.text)
                continue
    
    return heart_rates


filename = sys.argv[1]
print 'Reading', filename
hr_list = extract_heart_rates(filename)
for i in hr_list:
    print(i)



