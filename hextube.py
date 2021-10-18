# create hex cylinder in Blender


import os
import bpy, bmesh
import math
from mathutils import *


# length of the center bits out of the total hexagon height
CENTER_FRACTION = .5
TOP_FRACTION = (1.0 - CENTER_FRACTION) / 2
BOTTOM_FRACTION = (1.0 - CENTER_FRACTION) / 2

# bottom radius
R0 = 70.0
# top radius
R1 = 35.0
H = 72.0

THICKNESS = 5.0

def toRad(angle):
    return angle * math.pi / 180.0

def toDeg(angle):
    return angle * 180.0 / math.pi

def polarToXYZ(r, a, z):
    return Vector([r * math.cos(a), r * math.sin(a), z])

def polarToXYZ2(r, a, z):
    return (r * math.cos(a), r * math.sin(a), z)

def XYZToPolar(x, y, z):
    r = math.hypot(x, y)
    a = math.atan2(y, x)
    z = z
    return (r, a, z)

def deselect():
    #bpy.ops.object.select_all(action='DESELECT')
    for i in bpy.data.objects:
        i.select_set(False)

# select & activate the object
def selectByName(name):
    deselect()
    for i in bpy.context.scene.objects:
        #print(i.name)
        if i.name == name:
            i.select_set(True)
            bpy.context.view_layer.objects.active = i
            return i
    print("selectByName didn't find %s" % (name))


def findObject(name):
    for i in bpy.context.scene.objects:
        if i.name == name:
            return i
    return None

def deleteObjectNamed(name):
    deselect()
    obj = findObject(name)
    if obj != None:
        obj.select = True
        obj.hide = False
        bpy.context.scene.objects.active = obj
        bpy.ops.object.delete()
    else:
        print("deleteObjectNamed: %s not found" % (name))


# create an empty object.  Must be in object mode.
def createObject(name):
#    deleteObjectNamed(name)
    mesh = bpy.data.meshes.new("mesh")
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)  # put the object into the scene (link)
    bpy.context.view_layer.objects.active = obj  # set as the active object in the scene
    obj.select_set(True)  # select object
    mesh = obj.data
    bm = bmesh.new()
    return (obj, bm, mesh)



# search the verts in the model or create a new one
def getVert(bm, xyz):
    for i in bm.verts:
        if abs(i.co.x - xyz.x) < 0.001 and \
            abs(i.co.y - xyz.y) < 0.001 and \
            abs(i.co.z - xyz.z) < 0.001:
            return i
    return bm.verts.new(xyz)

# make quad on a cylinder where x is the linear distance on the circumference
def polarQuad(bm, r, topLeft, topRight, bottomRight, bottomLeft):
    c = math.pi * 2 * r
    
    
    a1 = topLeft.x / c * 2 * math.pi
    a3 = bottomRight.x / c * 2 * math.pi

# horizontal rectangle
    if topRight is None:
        bm.faces.new([getVert(bm, polarToXYZ(r, a1, topLeft.y)),
            getVert(bm, polarToXYZ(r, a3, topLeft.y)),
            getVert(bm, polarToXYZ(r, a3, bottomRight.y)),
            getVert(bm, polarToXYZ(r, a1, bottomRight.y))])
    else:
# diagonal rectangle
        a2 = topRight.x / c * 2 * math.pi
        a4 = bottomLeft.x / c * 2 * math.pi
        bm.faces.new([getVert(bm, polarToXYZ(r, a1, topLeft.y)),
            getVert(bm, polarToXYZ(r, a2, topRight.y)),
            getVert(bm, polarToXYZ(r, a3, bottomRight.y)),
            getVert(bm, polarToXYZ(r, a4, bottomLeft.y))])






def polarTri(bm, r, center, corners):
    c = math.pi * 2 * r
    verts = []
    for i in range(3):
        verts.append(getVert(bm, polarToXYZ(r,
            (center.x + corners[i].x) / c * 2 * math.pi,
            (center.y + corners[i].y))))
    bm.faces.new(verts)







