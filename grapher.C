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

// compilation command:
// g++ -O2  -o grapher grapher.C \
// 		-I../hvirtual/guicast \
// 		../hvirtual/guicast/x86_64/libguicast.a \
// 		-lX11 \
// 		-lXext \
// 		-lXft \
// 		-lXv \
// 		-lpthread \
// 		-lm \
// 		-lpng


#include "bchash.h"
#include "condition.h"
#include "cursors.h"
#include "filesystem.h"
#include "grapher.h"
#include "keys.h"
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>



#define MARGIN 5
// column colors
int colors[] = 
{
	0x000000,
	0xff0000,
	0x00ff00,
	0x0000ff,
	0xff00ff,
	0xff8080,
	0xffff00,
	0x008080,
	0x808080
};

BC_Hash *defaults;
int window_x = 0;
int window_y = 0;
int window_w = 640;
int window_h = 480;
char filename[BCTEXTLEN];
int64_t file_modified = -1;

// arguments from the user
int column1_arg = -1;
int column2_arg = -1;
int columns_arg = -1;
int row1_arg = -1;
int row2_arg = -1;
int have_row1 = 0;
int have_row2 = 0;
int64_t byte1_arg = -1;
int64_t byte2_arg = -1;
int bytes_per_number = 2;


// binary modes
#define BINARY_INT16 1
int binary_mode = 0;

MWindow *mwindow = 0;
Loader *loader = 0;
Row *data = 0;
int total_rows = 0;
int rows_allocated = 0;


void allocate_rows()
{
	if(total_rows >= rows_allocated)
	{
		int new_allocated = rows_allocated * 2;
        if(new_allocated < total_rows)
        {
            new_allocated = total_rows;
        }
		Row *new_data = new Row[new_allocated];
		for(int i = 0; i < total_rows && i < rows_allocated; i++)
		{
			new_data[i].copy(&data[i]);
		}
		delete [] data;
		data = new_data;
		rows_allocated = new_allocated;
	}
}




void save_defaults()
{
	defaults->update("WINDOW_X", window_x);
	defaults->update("WINDOW_Y", window_y);
	defaults->update("WINDOW_W", window_w);
	defaults->update("WINDOW_H", window_h);
	defaults->save();
}

void dump()
{
	printf("dump %d\n", __LINE__);
	for(int i = 0; i < total_rows; i++)
	{
		Row *row = &data[i];
		for(int j = 0; j < row->total; j++)
		{
			printf("%f ", row->data[j]);
		}
		printf("\n");
	}
}


Row::Row()
{
	total = 0;
}

void Row::copy(Row *src)
{
	int i;
	for(i = 0; i < src->total; i++)
	{
		data[i] = src->data[i];
	}
	total = src->total;
}

MWindow::MWindow()
 : BC_Window(filename, 
	window_x,
	window_y,
    window_w, 
    window_h,
	256,
	256,
	1,
	0,
	1,
	WHITE)
{
	crosshair_x = -1;
	crosshair_y = -1;
	mouse_x = -1;
	mouse_y = -1;
	cursor_row = -1;
	crosshair_visible = 0;

	// calculated values from the last drawing
	// values start at 0 internally
	row1 = -1;
	row2 = -1;
	column1 = -1;
	column2 = -1;
	graph_x1 = -1;
	graph_x2 = -1;
	graph_y1 = -1;
	graph_y2 = -1;

	// if zoomed, we use these values
	zoomed_row1 = -1;
	zoomed_row2 = -1;
	
	dragging = 0;
}

MWindow::~MWindow()
{
}

void MWindow::create_objects()
{
	lock_window("MainWindow::create_objects");

	show_window();
	unlock_window();
	loader->started->unlock();
}

