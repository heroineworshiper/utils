#ifndef _3DSTUFF_H
#define _3DSTUFF_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// format of a coord in the native file
typedef struct {
	float x,y,z;
} vector_t;

// format of a triangle for editing
typedef struct
{
    vector_t coords[3];
} triangle_t;

// format of a triangle in the native file
typedef struct
{
    vector_t n;
    vector_t coords[3];
    uint16_t attr;
} __attribute__((packed)) stl_triangle_t;

#define TEXTLEN 1024
#define BUFSIZE 1024
#define HEADER "MESH-MESH-MESH-MESH-MESH-MESH-MESH-MESH-MESH-MESH-MESH-MESH-MESH-MESH-MESH-MESH\n"
FILE *out;
int countOffset = 0;
int triangleCount = 0;

double length = 0;
// radians
double planeAngle = 0.0;


double topAspect = 1.0;
double planeSlope = 0.0;
double planeIntercept = 0.0;
double cosPlane = 1.0;
double sinPlane = 0.0;

void writeTriangle(vector_t coord0, vector_t coord1, vector_t coord2);
void writeTriangle2(vector_t coord0, vector_t coord1, vector_t coord2);

void writeInt32(int x)
{
    fwrite(&x, 1, sizeof(int), out);
}

void writeFloat(float x)
{
    fwrite(&x, 1, sizeof(float), out);
}


double toRad(double angle)
{
    return angle * M_PI * 2.0 / 360.0;
}

double hypot3(double x, double y, double z)
{
    return sqrt(x * x + y * y + z * z);
}

int open_stl(char *path)
{
    if((out = fopen(path, "r")))
    {
        printf("Overwrite existing file %s? (y/n)\n", path);
        char string[TEXTLEN];
        char* _ = fgets(string, TEXTLEN, stdin);
        if(strcmp(string, "y\n"))
        {
            printf("Giving up & going to a movie.\n");
    		exit(1);
        }
        fclose(out);
    }

    if(!(out = fopen(path, "w")))
    {
        printf("open_stl %d: Couldn't open %s\n", __LINE__, path);
        exit(1);
    }

    fprintf(out, HEADER);
    countOffset = ftell(out);
    writeInt32(0);


    cosPlane = cos(planeAngle);
    sinPlane = sin(planeAngle);


    return 0;
}

stl_triangle_t* read_stl(const char *path, int *count)
{
    FILE *in;
    uint8_t buffer[BUFSIZE];
    *count = 0;
    if(!(in = fopen(path, "r")))
    {
        printf("read_stl %d: Couldn't open %s\n", __LINE__, path);
        perror("");
        return 0;
    }
    
    int _ = fread(buffer, 1, strlen(HEADER), in);
    _ = fread(count, 1, sizeof(int), in);
    stl_triangle_t *triangles = (stl_triangle_t*)calloc(sizeof(stl_triangle_t), *count);
    _ = fread(triangles, sizeof(stl_triangle_t), *count, in);
    printf("read_stl %d: %d triangles\n", __LINE__, *count);
    
    fclose(in);
    return triangles;
}

void write_stl(char *path, int count, stl_triangle_t *triangles)
{
    if((out = fopen(path, "r")))
    {
        printf("Overwrite existing file %s? (y/n)\n", path);
        char string[TEXTLEN];
        char* _ = fgets(string, TEXTLEN, stdin);
        if(strcmp(string, "y\n"))
        {
            printf("Giving up & going to a movie.\n");
    		exit(1);
        }
        fclose(out);
    }

    if(!(out = fopen(path, "w")))
    {
        printf("open_stl %d: Couldn't open %s\n", __LINE__, path);
        exit(1);
    }

    fprintf(out, HEADER);
    writeInt32(count);
    int i;
    for(i = 0; i < count; i++)
    {
        stl_triangle_t *triangle = &triangles[i];
        writeTriangle(triangle->coords[0], 
            triangle->coords[1], 
            triangle->coords[2]);
    }

//    fwrite(triangles, sizeof(stl_triangle_t), count, out);
    fclose(out);
}




void close_stl()
{
    fseek(out, countOffset, SEEK_SET);
    printf("close_stl %d: triangleCount=%d\n", __LINE__, triangleCount);
    writeInt32(triangleCount);
    fclose(out);
}