def conify(bm):
    for i in bm.verts:
        r, a, z = XYZToPolar(i.co.x, i.co.y, i.co.z)
        r = R0 + (z / H) * (R1 - R0)
        i.co.x = r * math.cos(a)
        i.co.y = r * math.sin(a)
        i.co.z = z
    

# solidify it
def solidify(dst):
    dst.select_set(True)
    bpy.context.view_layer.objects.active = dst
    solidify = dst.modifiers.new(type="SOLIDIFY", name="solidify")
    solidify.thickness = -THICKNESS
    bpy.ops.object.modifier_apply(modifier="solidify")


# enter edit mode for the object & get the mesh
    bpy.ops.object.mode_set(mode = 'EDIT')
    bpy.ops.mesh.select_all(action='DESELECT')
    bm = bmesh.new()
    bm = bmesh.from_edit_mesh(dst.data)
# shift the inner wall
    a = math.atan2(R0 - R1, H)
    #print("a=%f offset=%f" % (toDeg(a), THICKNESS * math.sin(a)))
    z_offset = THICKNESS * math.sin(a)
    for i in bm.verts:
        r, a, z = XYZToPolar(i.co.x, i.co.y, i.co.z)
        middle_r = R0 + (z / H) * (R1 - R0) - THICKNESS / 2
        if r < middle_r:
            z += z_offset
            # reset the thickness
            r = R0 + (z / H) * (R1 - R0) - THICKNESS
            x, y, z = polarToXYZ2(r, a, z)
            i.co.x = x
            i.co.y = y
            i.co.z = z
    bm.free()
    bpy.ops.object.mode_set(mode = 'OBJECT')





def makeGrid(r, h, x_slices, y_slices, y_borders, line_w):
    dst, bm, mesh = createObject("tube")

    w = math.pi * 2.0 * r
    slice_h = (h - y_borders[0] - y_borders[1]) / y_slices
    slice_w = w / x_slices

# radius of the triangle with faces equal to line_w
    tri_r = line_w / 2 / math.sin(toRad(60.0))

# offsets of 3 hex corners around each intersection
# 0 - top center
# 1 - bottom right
# 2 - bottom left
    corners1 = []
    for i in range(3):
        corner = Vector([ \
            tri_r * math.cos(toRad(-i * 120.0 + 90.0)), \
            tri_r * math.sin(toRad(-i * 120.0 + 90.0)), \
            0.0])
        print("corner1 %d: %f %f %f" % (i, corner.x, corner.y, corner.z))
        corners1.append(corner)

# 0 - bottom center
# 1 - top left
# 2 - top right
    corners2 = []
    for i in range(3):
        corner = Vector([ \
            tri_r * math.cos(toRad(-i * 120.0 - 90.0)), \
            tri_r * math.sin(toRad(-i * 120.0 - 90.0)), \
            0.0])
        print("corner2 %d: %f %f %f" % (i, corner.x, corner.y, corner.z))
        corners2.append(corner)

    for x in range(0, x_slices):

# bottom row
        center_y = -CENTER_FRACTION / 2
        y2 = y_borders[0]
        y3 = y_borders[0] + (center_y + CENTER_FRACTION) * slice_h
        y4 = y_borders[0] + (center_y + 1) * slice_h
        x1 = x * slice_w
        x2 = (x + .5) * slice_w
        x3 = (x + 1) * slice_w

# bottom
        polarQuad(bm, 
            r, 
            Vector([x1 + corners2[1].x, y_borders[0], 0]), 
            None,
            Vector([x1 + corners2[2].x, 0, 0]),
            None)
        polarQuad(bm, 
            r, 
            Vector([x1 + corners2[2].x, y_borders[0], 0]), 
            None,
            Vector([x3 + corners2[1].x, 0, 0]),
            None)
# bottom left rect
        polarQuad(bm, 
            r, 
            Vector([x1, y3, 0]) + corners1[2], 
            None,
            Vector([x1, y2, 0]) + Vector([corners2[2].x, 0.0, 0.0]),
            None)
# top left tri
        polarTri(bm, r, Vector([x1, y3, 0]), corners1)
# top left rect
        polarQuad(bm, r, 
            Vector([x2, y4, 0]) + corners2[1], 
            Vector([x2, y4, 0]) + corners2[0],
            Vector([x1, y3, 0]) + corners1[1],
            Vector([x1, y3, 0]) + corners1[0])
