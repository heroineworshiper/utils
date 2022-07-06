/*
 * Convert STL file to kicad PCB
 *
 * Copyright (C) 2022 Adam Williams <broadcast at earthling dot net>
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

// This program converts a bunch of STL files into a PCB layout.



// g++ -O2 stl_to_pcb.c -o stl_to_pcb -lm
// ./stl_to_pcb pcb_commands cable.kicad_pcb

#include "3dstuff.h"
#include <vector>

using namespace std;


// contour width
#define WIDTH 0.01
#define VIA_SIZE 0.4
#define VIA_DRILL 0.2
#define VIA_TOP "F.Cu"
#define VIA_BOTTOM "B.Cu"
#define VIA_FUDGE 0.15

#define X 160.0
#define Y 100.0
// maximum distance to be considered touching
#define THRESHOLD 0.01


const char *header = 
"(kicad_pcb (version 20211014) (generator pcbnew)\n"
"\n"
"  (general\n"
"    (thickness 1.6)\n"
"  )\n"
"\n"
"  (paper \"A4\")\n"
"  (layers\n"
"    (0 \"F.Cu\" signal)\n"
"    (31 \"B.Cu\" signal)\n"
"    (32 \"B.Adhes\" user \"B.Adhesive\")\n"
"    (33 \"F.Adhes\" user \"F.Adhesive\")\n"
"    (34 \"B.Paste\" user)\n"
"    (35 \"F.Paste\" user)\n"
"    (36 \"B.SilkS\" user \"B.Silkscreen\")\n"
"    (37 \"F.SilkS\" user \"F.Silkscreen\")\n"
"    (38 \"B.Mask\" user)\n"
"    (39 \"F.Mask\" user)\n"
"    (40 \"Dwgs.User\" user \"User.Drawings\")\n"
"    (41 \"Cmts.User\" user \"User.Comments\")\n"
"    (42 \"Eco1.User\" user \"User.Eco1\")\n"
"    (43 \"Eco2.User\" user \"User.Eco2\")\n"
"    (44 \"Edge.Cuts\" user)\n"
"    (45 \"Margin\" user)\n"
"    (46 \"B.CrtYd\" user \"B.Courtyard\")\n"
"    (47 \"F.CrtYd\" user \"F.Courtyard\")\n"
"    (48 \"B.Fab\" user)\n"
"    (49 \"F.Fab\" user)\n"
"    (50 \"User.1\" user)\n"
"    (51 \"User.2\" user)\n"
"    (52 \"User.3\" user)\n"
"    (53 \"User.4\" user)\n"
"    (54 \"User.5\" user)\n"
"    (55 \"User.6\" user)\n"
"    (56 \"User.7\" user)\n"
"    (57 \"User.8\" user)\n"
"    (58 \"User.9\" user)\n"
"  )\n"
"\n"
"  (setup\n"
"    (pad_to_mask_clearance 0)\n"
"    (pcbplotparams\n"
"      (layerselection 0x00010fc_ffffffff)\n"
"      (disableapertmacros false)\n"
"      (usegerberextensions false)\n"
"      (usegerberattributes true)\n"
"      (usegerberadvancedattributes true)\n"
"      (creategerberjobfile true)\n"
"      (svguseinch false)\n"
"      (svgprecision 6)\n"
"      (excludeedgelayer true)\n"
"      (plotframeref false)\n"
"      (viasonmask false)\n"
"      (mode 1)\n"
"      (useauxorigin false)\n"
"      (hpglpennumber 1)\n"
"      (hpglpenspeed 20)\n"
"      (hpglpendiameter 15.000000)\n"
"      (dxfpolygonmode true)\n"
"      (dxfimperialunits true)\n"
"      (dxfusepcbnewfont true)\n"
"      (psnegative false)\n"
"      (psa4output false)\n"
"      (plotreference true)\n"
"      (plotvalue true)\n"
"      (plotinvisibletext false)\n"
"      (sketchpadsonfab false)\n"
"      (subtractmaskfromsilk false)\n"
"      (outputformat 1)\n"
"      (mirror false)\n"
"      (drillshape 1)\n"
"      (scaleselection 1)\n"
"      (outputdirectory \"\")\n"
"    )\n"
"  )\n"
"\n"
"  (net 0 \"\")\n"
"\n";

const char *footer = 
")\n";


typedef struct
{
    float x1, y1, x2, y2;
} line_t;

typedef vector<triangle_t*> blob_t;
typedef vector<vector_t*> polygon_t;

vector<const char*> command;
vector<const char*> src_stl;
vector<const char*> dst_layer;
vector<int> subtract_mode;
vector<int> fill_mode;
FILE *dst;


void flip_line(line_t *l)
{
    float x2 = l->x2;
    float y2 = l->y2;
    l->x2 = l->x1;
    l->y2 = l->y1;
    l->x1 = x2;
    l->y1 = y2;
}

// 2D distance
float hypot2(vector_t *a, vector_t *b)
{
    return hypot(a->x - b->x, a->y - b->y);
}

int touches2(float x1, float y1, float x2, float y2)
{
    return (hypot(x1 - x2, y1 - y2) <= THRESHOLD);
}

int touches(triangle_t *a, triangle_t *b)
{
    int i, j;
    for(i = 0; i < 3; i++)
    {
        vector_t *a_coord = &a->coords[i];
        for(j = 0; j < 3; j++)
        {
            vector_t *b_coord = &b->coords[j];
            
            double dist = hypot3(
                (a_coord->x - b_coord->x),
                (a_coord->y - b_coord->y),
                (a_coord->z - b_coord->z));
            if(dist <= THRESHOLD)
            {
                return 1;
            }
        }
    }

    return 0;
}

// test if the 2 points are both on the same triangle in horiz
// ignore Z.  Return the distance if they're joined or -1 if they're not.
float joined(vector_t *a, vector_t *b, vector<triangle_t*> *horiz)
{
    int i, j;
    int got_it = 0;
    for(i = 0; i < horiz->size(); i++)
    {
        triangle_t *tri = horiz->at(i);
        for(j = 0; j < 3; j++)
        {
            if(touches2(a->x, a->y, tri->coords[j].x, tri->coords[j].y))
            {
                got_it = 1;
                break;
            }
        }
        
        if(got_it)
        {
            got_it = 0;
            for(j = 0; j < 3; j++)
            {
                if(touches2(b->x, b->y, tri->coords[j].x, tri->coords[j].y))
                {
                    got_it = 1;
                    break;
                }
            }
        }
        
        if(got_it)
        {
            return hypot2(a, b);
        }
    }
    return -1;
}

// if the top or bottom surface
int is_flat(triangle_t *t)
{
    double dist1 = fabs(t->coords[0].z - t->coords[1].z);
    double dist2 = fabs(t->coords[1].z - t->coords[2].z);

    if(dist1 <= THRESHOLD && dist2 <= THRESHOLD)
    {
        return 1;
    }

    return 0;
}

// if 2 points are on top
int is_top(triangle_t *t)
{
    double avg = 0;
    int total = 0;
    int i;
    for(i = 0; i < 3; i++)
    {
        avg += t->coords[i].z;
    }
    avg /= 3;

    for(i = 0; i < 3; i++)
    {
        if(t->coords[i].z > avg)
        {
            total++;
        }
    }
    
    if(total > 1)
    {
        return 1;
    }
    return 0;
}

void triangle_to_line(line_t *line, triangle_t *t)
{
    double avg = 0;
    int total = 0;
    int i;
    for(i = 0; i < 3; i++)
    {
        avg += t->coords[i].z;
    }
    avg /= 3;

    for(i = 0; i < 3; i++)
    {
        if(t->coords[i].z > avg)
        {
            if(total == 0)
            {
                line->x1 = t->coords[i].x;
                line->y1 = t->coords[i].y;
            }
            else
            {
                line->x2 = t->coords[i].x;
                line->y2 = t->coords[i].y;
            }

            total++;
        }
    }
}

vector_t* new_vector(float x, float y)
{
    vector_t *result = new vector_t;
    result->x = x;
    result->y = y;
    return result;
}

triangle_t* new_triangle(stl_triangle_t *src)
{
    triangle_t *result = new triangle_t;
    result->coords[0] = src->coords[0];
    result->coords[1] = src->coords[1];
    result->coords[2] = src->coords[2];
    return result;
}

vector<char*> split(char *s) 
{
    vector<char*> res;
    char *ptr = s;
    char string_[TEXTLEN];
    while(*ptr != 0)
    {
        while(*ptr != 0 && (*ptr == ' ' || *ptr == '\n'))
        {
            ptr++;
        }
        if(*ptr != 0)
        {
            res.push_back(ptr);
            while(*ptr != 0 && *ptr != ' ' && *ptr != '\n')
            {
                ptr++;
            }
            if(*ptr == ' ' || *ptr == '\n')
            {
                *ptr = 0;
                ptr++;
            }
        }
    }
    
    return res;
}

void poly_footer(const char *layer, int fill_mode)
{
    if(fill_mode)
    {
        fprintf(dst, "      ) (layer \"%s\") (width 0) (fill solid) (tstamp 394598bd-24bc-4df8-aada-011874c85980))\n",
            layer);
    }
    else
    {
        fprintf(dst, "      ) (layer \"%s\") (width %.2f) (fill none) (tstamp 394598bd-24bc-4df8-aada-011874c85980))\n",
            layer,
            WIDTH);
    }
}

void make_via(blob_t *blob)
{
// get center X, Y from blob
    int i, j, k;
    float x1, y1, x2, y2;
    int first = 1;
    for(i = 0; i < blob->size(); i++)
    {
        triangle_t *tri = blob->at(i);
        for(j = 0; j < 3; j++)
        {
            if(tri->coords[j].x < x1 || first)
            {
                x1 = tri->coords[j].x;
            }
            if(tri->coords[j].x > x2 || first)
            {
                x2 = tri->coords[j].x;
            }
            if(tri->coords[j].y < y1 || first)
            {
                y1 = tri->coords[j].y;
            }
            if(tri->coords[j].y > x2 || first)
            {
                y2 = tri->coords[j].y;
            }
            first = 0;
        }
    }
    
    float x = (x2 + x1) / 2;
    float y = (y2 + y1) / 2;
    fprintf(dst, 
        "  (via (at %.2f %.2f) (size %.2f) (drill %.2f) (layers \"%s\" \"%s\") (free) (net 0) (tstamp fd8c0709-f22d-4587-947c-8777874d1427))\n",
        X + x,
        Y - y - VIA_FUDGE,
        VIA_SIZE,
        VIA_DRILL,
        VIA_TOP,
        VIA_BOTTOM);
}

void make_footprint(int blob_index, int src_index, blob_t *blob)
{
    int i, j, k;
    vector<polygon_t*> polys;
    vector<triangle_t*> horiz;

// extract useful triangles from the blob
    for(j = 0; j < blob->size(); j++)
    {
        triangle_t *tri = blob->at(j);
        if(is_flat(tri))
        {
// transfer to horizontal triangle table for later
            horiz.push_back(tri);
            blob->erase(blob->begin() + j);
            j--;
        }
        else
        if(!is_top(tri))
        {
// discard vertical triangle on bottom
            blob->erase(blob->begin() + j);
            j--;
        }
    }

    printf("main %d: blob %d edge triangles=%d\n", __LINE__, blob_index, (int)blob->size());

// convert the top vertical triangles to lines
    vector<line_t*> lines;
    for(i = 0; i < blob->size(); i++)
    {
        line_t *line = new line_t;
        lines.push_back(line);
        triangle_to_line(line, blob->at(i));
    }


// sort lines to make polygon contours
    while(lines.size() > 0)
    {
        vector<line_t*> sorted;
        sorted.push_back(lines.at(0));
        lines.erase(lines.begin());
        int done = 0;

        while(!done)
        {
            done = 1;
            for(i = 0; i < lines.size(); i++)
            {
                line_t *unknown = lines.at(i);
                line_t *front = sorted.front();

                int got_it = 0;
                if(touches2(unknown->x2, unknown->y2, front->x1, front->y1))
                {
// end of unknown line touches start of sorted list
                    sorted.insert(sorted.begin(), unknown);
                    got_it = 1;
                }
                else
                if(touches2(unknown->x1, unknown->y1, front->x1, front->y1))
                {
// start of unknown line touches start of sorted list
                    flip_line(unknown);
                    sorted.insert(sorted.begin(), unknown);
                    got_it = 1;
                }
                else
                {
                    line_t *back = sorted.back();
                    if(touches2(unknown->x1, unknown->y1, back->x2, back->y2))
                    {
// start of unknown line touches end of sorted list
                        sorted.push_back(unknown);
                        got_it = 1;
                    }
                    else
                    if(touches2(unknown->x2, unknown->y2, back->x2, back->y2))
                    {
// end of unknown line touches end of sorted list
                        flip_line(unknown);
                        sorted.push_back(unknown);
                        got_it = 1;
                    }
                }

                if(got_it)
                {
                    lines.erase(lines.begin() + i);
                    i--;
                    done = 0;
                }
            }
        }

        printf("main %d: polygon lines=%d lines left=%d\n", __LINE__, (int)sorted.size(), (int)lines.size());

// create new polygon from sorted lines
        polygon_t *polygon = new polygon_t;
        polygon->push_back(new_vector(sorted.at(0)->x1, sorted.at(0)->y1));
        for(i = 0; i < sorted.size(); i++)
        {
            polygon->push_back(new_vector(sorted.at(i)->x2, sorted.at(i)->y2));
        }
        polys.push_back(polygon);
    }


// Combine polygons whose start or end are joined by horizontal triangles
// These are holes created by Freecad's sweep tool.
    printf("main %d: unjoined polygons=%d\n", __LINE__, (int)polys.size());
    int done = 0;
    while(!done)
    {
        done = 1;
        for(i = 0; i < polys.size(); i++)
        {
            polygon_t *a = polys.at(i);
            for(j = 0; j < polys.size(); j++)
            {
                if(j != i)
                {
                    polygon_t *b = polys.at(j);
// compute distances of all possible joins since multiple joins are possible
                    float dist[] = { -1, -1, -1, -1 };
                    dist[0] = joined(a->front(), b->front(), &horiz);
                    dist[1] = joined(a->front(), b->back(), &horiz);
                    dist[2] = joined(a->back(), b->front(), &horiz);
                    dist[3] = joined(a->back(), b->back(), &horiz);

                    float shortest = -1;
                    int shortest_index = -1;
                    for(k = 0; k < 4; k++)
                    {
                        if(dist[k] > 0 &&
                            (shortest_index < 0 ||
                            dist[k] < shortest))
                        {
                            shortest = dist[k];
                            shortest_index = k;
                        }
                    }
// printf("main %d i=%d j=%d joined distances=%f %f %f %f shortest=%f shortest_index=%d\n", 
// __LINE__, 
// i, 
// j, 
// dist[0], 
// dist[1], 
// dist[2], 
// dist[3],
// shortest,
// shortest_index);

// transfer the b polygon to the a polygon
                    if(shortest_index == 0)
                    {
//printf("main %d: a=%d b=%d\n", __LINE__, (int)a->size(), (int)b->size());
                        while(b->size())
                        {
                            a->insert(a->begin(), b->front());
                            b->erase(b->begin());
                        }
                        done = 0;
                    }
                    else
                    if(shortest_index == 1)
                    {
//printf("main %d: a=%d b=%d\n", __LINE__, (int)a->size(), (int)b->size());
                        while(b->size())
                        {
                            a->insert(a->begin(), b->back());
                            b->pop_back();
                        }
                        done = 0;
                    }
                    else
                    if(shortest_index == 2)
                    {
//printf("main %d: a=%d b=%d\n", __LINE__, (int)a->size(), (int)b->size());
                        while(b->size())
                        {
                            a->push_back(b->front());
                            b->erase(b->begin());
                        }
                        done = 0;
                    }
                    else
                    if(shortest_index == 3)
                    {
//printf("main %d: a=%d b=%d\n", __LINE__, (int)a->size(), (int)b->size());
                        while(b->size())
                        {
                            a->push_back(b->back());
                            b->pop_back();
                        }
                        done = 0;
                    }

// erase the b polygon
                    if(!done)
                    {
                        polys.erase(polys.begin() + j);
                        j = polys.size();
                        i = polys.size();
                    }
                }
            }
        }
    }

    printf("main %d: joined polygons=%d\n", __LINE__, (int)polys.size());





// start the output footprint
    fprintf(dst, 
        "  (footprint \"LOGO\" (layer \"F.Cu\")\n"
        "    (tedit 0) (tstamp 0ee69f2a-4ef2-4004-81f1-f2c3e293a6f5)\n"
        "    (at %.2f %.2f)\n"
        "    (attr board_only exclude_from_pos_files exclude_from_bom)\n",
        X,
        Y);

// create subtraction booleans with child polygons
    if(subtract_mode.at(src_index))
    {
// TODO: detect child polygons & OR polygons.
// Parent is hard coded for now & we assume there are no overlapping OR polygons.
        polygon_t *polygon = polys.at(0);
        polygon_t final;
        for(i = 0; i < polygon->size(); i++)
        {
            final.push_back(polygon->at(i));
        }

        int poly_index;
        for(poly_index = 1; poly_index < polys.size(); poly_index++)
        {
            polygon_t *polygon = polys.at(poly_index);

// nearest points in 2 polygons
            int nearest_point1 = -1;
            int nearest_point2 = -1;
            float nearest_dist = -1;
            for(i = 0; i < final.size(); i++)
            {
                vector_t *point1 = final.at(i);
                for(j = 0; j < polygon->size(); j++)
                {
                    vector_t *point2 = polygon->at(j);
                    float dist = hypot(point1->x - point2->x, point1->y - point2->y);
                    if(dist < nearest_dist || nearest_dist < 0)
                    {
                        nearest_dist = dist;
                        nearest_point1 = i;
                        nearest_point2 = j;
                    }
                }
            }

// insert polygon
            for(i = 0; i < polygon->size(); i++)
            {
                int offset2 = i + nearest_point2;
                if(offset2 >= polygon->size())
                {
                    offset2 -= polygon->size();
                }
                final.insert(final.begin() + nearest_point1 + i + 1, polygon->at(offset2));
            }

// close off inserted polygon
            final.insert(final.begin() + nearest_point1 + polygon->size() + 1,
                polygon->at(nearest_point2));
            final.insert(final.begin() + nearest_point1 + polygon->size() + 2,
                final.at(nearest_point1));

        }

// Print booleaned polygons in 1 fp_poly
        fprintf(dst, "    (fp_poly (pts\n");

        for(i = 0; i < final.size(); i++)
        {
            vector_t *point = final.at(i);
            fprintf(dst, "        (xy %f %f)\n", point->x, -point->y);
        }
        poly_footer(dst_layer.at(src_index), fill_mode.at(src_index));

    }
    else
    {
// OR all the polygons in multiple fp_poly
        int poly_index;
        for(poly_index = 0; poly_index < polys.size(); poly_index++)
        {
            polygon_t *polygon = polys.at(poly_index);
            fprintf(dst, "    (fp_poly (pts\n");

            for(i = 0; i < polygon->size(); i++)
            {
                vector_t *point = polygon->at(i);
                fprintf(dst, "        (xy %f %f)\n", point->x, -point->y);
            }
            poly_footer(dst_layer.at(src_index), fill_mode.at(src_index));

        }
    }


// end the output footprint
    fprintf(dst, "  )\n");
}



int main(int argc, char *argv[])
{
    int i, j, k;
    
    if(argc < 3)
    {
        printf("Usage: stl_to_pcb <command file> <output PCB file>\n");
        printf("The command file has 1 line per STL file & multiple lines\n");
        printf("The command is polys or vias\n");
        printf("The stl file for polys contains XY polygons with arbitrary Z height.\n");
        printf("Subtract mode causes all polygons in the file to be drawn as 1 path.  Kicad interprets overlapping polygons as holes.\n");
        printf("Fill mode creates a solid mask\n");
        printf("The stl file for vias contains cylinders on the XY plane with arbitrary Z height & arbitrary radius.\n");
        printf("The center of each cylinder becomes a via of hard coded size.\n");
        printf("<command> <STL filename> <output layer><subtract mode 1 or 0><fill mode 1 or 0>\n");
        printf("Example command lines:\n");
        printf("polys trace.stl F.Cu 0 1\n");
        printf("vias vias.stl\n");
        return 1;
    }

    int src_count = 0;
    const char *command_path = 0;
    const char *dst_path = 0;
    for(i = 1; i < argc; i++)
    {
        if(i == 2)
        {
            dst_path = argv[i];
        }
        else
        if(i == 1)
        {
            command_path = argv[i];
        }
    }
    
    FILE *in = fopen(command_path, "r");
    if(!in)
    {
        printf("main %d: Couldn't open %s\n", __LINE__, command_path);
        return 1;
    }
    
    char string[TEXTLEN];
    while(!feof(in))
    {
        char *result = fgets(string, TEXTLEN, in);
        if(!result)
        {
            break;
        }
        
        vector<char*> tokens = split(string);
//         for(i = 0; i < tokens.size(); i++)
//         {
//             printf("%s\n", tokens.at(i));
//         }

        if(tokens.size() > 0)
        {
            if(tokens.at(0)[0] == '#' || tokens.at(0)[0] == '\n')
            {
                // comment
            }
            else
            {
                command.push_back(strdup(tokens.at(0)));
                src_stl.push_back(strdup(tokens.at(1)));

                if(!strcasecmp(command.back(), "polys"))
                {
                    dst_layer.push_back(strdup(tokens.at(2)));
                    subtract_mode.push_back(atoi(tokens.at(3)));
                    fill_mode.push_back(atoi(tokens.at(4)));
                }
                else
                {
                    dst_layer.push_back("");
                    subtract_mode.push_back(0);
                    fill_mode.push_back(0);
                }

                printf("main %d: command=%s input=%s layer=%s subtract=%d fill=%d\n", 
                    __LINE__, 
                    command.back(),
                    src_stl.back(),
                    dst_layer.back(),
                    subtract_mode.back(),
                    fill_mode.back());
            }
        }
    }
    fclose(in);

    
//     printf("main %d\n", __LINE__);
//     for(i = 0; i < src_stl.size(); i++)
//     {
//         printf("Source STL=%s Dest layer=%s\n", src_stl.at(i), dst_layer.at(i));
//     }
    printf("Dest PCB=%s\n", dst_path);
    
    dst = fopen(dst_path, "r");
    if(dst)
    {
        fclose(dst);
        printf("%s exists.  Overwrite? (y/n)\n", dst_path);
        char c = fgetc(stdin);
        if(c != 'y')
        {
            printf("Giving up & going to a movie.\n");
            return 1;
        }
    }

    dst = fopen(dst_path, "w");
    if(!dst)
    {
        printf("Couldn't open %s for writing.\n", dst_path);
        return 1;
    }

    fprintf(dst, "%s", header);

    int src_index;
    for(src_index = 0; src_index < src_stl.size(); src_index++)
    {
        vector<triangle_t*> src;
        vector<blob_t*> blobs;

        printf("main %d reading %s\n", __LINE__, src_stl.at(src_index));
        stl_triangle_t *stl = read_stl(src_stl.at(src_index), &src_count);

// convert to a triangle array
        for(i = 0; i < src_count; i++)
        {
            src.push_back(new_triangle(&stl[i]));
        }

// extract blobs from the file
        while(src.size() > 0)
        {
// start a new blob with the last triangle
            blob_t *blob = new blob_t;
            blobs.push_back(blob);
            blob->push_back(src.back());
            src.pop_back();


            int done = 0;
            while(!done)
            {
                done = 1;

// test all triangles in the blob with all the unknown triangles
                for(i = 0; i < blob->size(); i++)
                {
                    triangle_t *known = blob->at(i);
                    for(j = 0; j < src.size(); j++)
                    {
                        triangle_t *unknown = src.at(j);
                        if(touches(unknown, known))
                        {
// transfer the triangle to the blob
                            blob->push_back(unknown);
                            src.erase(src.begin() + j);
                            j--;
                            done = 0;
                        }
                    }
                }
            }
        }



        printf("main %d: total_blobs=%d\n", __LINE__, (int)blobs.size());


        if(!strcasecmp(command.at(src_index), "polys"))
        {
            for(i = 0; i < blobs.size(); i++)
            {
                blob_t *blob = blobs.at(i);
                make_footprint(i, src_index, blobs.at(i));
            }
        }
        else
        if(!strcasecmp(command.at(src_index), "vias"))
        {
            for(i = 0; i < blobs.size(); i++)
            {
                blob_t *blob = blobs.at(i);
                make_via(blobs.at(i));
            }
        }
        else
        {
            printf("main %d: unknown command %s\n", __LINE__, command.at(src_index));
        }
    }

    fprintf(dst, "%s", footer);
    fclose(dst);
}