vector_t polarToXYZ(vector_t point)
{
// polar to XYZ
    double angle = point.x;
    double radius = point.y;
    double z = point.z;
    double x = radius * cos(angle);
    double y = -radius * sin(angle);
// printf("polarToXYZ %d %f %f %f -> %f %f %f\n", 
//     __LINE__, 
//     point.x,
//     point.y,
//     point.z,
//     x,
//     y,
//     z);
// 

    if(fabs(planeAngle) > 0.001)
    {
// x slope given x
// rotate top X of cylinder
        double topX = x * cosPlane;
// rotate top Z of cylinder
        double topZ = planeIntercept + x * sinPlane;
        double slope = (topX - x) / topZ;
// interpolate X
        x += z * slope;
    }

    return (vector_t){ x, y, z };
}


vector_t XYZToPolar(vector_t xyz)
{
    double aspect = 1.0;
    if(fabs(topAspect - 1.0) > 0.001)
    {
        double fraction = xyz.z / length;
        aspect = (topAspect * fraction) + (1.0 - fraction);
    }
    xyz.x = xyz.x / aspect;

// circularize XY
    double y = xyz.y;
    double x = xyz.x;
    double z = xyz.z;
    if(fabs(planeAngle) > 0.001)
    {
// brute force
        double step = fabs(x) / 2;
        double goalX = x;
        while(step > 0.0001)
//        while(step > 1)
        {
            double topX = x * cosPlane;
            double topZ = planeIntercept + x * sinPlane;
            double slope = (topX - x) / topZ;
            double testX = x + z * slope;
//printf("XYZToPolar %d testX=%f x=%f\n", __LINE__, testX, x);
            if(testX < goalX)
            {
                x += step;
            }
            else
            {
                x -= step;
            }
            
            step /= 2;
        }
    }
    
// convert XY to angle & radius
    double angle = atan2(-y, x);
    double radius = hypot(x, y);
// printf("polarToXYZ %d %f %f %f -> %f %f %f\n", 
//     __LINE__, 
//     xyz.x,
//     xyz.y,
//     xyz.z,
//     angle,
//     radius,
//     z);

    return (vector_t){ angle, radius, z };
}

vector_t addVectors(vector_t a, vector_t b)
{
	return (vector_t){ a.x + b.x, a.y + b.y, a.z + b.z };
}
 
vector_t subVectors(vector_t a, vector_t b)
{
	return (vector_t){ a.x - b.x, a.y - b.y, a.z - b.z };
}

vector_t crossProduct(vector_t a, vector_t b)
{
    vector_t result;
	result.x = a.y * b.z - a.z * b.y;
	result.y = a.z * b.x - a.x * b.z;
	result.z = a.x * b.y - a.y * b.x;
    return result;
}

double magnitude(vector_t a)
{
    return sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
}

vector_t normalize(vector_t a)
{
    double m = magnitude(a);
    return (vector_t){ a.x / m, a.y / m, a.z / m };
}