void MWindow::draw_crosshair(int flash_it, int want_visible)
{
	if(crosshair_visible != want_visible)
	{
		int max_w = get_text_width(MEDIUMFONT, "00000000");
		clear_box((graph_x2 + graph_x1) / 2 - max_w / 2, 
			graph_y2 + MARGIN,
			max_w,
			get_text_height(MEDIUMFONT));

		
// printf("MWindow::draw_crosshair %d %d %d %d\n", 
// 	__LINE__, 
// 	mouse_x, 
// 	graph_x1,
// 	graph_x2);
		if(mouse_x >= graph_x1 && 
			mouse_x < graph_x2 && 
			mouse_y >= graph_y1 &&
			mouse_y < graph_y2)
		{
			set_color(0xffffff);
			set_line_dashes(1);
			set_inverse();
			draw_line(crosshair_x, graph_y1, crosshair_x, graph_y2);
			draw_line(graph_x1, crosshair_y, graph_x2, crosshair_y);
			set_line_dashes(0);
			set_opaque();

			if(want_visible)
			{
				set_color(0x000000);
				char string[BCTEXTLEN];
				sprintf(string, "%d", cursor_row);
				draw_center_text((graph_x2 + graph_x1) / 2, 
					graph_y2 + get_text_height(MEDIUMFONT),
					string);
			}
			
			crosshair_visible = !crosshair_visible;
		}
	}

	if(flash_it)
	{
		flash();
	}
}

int MWindow::row_to_x(int row)
{
	if(zoomed_row1 >= 0)
	{
		row1 = zoomed_row1;
		row2 = zoomed_row2;
	}

	return graph_x1 + 
		(row - row1) * 
		(graph_x2 - graph_x1 - 1) / 
		(row2 - row1 - 1);
}

int MWindow::cursor_motion_event()
{
// hide previous crosshair
	draw_crosshair(0, 0);
	
	int row = cursor_row = get_cursor_row();
	mouse_x = get_cursor_x();
	mouse_y = get_cursor_y();
// cursor position aligned to a row
	int cursor_x = row_to_x(row);
	int cursor_y = get_cursor_y();

	if(dragging)
	{
		int new_row1 = drag_origin_row1 + (drag_origin_row - row);
//printf("MWindow::cursor_motion_event %d %d\n", __LINE__, new_row1);
		if(new_row1 != row1)
		{
			int rows = row2 - row1;
			row2 = new_row1 + rows;
			row1 = new_row1;
			if(row1 < 0)
			{
				row2 += -row1;
				row1 = 0;
			}
			
			if(row2 > total_rows)
			{
				row1 -= row2 - total_rows;
				row2 = total_rows;
			}
			if(zoomed_row1 >= 0)
			{
				zoomed_row2 = row2;
				zoomed_row1 = row1;
			}
			

// draw new crosshair
// cursor position aligned to a row
			cursor_x = row_to_x(drag_origin_row);
			crosshair_x = cursor_x;
			crosshair_y = cursor_y;
			draw_it();
		}
		else
		{
			cursor_x = row_to_x(drag_origin_row);
			crosshair_x = cursor_x;
			crosshair_y = cursor_y;
			draw_crosshair(1, 1);
		}
	}
	else
	{
		if(mouse_x >= graph_x1 && 
			mouse_x < graph_x2 &&
			mouse_y >= graph_y1 &&
			mouse_y < graph_y2)
		{
			set_cursor(TRANSPARENT_CURSOR, 1, 0);
// draw new crosshair
			crosshair_x = cursor_x;
			crosshair_y = cursor_y;
			draw_crosshair(1, 1);
		}
		else
		{
			set_cursor(ARROW_CURSOR, 1, 0);
			draw_crosshair(1, 0);
		}
	}

	return 0;
}

int MWindow::cursor_enter_event()
{
	
	
	
	
	return 0;
}

int MWindow::cursor_leave_event()
{
// hide previous crosshair
	draw_crosshair(1, 0);
	mouse_x = -1;
	mouse_y = -1;
	crosshair_x = -1;
	crosshair_y = -1;
	return 0;
}

