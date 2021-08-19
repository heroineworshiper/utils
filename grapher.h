/*
 * simple data graphing program
 * Copyright (C) 2020 Adam Williams <broadcast at earthling dot net>
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


#ifndef GRAPHER_H
#define GRAPHER_H

#include "guicast.h"

// determined by colors
#define MAX_COLUMNS 9


class Row
{
public:
	Row();
	void copy(Row *src);
	float data[MAX_COLUMNS];
	int total;
};

class MWindow : public BC_Window
{
public:
	MWindow();
	~MWindow();
	
	void create_objects();
	int keypress_event();
	int resize_event(int w, int h);
	int translation_event();
	int cursor_motion_event();
	int cursor_enter_event();
	int cursor_leave_event();
	int button_press_event();
	int button_release_event();

	void draw_it();
	void calculate_limits();
	void draw_crosshair(int flash_it, int want_visible);
	void draw_number(int x, int y, float number);
	char* number_to_text(char *string, float number);
	int number_width(float number);
	int get_cursor_row();
	int row_to_x(int row);
	void zoom_in(int is_keypress);
	void zoom_out(int is_keypress);

// x aligned to a pixel
	int mouse_x;
	int mouse_y;
// x aligned to a row
	int crosshair_x;
	int crosshair_y;
	int cursor_row;
	int crosshair_visible;
	int dragging;
	int drag_origin_row;
	int drag_origin_y;
// visible rows when dragging started
	int drag_origin_row2;
	int drag_origin_row1;

// calculated values from the last drawing
// values start at 0 internally

// the rows to draw
	int row1;
	int row2;
// calculate the range from these rows
	int range_row1;
	int range_row2;
// the limits
	float max;
	float min;
// don't recalculate the range in every drawing
	int override_range;
	int column1;
	int column2;
	int graph_x1;
	int graph_x2;
	int graph_y1;
	int graph_y2;


// if zoomed, we use these values
	int zoomed_row1;
	int zoomed_row2;
};


class Loader : public Thread
{
public:
	Loader();
	
	void run();
	void load_file();
	
	
	static int64_t byte1, byte2;
	
	Condition *started;
};




#endif




