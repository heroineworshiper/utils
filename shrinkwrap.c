/*
 * Shrinkwrap utility
 *
 * Copyright (C) 2021 Adam Williams <broadcast at earthling dot net>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 */


// Scrap.  Blender can do this.
// Loads a shell with parallel sides & a 2D pattern STL file.  Maps 
// the pattern on the shell.  Won't work with a conical shell.

// Add an X2 coordinate to every vertex on the shell.  The X2
// coordinate is the horizontal distance from the vertex to the
// center of the shell if the shell is unwrapped & laid flat.  

// Then take
// every vertex on the pattern & find the nearest 4 vertices on the shell
// with X2 coordinates that bound the pattern's X.

// Determine the fraction
// of the X2 range the isogrid's X coordinate occupies.  Multiply the X2
// fraction by the original X range of the shell vertices to get a new X of
// the pattern vertex.   Multiply the X2 fraction by the original Y range
// of the shell vertices to get a new Y of the isogrid vertex.  Leave Z
// alone.

// gcc -O2 shrinkwrap.c -o shrinkwrap -lm
// ./shrinkwrap speaker.stl speaker3.stl speaker4.stl

#include "3dstuff.h"

// get the center X & the max Y
void get_model_extents(float *center_x, 
    float *max_y, 
    int *center_triangle,
    triangle_t *triangles, 
    int count)
{
    int i, j;
    float min_x = 65536;
    float max_x = -65536;
    *max_y = -65536;

    int first = 1;
    for(i = 0; i < count; i++)
    {
        triangle_t *triangle = triangles + i;
        for(j = 0; j < 3; j++)
        {
            vector *coord = &triangle->coords[j];
            if(first || coord->x < min_x)
            {
                min_x = coord->x;
            }
            if(first || coord->x > max_x)
            {
                max_x = coord->x;
            }
            if(first || coord->y > *max_y)
            {
                *max_y = coord->y;
            }
            first = 0;
        }
    }

    float want_center_x = (max_x + min_x) / 2;
//    printf("get_model_extents %d min_x=%f max_x=%f\n", __LINE__, min_x, max_x);

    *center_x = 65536;
    first = 1;
    for(i = 0; i < count; i++)
    {
        triangle_t *triangle = triangles + i;
        for(j = 0; j < 3; j++)
        {
            vector *coord = &triangle->coords[j];
            if(first ||
                fabs(coord->x - want_center_x) < fabs(*center_x - want_center_x))
            {
                *center_x = coord->x;
                *center_triangle = i;
            }
            first = 0;
        }
    }
}

void flatten_triangle(triangle_t *dst_model, 
    triangle_t *src_model, 
    int total,
    int number,
    int neighbor,
    float center_x,
    float max_y)
{
    int i;

// no neighboring flattened triangle
    if(neighbor < 0)
    {
// get closest vertex to center_x, max_y
        triangle_t *src = &src_model[number];
        triangle_t *dst = &dst_model[number];
        float dist = 0;
        int anchor = 0;
        for(i = 0; i < 3; i++)
        {
            vector *coord = &src->coords[i];
            float dist2 = hypot(coord->y - max_y, coord->x - center_x);
            if(i == 0 ||
                dist2 < dist)
            {
                anchor = i;
                dist = dist2;
            }
        }
        
// rotate the 2 other vertices to be on max_y
        vector *anchor_coord = &src->coords[anchor];
        for(i = 0; i < 3; i++)
        {
            if(i == anchor)
            {
                memcpy(&dst->coords[i], &src->coords[i], sizeof(vector));
            }
            else
            {
                vector *src_coord = &src->coords[i];
                vector *dst_coord = &dst->coords[i];
                
                float x_mag = hypot(src_coord->x - anchor_coord->x,
                    src_coord->y - anchor_coord->y);
                float z_mag = hypot(src_coord->z - anchor_coord->z, 
                    src_coord->y - anchor_coord->y);
// x sign is based on what side of the anchor we're on
                
// z sign is the same as the source
                
            }
        }
    }
}



int main(int argc, char *argv[])
{
    char *shell_path;
    char *pattern_path;
    char *output_path;
    char string[TEXTLEN];
    
    if(argc < 4)
    {
        printf("Usage: helix <shell> <pattern> <output>\n");
        return 1;
    }


    shell_path = argv[1];
    pattern_path = argv[2];
    output_path = argv[3];


    FILE *fd = fopen(output_path, "r");
    if(fd)
    {
        fclose(fd);
        printf("Output %s exists.  Replace it? (y/n)\n", output_path);
        char* _ = fgets(string, TEXTLEN, stdin);
        if(!strcmp(string, "y\n"))
        {
            printf("Overwriting\n");
        }
    }

    int shell_count;
    triangle_t *shell = read_stl(shell_path, &shell_count);

    int pattern_count;
    triangle_t *pattern = read_stl(pattern_path, &pattern_count);


// get extents of shell
    float shell_x, shell_y;
    int shell_triangle0;
    get_model_extents(&shell_x, &shell_y, &shell_triangle0, shell, shell_count);
    printf("main %d shell_x=%f shell_y=%f\n", __LINE__, shell_x, shell_y);

    float pattern_x, pattern_y;
    int pattern_triangle0;
    get_model_extents(&pattern_x, &pattern_y, &pattern_triangle0, pattern, pattern_count);
    printf("main %d pattern_x=%f pattern_y=%f\n", __LINE__, pattern_x, pattern_y);

// flatten the triangles
    triangle_t *flat_shell = malloc(sizeof(triangle_t) * shell_count);
    uint8_t *processed = calloc(1, shell_count);
// flatten the 1st triangle
    flatten_triangle(flat_shell, 
        shell, 
        shell_count, 
        shell_triangle0, 
        -1, 
        shell_x, 
        shell_y);
    processed[shell_triangle0] = 1;

    return 0;
}





