int MWindow::get_cursor_row()
{
	int cursor_x = get_cursor_x();
	int cursor_y = get_cursor_y();
	int result;
	
	if(dragging)
	{
		result = (cursor_x - graph_x1) * 
				(drag_origin_row2 - drag_origin_row1) / 
				(graph_x2 - graph_x1) + drag_origin_row1;
	}
	else
	{
		result = (cursor_x - graph_x1) * 
				(row2 - row1) / 
				(graph_x2 - graph_x1) + row1;
	}
	
	return result;
}

void MWindow::zoom_in(int is_keypress)
{
// zoom in
	if(row2 >= 0 && row1 >= 0 && row2 - row1 > 16)
	{
// hide the cursor because of event mangling
		if(is_keypress)
		{
			draw_crosshair(0, 0);
		}


		int rows = row2 - row1;
		cursor_row = get_cursor_row();
		int zoomed_rows = rows / 2;
		int left_rows = cursor_row - row1;
		zoomed_row1 = cursor_row - left_rows / 2;
		zoomed_row2 = zoomed_row1 + zoomed_rows;
		if(zoomed_row1 < 0)
		{
			zoomed_row1 = 0;
		}
		if(zoomed_row2 > total_rows)
		{
			zoomed_row2 = total_rows;
		}
		cursor_row = get_cursor_row();
		crosshair_x = row_to_x(cursor_row);
		crosshair_y = get_cursor_y();
		draw_it();
	}
}

void MWindow::zoom_out(int is_keypress)
{
// zoom out
	if(row2 >= 0 && row1 >= 0)
	{
// hide the cursor because of event mangling
		if(is_keypress)
		{
			draw_crosshair(0, 0);
		}
		
		int rows = row2 - row1;
		cursor_row = get_cursor_row();
		int zoomed_rows = rows * 2;
		int left_rows = cursor_row - row1;
		zoomed_row1 = cursor_row - left_rows * 2;
		zoomed_row2 = zoomed_row1 + zoomed_rows;
		if(zoomed_row1 < 0)
		{
			zoomed_row1 = 0;
		}
		if(zoomed_row2 > total_rows)
		{
			zoomed_row2 = total_rows;
		}
		cursor_row = get_cursor_row();
		crosshair_x = row_to_x(cursor_row);
		crosshair_y = get_cursor_y();
		draw_it();
	}
}

int MWindow::button_press_event()
{
	mouse_x = get_cursor_x();
	mouse_y = get_cursor_y();
	if(mouse_x >= graph_x1 && 
		mouse_x < graph_x2 && 
		mouse_y >= graph_y1 &&
		mouse_y < graph_y2)
	{
		if(get_buttonpress() == 4)
		{
			zoom_in(0);
		}
		else
		if(get_buttonpress() == 5)
		{
			zoom_out(0);
		}
		else
	// print the row
		{
				int row = cursor_row = get_cursor_row();
				crosshair_x = row_to_x(cursor_row);
				crosshair_y = get_cursor_y();
				draw_crosshair(1, 1);

				dragging = 1;
				drag_origin_row = row;
				drag_origin_row2 = row2;
				drag_origin_row1 = row1;
				drag_origin_y = crosshair_y;

				if(binary_mode)
				{
					int64_t hex_offset = row * (bytes_per_number * column2) + 
						Loader::byte1;
					printf("row=%d byte=0x%lx values=", row, hex_offset);
				}
				else
				{
					printf("%d values=", row);
				}

				Row *ptr = &data[row];
				char string[BCTEXTLEN];
				for(int i = column1; i < column2; i++)
				{
					if(i < ptr->total)
					{
						printf("%s ", number_to_text(string, ptr->data[i]));
					}
				}
				printf("\n");
		}
	}
	return 1;
}

int MWindow::button_release_event()
{
	dragging = 0;
	return 1;
}


int MWindow::keypress_event()
{
	switch(get_keypress())
	{
		case ESC:
		case 'q':
			set_done(0);

			return 1;
			break;
		
		case UP:
			if(mouse_x >= graph_x1 && 
				mouse_x < graph_x2 && 
				mouse_y >= graph_y1 &&
				mouse_y < graph_y2)
			{
				zoom_out(1);
			}
			break;
		
		case DOWN:
			if(mouse_x >= graph_x1 && 
				mouse_x < graph_x2 && 
				mouse_y >= graph_y1 &&
				mouse_y < graph_y2)
			{
				zoom_in(1);
			}
			break;
	}

	return 0;
}

