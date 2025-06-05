#!/usr/bin/python

# simple GPX to KML converter from grok


import xml.etree.ElementTree as ET
import sys

def gpx_to_kml(gpx_file_path, kml_file_path):
    # Define GPX namespace
    namespaces = {'gpx': 'http://www.topografix.com/GPX/1/1'}
    
    # Parse the GPX file
    tree = ET.parse(gpx_file_path)
    root = tree.getroot()
    
    # Extract coordinates from track points
    coordinates = []
    for trkpt in root.findall('.//gpx:trkpt', namespaces):
        lat = trkpt.get('lat')
        lon = trkpt.get('lon')
        ele = trkpt.find('gpx:ele', namespaces)
        elevation = ele.text if ele is not None else '0'  # Default to 0 if no elevation
        coordinates.append((lon, lat, elevation))  # KML uses lon,lat,ele order
    
    # Create KML content
    coord_text = ""
    for lon, lat, ele in coordinates:
        coord_text += lon + "," + lat + "," + ele + " "
    kml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" \
        "<kml xmlns=\"http://www.opengis.net/kml/2.2\">" \
        "  <Document>" \
        "    <name>GPX to KML Path</name>" \
        "    <Style id=\"pathStyle\">" \
        "      <LineStyle>" \
        "        <color>ff0000ff</color>  <!-- Red path, ARGB format -->" \
        "        <width>4</width>" \
        "      </LineStyle>" \
        "    </Style>" \
        "    <Placemark>" \
        "      <name>Path</name>" \
        "      <styleUrl>#pathStyle</styleUrl>" \
        "      <LineString>" \
        "        <tessellate>1</tessellate>" \
        "        <coordinates>" \
        "          %s" \
        "        </coordinates>" \
        "      </LineString>" \
        "    </Placemark>" \
        "  </Document>" \
        "</kml>" % coord_text

    # Write to KML file
    with open(kml_file_path, 'w') as f:
        f.write(kml)




gpx_file = sys.argv[1]
kml_file = sys.argv[2]

gpx_to_kml(gpx_file, kml_file)





