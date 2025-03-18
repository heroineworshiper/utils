// HDR with floating point images
// Copyright (C) 2025 Adam Williams <broadcast at earthling dot net>
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA



// g++ -std=c++11 -o hdr hdr.C -I../countreps/include/ -ltiff `PKG_CONFIG_PATH=/root/countreps/lib/pkgconfig/ pkg-config opencv --libs`

// LD_LIBRARY_PATH=/root/countreps/lib/ ./hdr

// LD_LIBRARY_PATH=/root/countreps/lib/ ./hdr /tmp/mark3000486.tif \
// /tmp/mark3000487.tif /tmp/mark3000489.tif /tmp/mark3000488.tif \
// /tmp/mark3000491.tif /tmp/mark3000490.tif /tmp/mark3000492.tif \
// mark3.tif

#include <opencv2/opencv.hpp>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <tiffio.h>
#include <vector>

using namespace cv;
using namespace std;

#define MAX_COLOR 128

vector<const char*> input_paths;



//vector<float> exposures({ 1, 2, 4, 8, 16, 32, 64 });

const char *output_path = 0;

vector<Mat> input_images;
Mat output_image;

uint8_t *compressed_data = 0;
int compressed_size = 0;
int compressed_allocated = 0;
int compressed_offset = 0;
int w;
int h;
int components;
int bitspersample;
int sampleformat;


static tsize_t tiff_read(thandle_t ptr, tdata_t buf, tsize_t size)
{
	if(compressed_size < compressed_offset + size)
		return 0;
	memcpy(buf, compressed_data + compressed_offset, size);
	compressed_offset += size;
	return size;
}

static tsize_t tiff_write(thandle_t ptr, tdata_t buf, tsize_t size)
{
	if(compressed_allocated < compressed_offset + size)
	{
        compressed_allocated = (compressed_offset + size) * 2;
        compressed_data = (uint8_t*)realloc(compressed_data, compressed_allocated);
	}


	if(compressed_size < compressed_offset + size)
		compressed_size = compressed_offset + size;
	memcpy(compressed_data + compressed_offset,
		buf,
		size);
	compressed_offset += size;
	return size;
}

static toff_t tiff_seek(thandle_t ptr, toff_t off, int whence)
{
	switch(whence)
	{
		case SEEK_SET:
			compressed_offset = off;
			break;
		case SEEK_CUR:
			compressed_offset += off;
			break;
		case SEEK_END:
			compressed_offset = compressed_size + off;
			break;
	}
	return compressed_offset;
}

static int tiff_close(thandle_t ptr)
{
	return 0;
}

static toff_t tiff_size(thandle_t ptr)
{
	return compressed_size;
}

static int tiff_mmap(thandle_t ptr, tdata_t* pbase, toff_t* psize)
{
	*pbase = compressed_data;
	*psize = compressed_size;
	return 0;
}

void tiff_unmap(thandle_t ptr, tdata_t base, toff_t size)
{
}


Mat read_frame(const char *path)
{ 
    TIFF *stream;
    FILE *fd = fopen(path, "r");
    Mat dummy;
    if(!fd)
    {
        printf("Couldn't open %s for reading\n", path);
        return dummy;
    }
    fseek(fd, 0, SEEK_END);
    compressed_size = ftell(fd);
    fseek(fd, 0, SEEK_SET);
    compressed_data = (uint8_t*)malloc(compressed_size);
    fread(compressed_data, compressed_size, 1, fd);
    fclose(fd);
    compressed_offset = 0;
    stream = TIFFClientOpen("the_file", 
		"r",
	    (void*)0,
	    tiff_read, 
		tiff_write,
	    tiff_seek, 
		tiff_close,
	    tiff_size,
	    tiff_mmap, 
		tiff_unmap);
	TIFFGetField(stream, TIFFTAG_IMAGEWIDTH, &w);
	TIFFGetField(stream, TIFFTAG_IMAGELENGTH, &h);
	components = 0;
	TIFFGetField(stream, TIFFTAG_SAMPLESPERPIXEL, &components);
	bitspersample = 0;
	TIFFGetField(stream, TIFFTAG_BITSPERSAMPLE, &bitspersample);
	sampleformat = 0;
	TIFFGetField(stream, TIFFTAG_SAMPLEFORMAT, &sampleformat);

    printf("read_frame %d: %s bits=%d components=%d\n",
        __LINE__,
        path,
        bitspersample,
        components);
    Mat result(h, w, CV_32FC3);
    float *ptr = (float*)result.ptr(0);
    for(int i = 0; i < h; i++)
	{
// scale the input to 0-255
//        for(int j = 0; j < w * 3; j++) ptr[i * w * 3 + j] *= 255;
		TIFFReadScanline(stream, ptr + i * w * 3, i, 0);
    }
	TIFFClose(stream);
    free(compressed_data);
    compressed_data = 0;
    return result;
}