int MWindow::translation_event()
{
	window_x = get_x();
	window_y = get_y();
	::save_defaults();
}

int MWindow::resize_event(int w, int h)
{
	window_w = w;
	window_h = h;
	::save_defaults();
	draw_it();
	return 1;
}

char* MWindow::number_to_text(char *string, float number)
{
	sprintf(string, "%f", number);
// chop at 6 characters or the . position, whichever is greater
	int dot = -1;
	int max_length = 6;
	for(int i = 0; i < strlen(string); i++)
	{
		if(string[i] == '.')
		{
			dot = i;
			break;
		}
	}

	if(dot < max_length)
	{
		string[max_length] = 0;
		if(string[max_length - 1] == '.')
		{
			string[max_length - 1] = 0;
		}
	}
	else
	{
		string[dot] = 0;
	}
	
	return string;
}

int MWindow::number_width(float number)
{
	char string[BCTEXTLEN];
	number_to_text(string, number);
	return get_text_width(MEDIUMFONT, string);
}

void MWindow::draw_number(int x, int y, float number)
{
	char string[BCTEXTLEN];
	number_to_text(string, number);

	draw_text(x,
		y,
		string);

}


void MWindow::calculate_limits()
{
	max = 0;
	min = 0;
	int first = 1;
	for(int i = range_row1; i < range_row2; i++)
	{
        if(i >= 0)
        {
		    Row *row = &data[i];
		    for(int j = column1; j < row->total && j < column2; j++)
		    {
			    float value = row->data[j];
			    if(value > max || first)
			    {
				    max = value;
			    }

			    if(value < min || first)
			    {
				    min = value;
			    }
			    first = 0;
		    }
        }
	}
}