double dotProduct(vector_t a, vector_t b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

vector_t scaleVector(double l, vector_t a)
{
	return (vector_t){ l * a.x, l * a.y, l * a.z };
}

// does the line intersect the plane?
int intersect(vector_t lineVector, vector_t planeNormal)
{
    if(dotProduct(lineVector, planeNormal) == 0)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

// point where line intersects plane.  Not reliable.
vector_t intersectionPoint(vector_t lineVector, 
    vector_t linePoint, 
    vector_t planeNormal, 
    vector_t planePoint)
{
	vector_t diff = subVectors(linePoint,planePoint);
 
	return addVectors(addVectors(diff,planePoint),
        scaleVector(
            -dotProduct(diff,planeNormal) / 
                dotProduct(lineVector,planeNormal),
            lineVector));
}

// brute force intersection between line & plane
vector_t intersectionPoint2(vector_t xyz1, 
    vector_t xyz2, 
    double planeIntercept, 
    double planeSlope)
{
    double testFraction = 0.5;
    double step = 0.5;
    vector_t testPoint;
    vector_t diff = subVectors(xyz2, xyz1);
    while(step > 0.0001)
    {
        testPoint = addVectors(xyz1, 
            scaleVector(testFraction, diff));
        double testZ = planeIntercept + testPoint.x * planeSlope;
        step /= 2;
        if(testPoint.z > testZ)
        {
            testFraction -= step;
        }
        else
        {
            testFraction += step;
        }
    }
    
    return testPoint;
}

void writeTriangle(vector_t coord0, vector_t coord1, vector_t coord2)
{
// normal
    vector_t n = crossProduct(subVectors(coord1, coord0),
        subVectors(coord2, coord1));
    if(magnitude(n) > 0)
    {
        n = normalize(n);
    }
    
    writeFloat(n.x);
    writeFloat(n.y);
    writeFloat(n.z);

// vertex
    writeFloat(coord0.x);
    writeFloat(coord0.y);
    writeFloat(coord0.z);
// vertex
    writeFloat(coord1.x);
    writeFloat(coord1.y);
    writeFloat(coord1.z);
// vertex
    writeFloat(coord2.x);
    writeFloat(coord2.y);
    writeFloat(coord2.z);

// attribute
    uint16_t x = 0;
    fwrite(&x, 1, 2, out);
    
    triangleCount++;
}



void writeTriangle2(vector_t coord0, vector_t coord1, vector_t coord2)
{
    writeTriangle(coord2, coord1, coord0);
}


void writeQuad(vector_t coord0, vector_t coord1, vector_t coord2, vector_t coord3)
{
    if(isnan(coord0.x) ||
        isnan(coord0.y) ||
        isnan(coord0.z) ||
        isnan(coord1.x) ||
        isnan(coord1.y) ||
        isnan(coord1.z) ||
        isnan(coord2.x) ||
        isnan(coord2.y) ||
        isnan(coord2.z) ||
        isnan(coord3.x) ||
        isnan(coord3.y) ||
        isnan(coord3.z))
    {
        printf("writeQuad %d: Nan at triangle %d\n", __LINE__, triangleCount);
        return;
    }
        

    writeTriangle(coord0, coord1, coord2);
    writeTriangle(coord0, coord2, coord3);
}

void loopToSolid(vector_t *edgeLoop1, vector_t *edgeLoop2, int loopPoints)
{
    int j;
    for(j = 0; j < loopPoints - 1; j++)
    {
        writeQuad(edgeLoop1[j], 
            edgeLoop1[j + 1], 
            edgeLoop2[j + 1], 
            edgeLoop2[j]);
    }

// join last point to 1st point in the loop
    writeQuad(edgeLoop1[loopPoints - 1], 
        edgeLoop1[0], 
        edgeLoop2[0], 
        edgeLoop2[loopPoints - 1]);
}


// create a solid from edge loops
// edgeLoops -> XYZ coords
// capIt -> make quads for the ends, otherwise the last & first edge loops are joined
void loopsToSolid(vector_t **edgeLoops, 
    int totalLoops, 
    int loopPoints,
    int capIt)
{
    if(totalLoops < 2)
    {
        printf("loopsToSolid %d totalLoops=%d\n", __LINE__, totalLoops);
        return;
    }

#ifdef USE_CLIPPING
    capIt = 1;
#endif

    int i;
    for(i = 0; i < totalLoops - 1; i++)
    {
        vector_t *edgeLoop1 = edgeLoops[i];
        vector_t *edgeLoop2 = edgeLoops[i + 1];
        loopToSolid(edgeLoops[i], edgeLoops[i + 1], loopPoints);
    }
    
    if(!capIt)
    {
        loopToSolid(edgeLoops[totalLoops - 1], edgeLoops[0], loopPoints);
    }
    else
    {
// make end pieces.  Assume they're quads for now & loop 0 is the bottom
        writeQuad(edgeLoops[0][3], 
            edgeLoops[0][2], 
            edgeLoops[0][1], 
            edgeLoops[0][0]);
        writeQuad(edgeLoops[totalLoops - 1][0], 
            edgeLoops[totalLoops - 1][1], 
            edgeLoops[totalLoops - 1][2], 
            edgeLoops[totalLoops - 1][3]);
    }
}

#endif // _3DSTUFF_H