void write_frame(const char *path)
{
    TIFF *stream;
    compressed_size = 0;
    compressed_data = 0;
    compressed_offset = 0;
    stream = TIFFClientOpen("the_file", 
		"w",
	    (void*)0,
	    tiff_read, 
		tiff_write,
	    tiff_seek, 
		tiff_close,
	    tiff_size,
	    tiff_mmap, 
		tiff_unmap);
	TIFFSetField(stream, TIFFTAG_IMAGEWIDTH, w);
	TIFFSetField(stream, TIFFTAG_IMAGELENGTH, h);
	TIFFSetField(stream, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
	TIFFSetField(stream, TIFFTAG_SAMPLESPERPIXEL, 3);
	TIFFSetField(stream, TIFFTAG_BITSPERSAMPLE, 32);
    TIFFSetField(stream, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP);
	TIFFSetField(stream, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
	TIFFSetField(stream, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
 	TIFFSetField(stream, TIFFTAG_ROWSPERSTRIP, 
 		TIFFDefaultStripSize(stream, (uint32_t)-1));
//  	TIFFSetField(stream, TIFFTAG_ROWSPERSTRIP, 
// 		(8 * 1024) / bytesperrow);
	TIFFSetField(stream, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);

    float *ptr = (float*)output_image.ptr(0);
    for(int i = 0; i < h; i++)
    {
// scale the output to 0-255
        for(int j = 0; j < w * 3; j++) ptr[i * w * 3 + j] *= MAX_COLOR;
        TIFFWriteScanline(stream, 
            ptr + i * w * 3, 
            i, 
            0);
    }

	TIFFClose(stream);

    FILE *fd = fopen(path, "w");
    if(!fd) printf("Couldn't open %s for writing\n", path);
    fwrite(compressed_data, compressed_size, 1, fd);
    fclose(fd);
    
    free(compressed_data);
    compressed_data = 0;
}


int main(int argc, char *argv[])
{
    if(argc < 4)
    {
        printf("Usage: hdr <input files> <output file>\n");
        exit(1);
    }
    
    for(int i = 1; i < argc - 1; i++)
    {
        input_paths.push_back(argv[i]);
    }
    output_path = argv[argc - 1];
    FILE *fd = fopen(output_path, "r");
    if(fd)
    {
        fclose(fd);
        printf("Overwrite output file %s?\n", output_path);
        char c = fgetc(stdin);
        if(c != 'y')
        {
            printf("Giving up & going to a movie.\n");
            return 1;
        }
    }

    for(int i = 0; i < input_paths.size(); i++)
        input_images.push_back(read_frame(input_paths.at(i)));
    printf("main %d: processing\n", __LINE__);

// 8 bit only
//    Ptr<MergeDebevec> merge = createMergeDebevec();
//    Ptr<MergeRobertson> merge = createMergeRobertson();
//    merge->process(input_images, output_image, exposures);

// floating point
    Ptr<MergeMertens> merge_mertens = createMergeMertens();
    merge_mertens->process(input_images, output_image);
    printf("main %d: writing %s\n", __LINE__, output_path);
    write_frame(output_path);
}