void MWindow::draw_it()
{
	if(!dragging)
	{
// the rows for calculating the range
		range_row1 = row1_arg;
		range_row2 = row2_arg;


// print all lines
		if(!have_row1 && !have_row2)
		{
			range_row1 = 0;
			range_row2 = total_rows;
		}
		else
		if(!have_row2)
		{
// print from row1 to the end
			range_row2 = total_rows;
		}
		else
		if(!have_row1)
		{
// print from the start to row 2
			range_row1 = 0;
		}

// convert rows before end to absolute rows
		if(have_row1 && range_row1 < 0)
		{
			range_row1 = total_rows + range_row1;
		}

		if(have_row2 && range_row2 < 0)
		{
			range_row2 = total_rows + range_row2;
		}

		if(range_row1 > total_rows)
		{
			range_row1 = total_rows;
		}

		if(range_row2 > total_rows)
		{
			range_row2 = total_rows;
		}

// only draw these rows
		if(zoomed_row1 >= 0)
		{
			row1 = zoomed_row1;
			row2 = zoomed_row2;
		}
		else
		{
			row1 = range_row1;
			row2 = range_row2;
		}
	}

// calculate column numbers
	column1 = column1_arg;
	column2 = column2_arg;

	int total_columns = 0;
	for(int i = row1; i < row2; i++)
	{
        if(i >= 0)
        {
		    Row *row = &data[i];
		    if(row->total > total_columns)
		    {
			    total_columns = row->total;
		    }
        }
	}

	if(column1 < 0 || column1 >= total_columns)
	{
		column1 = 0;
	}
	if(column2 < 0 || column2 >= total_columns)
	{
		column2 = total_columns;
	}


// calculate limits
	if(!override_range)
	{
		calculate_limits();
	}


	clear_box(0, 0, window_w, window_h);

	int columns = column2 - column1;
	int box_w = 10;
	int legend_w = MARGIN + 
		box_w + 
		MARGIN +
		get_text_width(MEDIUMFONT, "0");
	int legend_x = window_w - legend_w;
	int text_h = get_text_height(MEDIUMFONT);
	int scale_w = number_width(max);
	int scale_w2 = number_width(min);
	if(scale_w2 > scale_w)
	{
		scale_w = scale_w2;
	}
	scale_w2 = number_width(0.0);
	if(scale_w2 > scale_w)
	{
		scale_w = scale_w2;
	}

	graph_x2 = legend_x - MARGIN;
	graph_x1 = MARGIN * 2 + scale_w;
	graph_y1 = MARGIN;
	graph_y2 = window_h - MARGIN - text_h - MARGIN;

	set_color(BLACK);
	draw_rectangle(graph_x1, 
		graph_y1, 
		graph_x2 - graph_x1, 
		graph_y2 - graph_y1);
// draw Y scale
	set_font(MEDIUMFONT);
	draw_number(MARGIN,
		graph_y1 + text_h,
		max);
	draw_number(MARGIN,
		graph_y2,
		min);

// draw X scale
	char string[BCTEXTLEN];
	sprintf(string, "%d", row1);
	draw_text(graph_x1,
		graph_y2 + text_h + MARGIN,
		string);
	draw_line(graph_x1 - MARGIN,
		graph_y1,
		graph_x1,
		graph_y1);
	sprintf(string, "%d", row2);
	draw_text(graph_x2 - get_text_width(MEDIUMFONT, string),
		graph_y2 + text_h + MARGIN,
		string);
	draw_line(graph_x1 - MARGIN,
		graph_y2,
		graph_x1,
		graph_y2);

	int zero_y = graph_y2 - (-min) / (max - min) * (graph_y2 - graph_y1);
	if(zero_y >= graph_y1 + text_h * 2 &&
		zero_y < graph_y2 - text_h)
	{
		sprintf(string, "0");
		draw_text(MARGIN,
			zero_y + text_h / 2,
			string);
		draw_line(graph_x1 - MARGIN,
			zero_y,
			graph_x1,
			zero_y);
	}

// draw legend
	for(int i = 0; i < columns; i++)
	{
		int y = MARGIN + (text_h + MARGIN) * i;
		int x1 = legend_x;
		set_color(colors[i]);
		draw_box(legend_x, 
			y, 
			box_w, 
			text_h);
		set_font(MEDIUMFONT);
		char string[BCTEXTLEN];
		sprintf(string, "%d", i + 1);
		draw_text(legend_x + box_w + MARGIN,
			y + text_h,
			string);
	}

// the data
	for(int j = column1; j < column2; j++)
	{
		set_color(colors[j]);
		int x1 = graph_x1;
		int y1 = 0;
		for(int i = row1; i < row2; i++)
		{
            if(i >= 0)
            {
			    float value = data[i].data[j];
			    int x = row_to_x(i);
			    int y = graph_y2 - 1 - (value - min) * (graph_y2 - graph_y1 - 1) / (max - min);
			    if(i > row1)
			    {
				    draw_line(x1, y1, x, y);
			    }
			    y1 = y;
			    x1 = x;
            }
		}
	}

	draw_crosshair(0, 1);

	flash();

}



int64_t Loader::byte1 = 0;
int64_t Loader::byte2 = 0;

Loader::Loader()
 : Thread(1, 0, 0)
{
	started = new Condition(0);
	byte1 = -1;
	byte2 = -1;
}


void Loader::run()
{
// wait for the window
	started->lock();
	
	while(1)
	{
		int64_t new_modified = FileSystem::get_date(filename);
		if(new_modified != file_modified)
		{
			file_modified = new_modified;
			mwindow->lock_window("Loader::run");
			load_file();
			mwindow->draw_it();
			mwindow->unlock_window();
		}
		
        
        usleep(250000);
		//sleep(1);
	}
	
}