# top tri
        polarTri(bm, r, Vector([x2, y4, 0]), corners2)
# top right rect
        polarQuad(bm, r, 
            Vector([x2, y4, 0]) + corners2[0], 
            Vector([x2, y4, 0]) + corners2[2],
            Vector([x3, y3, 0]) + corners1[0],
            Vector([x3, y3, 0]) + corners1[2])

# top row
        x_offset = 0.0
        if (y_slices % 2) > 0:
            x_offset += slice_w / 2
        center_y = (y_slices - CENTER_FRACTION / 2)
        y2 = y_borders[0] + center_y * slice_h
        y3 = h - y_borders[1]
        x1 = x * slice_w + x_offset
        x2 = (x + 1) * slice_w + x_offset

# top
        polarQuad(bm, 
            r, 
            Vector([x1 + corners1[2].x, h, 0]), 
            None,
            Vector([x1 + corners1[1].x, h - y_borders[1], 0]),
            None)
        polarQuad(bm, 
            r, 
            Vector([x1 + corners1[1].x, h, 0]), 
            None,
            Vector([x2 + corners1[2].x, h - y_borders[1], 0]),
            None)

# top left
        polarQuad(bm, 
            r, 
            Vector([x1, y3, 0]) + Vector([corners1[2].x, 0, 0]),
            None,
            Vector([x1, y2, 0]) + corners2[2],
            None)

# middle rows
        for y in range(1, y_slices):
            x_offset = 0.0
# shift odd row
            if (y % 2) > 0:
                x_offset += slice_w / 2

            center_y = (y - CENTER_FRACTION / 2)
            y1 = y_borders[0] + (center_y - BOTTOM_FRACTION) * slice_h
            y2 = y_borders[0] + center_y * slice_h
            y3 = y_borders[0] + (center_y + CENTER_FRACTION) * slice_h
            y4 = y_borders[0] + (center_y + 1) * slice_h
            x1 = x * slice_w + x_offset
            x2 = (x + .5) * slice_w + x_offset
            x3 = (x + 1) * slice_w + x_offset

# bottom left rect
            polarQuad(bm, 
                r, 
                Vector([x1, y3, 0]) + corners1[2], 
                None,
                Vector([x1, y2, 0]) + corners2[2],
                None)
# top left tri
            polarTri(bm, r, Vector([x1, y3, 0]), corners1)
# top left rect
            polarQuad(bm, r, 
                Vector([x2, y4, 0]) + corners2[1], 
                Vector([x2, y4, 0]) + corners2[0],
                Vector([x1, y3, 0]) + corners1[1],
                Vector([x1, y3, 0]) + corners1[0])
# top tri
            polarTri(bm, r, Vector([x2, y4, 0]), corners2)
# top right rect
            polarQuad(bm, r, 
                Vector([x2, y4, 0]) + corners2[0], 
                Vector([x2, y4, 0]) + corners2[2],
                Vector([x3, y3, 0]) + corners1[0],
                Vector([x3, y3, 0]) + corners1[2])


# deform it into a cone
    if True:
        conify(bm)



    bm.faces.ensure_lookup_table()
    bm.to_mesh(mesh)
    bm.free()  # always do this when finished

    if True:
        solidify(dst)

# make conic section for speaker outline
    dst, bm, mesh = createObject("tube outline")
    for x in range(0, x_slices):
        x1 = x * slice_w
        x2 = (x + 1) * slice_w
# outside
        polarQuad(bm, 
            r,
            Vector([x1 + corners1[2].x, h, 0]), 
            None, 
            Vector([x1 + corners2[2].x, 0, 0]),
            None)
        polarQuad(bm, 
            r,
            Vector([x1 + corners1[1].x, h, 0]), 
            None, 
            Vector([x2 + corners2[1].x, 0, 0]),
            None)

    conify(bm)

    bm.faces.ensure_lookup_table()
    bm.to_mesh(mesh)
    bm.free()  # always do this when finished

    solidify(dst)






makeGrid(R0, H, 46, 10, [ 2.0, 2.0 ], 2.0)






