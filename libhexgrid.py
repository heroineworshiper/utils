# hex grid library


import FreeCAD as App
import FreeCADGui as Gui
import Sketcher
import Part
import math


def toRad(angle):
    return angle * math.pi * 2.0 / 360.0

# make a closed polygon
def makePoly(sketch, coords, closeIt = True):
    prevLine = None
    for i in range(0, len(coords) - 1):
        sketch.addGeometry(Part.LineSegment(coords[i],
            coords[i + 1]),
            False)

    if closeIt:
        sketch.addGeometry(Part.LineSegment(coords[len(coords) - 1],
            coords[0]),
            False)


def makeHex(sketch, 
    corners, 
    x1, 
    x2, 
    x3, 
    y1, 
    y2, 
    y3,
    y4):
    coords = [
        App.Vector(x2, y1, 0.0) + corners[0],
        App.Vector(x1, y2, 0.0) + corners[2],
        App.Vector(x1, y3, 0.0) - corners[2],
        App.Vector(x2, y4, 0.0) - corners[0],
        App.Vector(x3, y3, 0.0) - corners[1],
        App.Vector(x3, y2, 0.0) + corners[1]]
    makePoly(sketch, coords)



def makeGrid(w, h, x_slices, y_slices, x_border, y_border, line_w):
# fudge the x borders
    x_borders[0] = x_borders[0] - .6
    x_borders[1] = x_borders[1] - .6
    slice_h = (h - y_borders[0] - y_borders[1]) / y_slices
    slice_w = (w - x_borders[0] - x_borders[1]) / x_slices


    sketch = doc.addObject('Sketcher::SketchObject','gridsketch')
    sketch.Placement = App.Placement(
        App.Vector(0.0, 0.0, 0.0),
        App.Rotation(App.Vector(0, 0, 1), 0.0))

# offsets of 3 hex corners around each intersection
    corners = []
    for i in range(3):
        corner = App.Vector( \
            line_w * math.cos(toRad(i * 120.0 + 90.0)), \
            line_w * math.sin(toRad(i * 120.0 + 90.0)), \
            0.0)
        corners.append(corner)

# middle rows
    for y in range(y_slices):
        for x in range(x_slices):
            y1 = y_borders[0] + y * slice_h
            y2 = y_borders[0] + (y + .25) * slice_h
            y3 = y_borders[0] + (y + .75) * slice_h
            y4 = y_borders[0] + (y + 1) * slice_h
            x1 = x_borders[0] + x * slice_w
            x2 = x_borders[0] + (x + .5) * slice_w
            x3 = x_borders[0] + (x + 1) * slice_w

            makeHex(sketch,
                corners,
                x1,
                x2,
                x3,
                y1,
                y2,
                y3,
                y4)

