void Loader::load_file()
{
	FILE *fd = fopen(filename, "r");
	if(!fd)
	{
		printf("load_file %d: failed to load %s\n", __LINE__, filename);
		return;
	}

	if(data)
	{
		delete [] data;
		total_rows = 0;
	}
	
	data = new Row[16];
	rows_allocated = 16;

	if(binary_mode)
	{
		byte1 = byte1_arg;
		byte2 = byte2_arg;
		if(byte1 < 0)
		{
			byte1 = 0;
		}
		if(byte2 < 0)
		{
			struct stat ostat;
			stat(filename, &ostat);
			byte2 = ostat.st_size;
		}
		
		int column1 = column1_arg;
		int column2 = column2_arg;
		if(column1_arg < 0)
		{
			column1 = 0;
		}
		if(column2_arg < 0)
		{
			column2 = columns_arg;
		}
		
		
		fseek(fd, byte1, SEEK_SET);
		uint8_t buffer[BCTEXTLEN];
		int64_t offset = byte1;
		while(!feof(fd) && offset < byte2)
		{
			int bytes_read = fread(buffer, 1, BCTEXTLEN, fd);
			offset += BCTEXTLEN;
			
			if(bytes_read > 0)
			{
// scan each row
				uint8_t *ptr = buffer;
				for(int i = 0; 
					i < bytes_read; 
					i += columns_arg * bytes_per_number)
				{
					allocate_rows();
					Row *dst = &data[total_rows];
					total_rows++;
					dst->total = columns_arg;
// scan each column
					for(int j = 0; j < columns_arg; j++)
					{
						int16_t value = ptr[j * 2] | 
							(((uint16_t)ptr[j * 2 + 1]) << 8);
						dst->data[j] = value;
					}
					ptr += columns_arg * 2;
				}
			}
		}
	}
	else
	{
		char string[BCTEXTLEN];
		char string2[BCTEXTLEN];
		while(!feof(fd))
		{
			if(!fgets(string, BCTEXTLEN, fd)) break;

			allocate_rows();


			char *ptr1 = string;
			int column = 0;
			Row *dst = &data[total_rows];
			total_rows++;
			dst->total = 0;
			while(*ptr1 != 0 && column < MAX_COLUMNS)
			{
				while(*ptr1 != 0 && 
					*ptr1 == ' ' ||
					*ptr1 == ',' ||
					*ptr1 == '\t')
				{
					ptr1++;
				}

				if(*ptr1)
				{
					char *ptr2 = string2;
					while(*ptr1 != 0 &&
						*ptr1 != ',' &&
						*ptr1 != ' ' &&
						*ptr1 != '\t')
					{
						*ptr2 = *ptr1;
						ptr1++;
						ptr2++;
					}
					*ptr2 = 0;

					dst->data[column] = atof(string2);
					dst->total++;
					column++;
				}
			}		
		}
	}
	
	fclose(fd);
}



int64_t atobyte(char *text)
{
	int64_t result;
	if(strlen(text) > 2 &&
		text[0] == '0' &&
		text[1] == 'x')
	{
		sscanf(text, "%lx", &result);
	}
	else
	{
		result = atol(text);
	}
	return result;
}

