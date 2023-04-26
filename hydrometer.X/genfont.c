// generate the font data from font.png

// gcc -o genfont genfont.c -lpng
// ./genfont > font.c
// generate the rotated font for the upside down display
// ./genfont r > font2.c

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <png.h>

#define PATH "font.png"
#define GLYPH_W 240
#define GLYPH_H 320


void main(int argc, char *argv[])
{
    int rotate = 0;
    if(argc > 1 && !strcmp(argv[1], "r"))
    {
        rotate = 1;
    }


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

#define MAX_SIZE (GLYPH_W * GLYPH_H * bytes_per_pixel)
    uint8_t *temp = malloc(MAX_SIZE);

    for(int digit = 0; digit < 10; digit++)
    {
        int x = digit * GLYPH_W;

        if(rotate)
        {
            for(int i = 0; i < GLYPH_H; i++)
            {
                uint8_t *src = data + 
                    i * w * bytes_per_pixel + 
                    x * bytes_per_pixel;
                uint8_t *dst = temp +
                    (GLYPH_H - i - 1) * GLYPH_W * bytes_per_pixel +
                    (GLYPH_W - 1) * bytes_per_pixel;
                for(int j = 0; j < GLYPH_W; j++)
                {
                    dst[0] = src[0];
                    dst[1] = src[1];
                    dst[2] = src[2];
                    dst[3] = src[3];
                    dst -= bytes_per_pixel;
                    src += bytes_per_pixel;
                }
            }

            for(int i = 0; i < GLYPH_H; i++)
            {
                uint8_t *src = temp + i * GLYPH_W * bytes_per_pixel;
                uint8_t *dst = data + 
                    i * w * bytes_per_pixel + 
                    x * bytes_per_pixel;
                memcpy(dst, src, GLYPH_W * bytes_per_pixel);
            }
        }

        uint8_t *dst = temp;
// the pixel value
        int value = 0;
// number of repeats - 1
        int length = 0;
        int start = 1;
        for(int i = 0; i < GLYPH_H; i++)
        {
            uint8_t *src = data + 
                i * w * bytes_per_pixel + 
                x * bytes_per_pixel;
            for(int j = 0; j < GLYPH_W; j++)
            {
                int new_value = 0;
                if(*src > 128)
                {
                    new_value = 1;
                }
                src += bytes_per_pixel;


                if(start)
                {
// 1st pixel
                    value = new_value;
                    length = 0;
                    start = 0;
                }
                else
                if(new_value != value || length >= 127)
                {
// value changed or length hit maximum
                    *dst++ = (value << 7) | length;
                    value = new_value;
                    length = 0;
                }
                else
                {
// same value repeated
                    length++;
                }
            }
        }

// final code
        *dst++ = (value << 7) | length;
        int size = dst - temp;

        printf("asm(\n"
	        "\t\".section .text,code\\n\"\n"
	        "\t\".global _digit%d\\n\"\n"
	        "\t\"_digit%d:\\n\"\n"
            "\t\".pbyte ", digit, digit);

        for(int i = 0; i < size; i++)
        {
            if(i < size - 1 && ((i + 1) % 16) == 0)
            {
                printf("0x%02x\\n\"\n\t\".pbyte ", temp[i]);
            }
            else
            if(i < size - 1)
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
    
    
    
}
