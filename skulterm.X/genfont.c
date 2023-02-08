// generate the font data from font.png

// gcc -o genfont genfont.c -lpng
// ./genfont > font.c

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <png.h>

#define PATH "font.png"
#define GLYPH_W 8
#define GLYPH_H 8


void main(int argc, char *argv[])
{
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
	png_infop info_ptr = png_create_info_struct(png_ptr);
	FILE *fd = fopen(PATH, "r");
    if(!fd)
    {
        printf("Couldn't open %s\n", PATH);
    }
    png_init_io(png_ptr, fd);
    png_read_info(png_ptr, info_ptr);

    int w = png_get_image_width(png_ptr, info_ptr);
	int h = png_get_image_height(png_ptr, info_ptr);
	int color_model = png_get_color_type(png_ptr, info_ptr);



    int bytes_per_pixel = 3;
	switch(color_model)
	{
		case PNG_COLOR_TYPE_RGB:
			bytes_per_pixel = 3;
			break;


		case PNG_COLOR_TYPE_GRAY_ALPHA:
		case PNG_COLOR_TYPE_RGB_ALPHA:
		default:
			bytes_per_pixel = 4;
			break;
	}

//    printf("color_model=%d w=%d h=%d bytes_per_pixel=%d\n", color_model, w, h, bytes_per_pixel);
    uint8_t *data = malloc(w * h * bytes_per_pixel);
    uint8_t **rows = malloc(sizeof(uint8_t*) * h);
    for(int i = 0; i < h; i++)
    {
        rows[i] = data + i * w * bytes_per_pixel;
    }


	png_read_image(png_ptr, rows);


// convert pixels to bits
    uint8_t *temp = malloc(h);
    for(int i = 0; i < h; i++)
    {
        uint8_t value = 0;
        for(int j = 0; j < 8; j++)
        {
            value <<= 1;
            if(rows[i][j * bytes_per_pixel] > 128)
            {
                value |= 1;
            }
        }
        temp[i] = value;
    }

    printf("asm(\n"
	    "\t\".section .text,code\\n\"\n"
	    "\t\".global _font\\n\"\n"
	    "\t\"_font:\\n\"\n"
        "\t\".pbyte ");

    for(int i = 0; i < h; i++)
    {
        if(i < h - 1 && ((i + 1) % 16) == 0)
        {
            printf("0x%02x\\n\"\n\t\".pbyte ", temp[i]);
        }
        else
        if(i < h - 1)
        {
            printf("0x%02x, ", temp[i]);
        }
        else
        {
            printf("0x%02x\\n\"\n", temp[i]);
        }
    }
    printf(");\n\n\n");

    
    
}