int main(int argc, char *argv[])
{
	if(argc < 2)
	{
		printf("Usage: %s <filename>\n", argv[0]);
		printf("Graph data from a file with continuous polling.\n");
		printf("Text mode columns are separated by spaces, tabs, or commas.\n\n");
		printf("Binary mode requires setting the total columns.\n\n");
		printf(" -c1 - 1st column + 1.\n");
		printf(" -c2 - 2nd column + 1\n");
		printf(" -c - total columns for binary mode.  Not used in text mode.\n");
		printf(" -r1 - 1st row + 1.\n");
		printf(" -r2 - last row + 1\n");
		printf(" -int16 - Use binary mode & parse int16's instead of text\n");
		printf(" -b1 - 1st byte in binary mode.\n");
		printf(" -b2 - last byte in binary mode\n\n");
		printf("In the GUI, f fits the scale to the visible rows.\n");
		printf("\nExamples:\n");
		printf("Graph 2 columns from the first 1024 lines\n");
		printf("\tgrapher /tmp/x -c1 1 -c2 2 -r2 1024\n");
		printf("Graph 2 columns from lines 10 to 1024\n");
		printf("\tgrapher /tmp/x -c1 1 -c2 2 -r1 10 -r2 1024\n");
		printf("Graph 2 columns from the last 1024 lines\n");
		printf("\tgrapher /tmp/x -c1 1 -c2 2 -r1 -1024\n");
		printf("Graph 2 columns from the last 1024 lines to the last 5 lines\n");
		printf("\tgrapher /tmp/x -c1 1 -c2 2 -r1 -1024 -r2 -5\n");
		printf("Graph 2 columns from the entire file\n");
		printf("\tgrapher /tmp/x -c1 1 -c2 2\n");
		printf("Graph 2 columns from int16 binary data.\n");
		printf("\tgrapher ~/terminal.cap -b1 0xda -b2 0x10da -c 2\n");
		
		exit(1);
	}

	filename[0] = 0;
	defaults = new BC_Hash("~/.grapher");
	defaults->load();
	window_x = defaults->get("WINDOW_X", window_x);
	window_y = defaults->get("WINDOW_Y", window_y);
	window_w = defaults->get("WINDOW_W", window_w);
	window_h = defaults->get("WINDOW_H", window_h);
	save_defaults();
	
	int i;
	for(i = 1; i < argc; i++)
	{	
		if(!strcmp(argv[i], "-int16"))
		{
			binary_mode = BINARY_INT16;
		}
		else
		if(!strcmp(argv[i], "-b1"))
		{
			binary_mode = BINARY_INT16;
			i++;
			if(i < argc)
			{
				byte1_arg = atobyte(argv[i]);
			}
			else
			{
				printf("%s needs another argument\n", argv[i - 1]);
				exit(1);
			}
		}
		else
		if(!strcmp(argv[i], "-b2"))
		{
			binary_mode = BINARY_INT16;
			i++;
			if(i < argc)
			{
				byte2_arg = atobyte(argv[i]);
			}
			else
			{
				printf("%s needs another argument\n", argv[i - 1]);
				exit(1);
			}
		}
		else
		if(!strcmp(argv[i], "-c"))
		{
			i++;
			if(i < argc)
			{
				columns_arg = atoi(argv[i]);
			}
			else
			{
				printf("%s needs another argument\n", argv[i - 1]);
				exit(1);
			}
		}
		else
		if(!strcmp(argv[i], "-c1"))
		{
			i++;
			if(i < argc)
			{
				column1_arg = atoi(argv[i]) - 1;
			}
			else
			{
				printf("%s needs another argument\n", argv[i - 1]);
				exit(1);
			}
		}
		else
		if(!strcmp(argv[i], "-c2"))
		{
			i++;
			if(i < argc)
			{
				column2_arg = atoi(argv[i]);
			}
			else
			{
				printf("%s needs another argument\n", argv[i - 1]);
				exit(1);
			}
		}
		else
		if(!strcmp(argv[i], "-r1"))
		{
			i++;
			if(i < argc)
			{
				row1_arg = atoi(argv[i]) - 1;
				have_row1 = 1;
			}
			else
			{
				printf("%s needs another argument\n", argv[i - 1]);
				exit(1);
			}
		}
		else
		if(!strcmp(argv[i], "-r2"))
		{
			i++;
			if(i < argc)
			{
				row2_arg = atoi(argv[i]);
				have_row2 = 1;
			}
			else
			{
				printf("%s needs another argument\n", argv[i - 1]);
				exit(1);
			}
		}
		else
		if(filename[0] == 0)
		{
			strcpy(filename, argv[i]);
		}
		else
		{
			printf("Unrecognized option %s\n", argv[i]);
			exit(1);
		}
	}

	if(binary_mode && columns_arg < 0)
	{
		printf("Must set the total columns in binary mode.\n");
		exit(1);
	}

//	load_file();
//	dump();
	loader = new Loader();
	loader->start();

	mwindow = new MWindow();
	mwindow->create_objects();
	mwindow->run_window();
}



