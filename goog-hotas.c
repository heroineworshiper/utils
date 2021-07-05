/*
 * Goog Hotas
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



// convert Thrustmaster T Flight Hotas to key strokes for Goog Earth
// It uses the /dev/uhid user HID device to create a virtual HID keyboard & mouse.
// It uses a screencapture to read the graphical positions of the controls.

// gcc -o goog-hotas goog-hotas.c -lpthread -lX11 -lXext
// install the uhid.ko kernel module

// based on the keyboard example using the HID gadget driver
// https://www.kernel.org/doc/html/latest/usb/gadget_hid.html
// & the mouse example using the UHID driver
// /usr/src/linux-4.9.39/samples/uhid/uhid-example.c

// Start with the flight simulator paused.
// Position the capture window with the console interface.
// Click in the window to grab the pointer.
// Move the mouse to trim roll/pitch.

// joystick mappings:
// X - trim pitch down
// O - trim pitch up
// SHARE, OPTIONS - brakes
// throttle Y - throttle
// throttle X - trim yaw
// R2 - flaps up
// L2 - flaps down
// fire button - gear
// hat switch - look around
// joystick X - roll
// joystick Y - pitch
// joystick Z - yaw
// RED LED - pause





#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <linux/uhid.h>
#include <signal.h>
#include <pthread.h>
#include <X11/Xutil.h>
#include <sys/shm.h>
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

#define TEXTLEN 1024

// use keys for roll/pitch
//#define USE_KEYS

#define FRAMERATE 60
#define MIN_ANALOG 0
#define MAX_ANALOG 32
#define MIN_MOUSE 200
#define MAX_MOUSE 4000
#define MIN_W 320
#define MAX_W 1024
#define MIN_H 240
#define MAX_H 1024
#define REPORT_SIZE 8
#define ESC 0x1b
#define PREFS_FILE ".goog-hotas.rc"

#define CLAMP(x, y, z) ((x) = ((x) < (y) ? (y) : ((x) > (z) ? (z) : (x))))

// keyboard report descriptor
static unsigned char keyboard_rdesc[] = 
{
    0x05, 0x01,     /* USAGE_PAGE (Generic Desktop)           */
    0x09, 0x06,     /* USAGE (Keyboard)                       */
    0xa1, 0x01,     /* COLLECTION (Application)               */
    0x05, 0x07,     /*   USAGE_PAGE (Keyboard)                */
    0x19, 0xe0,     /*   USAGE_MINIMUM (Keyboard LeftControl) */
    0x29, 0xe7,     /*   USAGE_MAXIMUM (Keyboard Right GUI)   */
    0x15, 0x00,     /*   LOGICAL_MINIMUM (0)                  */
    0x25, 0x01,     /*   LOGICAL_MAXIMUM (1)                  */
    0x75, 0x01,     /*   REPORT_SIZE (1)                      */
    0x95, 0x08,     /*   REPORT_COUNT (8)                     */
    0x81, 0x02,     /*   INPUT (Data,Var,Abs)                 */
    0x95, 0x01,     /*   REPORT_COUNT (1)                     */
    0x75, 0x08,     /*   REPORT_SIZE (8)                      */
    0x81, 0x03,     /*   INPUT (Cnst,Var,Abs)                 */
    0x95, 0x05,     /*   REPORT_COUNT (5)                     */
    0x75, 0x01,     /*   REPORT_SIZE (1)                      */
    0x05, 0x08,     /*   USAGE_PAGE (LEDs)                    */
    0x19, 0x01,     /*   USAGE_MINIMUM (Num Lock)             */
    0x29, 0x05,     /*   USAGE_MAXIMUM (Kana)                 */
    0x91, 0x02,     /*   OUTPUT (Data,Var,Abs)                */
    0x95, 0x01,     /*   REPORT_COUNT (1)                     */
    0x75, 0x03,     /*   REPORT_SIZE (3)                      */
    0x91, 0x03,     /*   OUTPUT (Cnst,Var,Abs)                */
    0x95, 0x06,     /*   REPORT_COUNT (6)                     */
    0x75, 0x08,     /*   REPORT_SIZE (8)                      */
    0x15, 0x00,     /*   LOGICAL_MINIMUM (0)                  */
    0x25, 0x65,     /*   LOGICAL_MAXIMUM (101)                */
    0x05, 0x07,     /*   USAGE_PAGE (Keyboard)                */
    0x19, 0x00,     /*   USAGE_MINIMUM (Reserved)             */
    0x29, 0x65,     /*   USAGE_MAXIMUM (Keyboard Application) */
    0x81, 0x00,     /*   INPUT (Data,Ary,Abs)                 */
    0xc0            /* END_COLLECTION                         */
};

static unsigned char mouse_rdesc[] = {
	0x05, 0x01,	/* USAGE_PAGE (Generic Desktop) */
	0x09, 0x02,	/* USAGE (Mouse) */
	0xa1, 0x01,	/* COLLECTION (Application) */
	0x09, 0x01,		/* USAGE (Pointer) */
	0xa1, 0x00,		/* COLLECTION (Physical) */
	0x85, 0x01,			/* REPORT_ID (1) */
	0x05, 0x09,			/* USAGE_PAGE (Button) */
	0x19, 0x01,			/* USAGE_MINIMUM (Button 1) */
	0x29, 0x03,			/* USAGE_MAXIMUM (Button 3) */
	0x15, 0x00,			/* LOGICAL_MINIMUM (0) */
	0x25, 0x01,			/* LOGICAL_MAXIMUM (1) */
	0x95, 0x03,			/* REPORT_COUNT (3) */
	0x75, 0x01,			/* REPORT_SIZE (1) */
	0x81, 0x02,			/* INPUT (Data,Var,Abs) */
	0x95, 0x01,			/* REPORT_COUNT (1) */
	0x75, 0x05,			/* REPORT_SIZE (5) */
	0x81, 0x01,			/* INPUT (Cnst,Var,Abs) */
	0x05, 0x01,			/* USAGE_PAGE (Generic Desktop) */
	0x09, 0x30,			/* USAGE (X) */
	0x09, 0x31,			/* USAGE (Y) */
	0x09, 0x38,			/* USAGE (WHEEL) */
	0x15, 0x81,			/* LOGICAL_MINIMUM (-127) */
	0x25, 0x7f,			/* LOGICAL_MAXIMUM (127) */
	0x75, 0x08,			/* REPORT_SIZE (8) */
	0x95, 0x03,			/* REPORT_COUNT (3) */
	0x81, 0x06,			/* INPUT (Data,Var,Rel) */
	0xc0,			/* END_COLLECTION */
	0xc0,		/* END_COLLECTION */
};

// key codes
#define SPACE 0x2c
#define PERIOD 0x37
#define COMMA 0x36
// modifiers
#define CTRL_MOD 0x01
#define SHIFT 0x02
#define PGUP 0x4b
#define PGDN 0x4e
#define HOME 0x4a
#define END 0x4d
#define UP 0x52
#define DN 0x51
#define RIGHT 0x4f
#define LEFT 0x50
#define KP_ENTER 0x58
#define KP_INS 0x49

// button to keyboard table
typedef struct 
{
    int offset;
    int mask;
    uint8_t code;
    uint8_t modifier;
    int is_down;
} button_t;


button_t buttons[] = 
{
// fire button -> gear
    { 6, 0x2, 'g' - ('a' - 0x4), 0, 0 },
// L2 -> flaps down
    { 6, 0x4, 'f' - ('a' - 0x4), 0, 0 },
// R2 - > flaps up
    { 6, 0x8, 'f' - ('a' - 0x4), SHIFT, 0 },
// LED button -> pause
    { 7, 0x1, SPACE, 0, 0 },
// options -> right brake
    { 6, 0x20, PERIOD, 0, 0 },
// share -> left brake
    { 6, 0x10, COMMA, 0, 0 },
// O - pitch trim down
    { 5, 0x40, END, 0, 0 },
// X - pitch trim up
    { 5, 0x20, HOME, 0, 0 },


// HAT left - look left
    { 63, 0x01, LEFT, CTRL_MOD, 0 },
// HAT right - look right
    { 63, 0x02, RIGHT, CTRL_MOD, 0 },
// HAT up - look up
    { 63, 0x04, UP, CTRL_MOD, 0 },
// HAT down - look down
    { 63, 0x08, DN, CTRL_MOD, 0 }
};
#define TOTAL_BUTTONS (sizeof(buttons) / sizeof(button_t))


int key_fd;
int mouse_fd;
uint8_t **row_data;


// persistent report state
uint8_t report_mod = 0;
// up to 2 persistent keys
uint8_t report_key[2] = { 0 };
#define REPORT_KEYS (sizeof(report_key) / sizeof(uint8_t))


// analog values
int throttle = MIN_ANALOG;
int yaw = (MIN_ANALOG + MAX_ANALOG) / 2;
int yaw_trim = -1;
int got_yaw_trim = 0;
// if the HUD was detected
int have_throttle = 0;
int have_yaw = 0;

int want_throttle = MIN_ANALOG;
#ifdef USE_KEYS
int pitch = (MIN_ANALOG + MAX_ANALOG) / 2;
int roll = (MIN_ANALOG + MAX_ANALOG) / 2;
int want_pitch = (MIN_ANALOG + MAX_ANALOG) / 2;
int want_roll = (MIN_ANALOG + MAX_ANALOG) / 2;
#else
int pitch = 0;
int roll = 0;
int want_pitch = 0;
int want_roll = 0;
#endif

int want_yaw = (MIN_ANALOG + MAX_ANALOG) / 2;

// bitmap extents
int throttle_right = 0;
int pitch_left = 0;
int capture_x = 50;
int capture_y = 625;
int capture_w = 320;
int capture_h = 240;
int mouse_range = 3000;
int root_w = 3840;
int root_h = 2160;
Display *display;
Window left;
Window right;
Window top;
Window bottom;
pthread_mutex_t settings_lock;

// kernel interface
static int create_keyboard()
{
	struct uhid_event ev;

    printf("Creating Thrustmaster Keyboard\n");

	const char *path = "/dev/uhid";
	key_fd = open(path, O_RDWR | O_CLOEXEC);
	if (key_fd < 0) {
		printf("create_keyboard %d: Cannot open %s\n", __LINE__, path);
		exit(0);
	}


	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_CREATE;
	strcpy((char*)ev.u.create.name, "Thrustmaster Keyboard");
	ev.u.create.rd_data = keyboard_rdesc;
	ev.u.create.rd_size = sizeof(keyboard_rdesc);
	ev.u.create.bus = BUS_USB;
	ev.u.create.vendor = 0x15d9;
	ev.u.create.product = 0x0a37;
	ev.u.create.version = 0;
	ev.u.create.country = 0;

    if(write(key_fd, &ev, sizeof(ev)) < sizeof(ev))
    {
        printf("create_keyboard %d: create failed\n", __LINE__);
        perror("");
    }
}

// kernel interface
static int create_mouse()
{
	struct uhid_event ev;

    printf("Creating Thrustmaster Mouse\n");

	const char *path = "/dev/uhid";
	mouse_fd = open(path, O_RDWR | O_CLOEXEC);
	if (mouse_fd < 0) {
		printf("create_mouse %d: Cannot open %s\n", __LINE__, path);
		exit(0);
	}


	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_CREATE;
	strcpy((char*)ev.u.create.name, "Thrustmaster Mouse");
	ev.u.create.rd_data = mouse_rdesc;
	ev.u.create.rd_size = sizeof(mouse_rdesc);
	ev.u.create.bus = BUS_USB;
	ev.u.create.vendor = 0x15d9;
	ev.u.create.product = 0x0a38;
	ev.u.create.version = 0;
	ev.u.create.country = 0;

    if(write(mouse_fd, &ev, sizeof(ev)) < sizeof(ev))
    {
        printf("create_mouse %d: create failed\n", __LINE__);
        perror("");
    }
}

static void destroy()
{
	struct uhid_event ev;

    printf("Destroying user HID devices\n");
	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_DESTROY;

    write(key_fd, &ev, sizeof(ev));
    write(mouse_fd, &ev, sizeof(ev));

// reset the console
	struct termios info;
	tcgetattr(fileno(stdin), &info);
	info.c_lflag |= ICANON;
	info.c_lflag |= ECHO;
	tcsetattr(fileno(stdin), TCSANOW, &info);
}

void sig_catch(int sig)
{
	destroy();
    exit(0);
}

void send_keyboard(uint8_t *report, int size)
{
	struct uhid_event ev;
    bzero(&ev, sizeof(ev));
	ev.type = UHID_INPUT;
	ev.u.input.size = size;
    memcpy(ev.u.input.data, report, size);
    int result = write(key_fd, &ev, sizeof(ev));
//    printf("send_keyboard %d: result=%d\n", __LINE__, result);
    if(result < 0)
    {
        printf("send_keyboard %d write failed\n", __LINE__);
        perror("");
    }
}

void send_mouse(uint8_t *report, int size)
{
	struct uhid_event ev;
    bzero(&ev, sizeof(ev));
	ev.type = UHID_INPUT;
	ev.u.input.size = size;
    memcpy(ev.u.input.data, report, size);
    int result = write(mouse_fd, &ev, sizeof(ev));
//    printf("send_mouse %d: result=%d\n", __LINE__, result);
    if(result < 0)
    {
        printf("send_mouse %d write failed\n", __LINE__);
        perror("");
    }
}


// bitmap scanning routines
void detect_throttle()
{
    int left_edge = -1;
    int right_edge = -1;
    int top_edge = capture_h;
    int bottom_edge = 0;
    int range_top = -1;
    int range_bottom = -1;
    for(int x = 1; x < capture_w; x++)
    {
        for(int y = 1; y < capture_h; y++)
        {
            uint8_t *pixel = row_data[y] + x;
            if(*pixel)
            {
// range is based on top left corner
                if(left_edge < 0)
                {
                    left_edge = x;
                    range_top = y + 1;
                }


                right_edge = x;
// range is based on leftmost column
                if(right_edge == left_edge)
                {
                    range_bottom = y - 1;
                }

                if(y < top_edge)
                {
                    top_edge = y;
                }
                if(y > bottom_edge)
                {
                    bottom_edge = y;
                }
            }
        }

// went a column without finding anything
        if(left_edge >= 0 &&
            x > right_edge)
        {
            break;
        }
    }

    throttle_right = right_edge;

// printf("detect_throttle %d: left=%d right=%d top=%d bottom=%d range_top=%d range_bottom=%d\n",
// __LINE__,
// left_edge,
// right_edge,
// top_edge,
// bottom_edge,
// range_top,
// range_bottom);

    if(left_edge < right_edge &&
        range_bottom > range_top)
    {
// detect throttle position by searching for longest vertical line
        int line_bottom = -1;
        int line_top = -1;
        for(int x = (left_edge + right_edge) / 2; x <= right_edge; x++)
        {
            int line_bottom2 = -1;
            int line_top2 = -1;
            for(int y = top_edge; y <= bottom_edge; y++)
            {
                uint8_t *pixel = row_data[y] + x;
                if(*pixel)
                {
// got a line
                    if(line_top2 < 0)
                    {
                        line_top2 = y;
                    }
                    line_bottom2 = y + 1;
                    if(line_bottom2 - line_top2 > line_bottom - line_top)
                    {
// new longest line
                        line_top = line_top2;
                        line_bottom = line_bottom2;
                    }
                }
                else
                {
// line ended
// reset next line
                    line_bottom2 = -1;
                    line_top2 = -1;
                }
            }

        }
        int line_position = (line_top + line_bottom) / 2;
        throttle = (range_bottom - line_position) * 
            (MAX_ANALOG - MIN_ANALOG) / 
            (range_bottom - range_top);
        CLAMP(throttle, MIN_ANALOG, MAX_ANALOG);
        have_throttle = 1;


//         printf("detect_throttle %d: line_top=%d line_bottom=%d throttle=%d\n", 
//             __LINE__, 
//             line_top,
//             line_bottom,
//             throttle);
    }
    else
    {
        have_throttle = 0;
    }
}


void detect_pitch()
{
    int left_edge = -1;
    int right_edge = -1;
    int top_edge = capture_h;
    int bottom_edge = 0;
    int range_top = -1;
    int range_bottom = -1;
    for(int x = capture_w - 1; x >= 0; x--)
    {
        int range_top2 = -1;
        int range_bottom2 = -1;
        for(int y = 1; y < capture_h; y++)
        {
            uint8_t *pixel = row_data[y] + x;
            if(*pixel)
            {
                if(right_edge < 0)
                {
                    right_edge = x;
                }

// range uses top left corner
                if(range_top2 < 0)
                {
                    range_top2 = y + 1;
                }
// range uses bottom left corner
                range_bottom2 = y - 1;
                left_edge = x;

                if(y < top_edge)
                {
                    top_edge = y;
                }
                if(y > bottom_edge)
                {
                    bottom_edge = y;
                }
            }
        }

// went a column without finding anything
        if(right_edge >= 0 &&
            x < left_edge)
        {
            break;
        }
        else
        {
// range is last row
            range_top = range_top2;
            range_bottom = range_bottom2;
        }
    }

    pitch_left = left_edge;
    
// printf("detect_pitch %d: left=%d right=%d top=%d bottom=%d range_top=%d range_bottom=%d\n",
// __LINE__,
// left_edge,
// right_edge,
// top_edge,
// bottom_edge,
// range_top,
// range_bottom);

    if(left_edge < right_edge &&
        range_bottom > range_top)
    {
// detect throttle position by searching for longest vertical line
        int line_bottom = -1;
        int line_top = -1;
        for(int x = (left_edge + right_edge) / 2; x <= right_edge; x++)
        {
            int line_bottom2 = -1;
            int line_top2 = -1;
            for(int y = top_edge; y <= bottom_edge; y++)
            {
                uint8_t *pixel = row_data[y] + x;
                if(*pixel)
                {
// got a line
                    if(line_top2 < 0)
                    {
                        line_top2 = y;
                    }
                    line_bottom2 = y + 1;
                    if(line_bottom2 - line_top2 > line_bottom - line_top)
                    {
                        line_top = line_top2;
                        line_bottom = line_bottom2;
                    }
                }
                else
                {
// line ended
// reset next line
                    line_bottom2 = -1;
                    line_top2 = -1;
                }
            }

        }
        int line_position = (line_top + line_bottom) / 2;

#ifdef USE_KEYS
        pitch = (range_bottom - line_position) * 
            (MAX_ANALOG - MIN_ANALOG) / 
            (range_bottom - range_top);
        CLAMP(pitch, MIN_ANALOG, MAX_ANALOG);
#endif


//         printf("detect_pitch %d: line_top=%d line_bottom=%d pitch=%d\n", 
//             __LINE__, 
//             line_top,
//             line_bottom,
//             pitch);
    }
}

void detect_roll()
{
    int left_edge = capture_w;
    int right_edge = -1;
    int top_edge = -1;
    int bottom_edge = 0;
    int range_left = -1;
    int range_right = -1;
    
    for(int y = 1; y < capture_h; y++)
    {
        for(int x = throttle_right + 1; x < pitch_left; x++)
        {
            uint8_t *pixel = row_data[y] + x;
            if(*pixel)
            {
                if(top_edge < 0)
                {
                    top_edge = y;
// range is top left corner
                    range_left = x + 1;
                }
                
                bottom_edge = y;
                if(bottom_edge == top_edge)
                {
// range is top right corner
                    range_right = x - 1;
                }
                
                if(x < left_edge)
                {
                    left_edge = x;
                }
                if(x > right_edge)
                {
                    right_edge = x;
                }
            }
        }

// went a row without finding anything
        if(top_edge >= 0 &&
            y > bottom_edge)
        {
            break;
        }
    }


// printf("detect_roll %d: left=%d right=%d top=%d bottom=%d\n",
// __LINE__,
// left_edge,
// right_edge,
// top_edge,
// bottom_edge);

    if(top_edge < bottom_edge &&
        range_right > range_left)
    {
// detect position by searching for longest horizontal line
        int line_right = -1;
        int line_left = -1;
        for(int y = (top_edge + bottom_edge) / 2; y <= bottom_edge; y++)
        {
            int line_right2 = -1;
            int line_left2 = -1;
            for(int x = left_edge; x <= right_edge; x++)
            {
                uint8_t *pixel = row_data[y] + x;
                if(*pixel)
                {
// got a line
                    if(line_left2 < 0)
                    {
                        line_left2 = x;
                    }
                    line_right2 = x + 1;
                    if(line_right2 - line_left2 > line_right - line_left)
                    {
// new longest line
                        line_left = line_left2;
                        line_right = line_right2;
                    }
                }
                else
                {
// line ended
// reset next line
                    line_right2 = -1;
                    line_left2 = -1;
                }
            }
        }
        
        int line_position = (line_left + line_right) / 2;

#ifdef USE_KEYS
        roll = (line_position - range_left) *
            (MAX_ANALOG - MIN_ANALOG) /
            (range_right - range_left);
        CLAMP(roll, MIN_ANALOG, MAX_ANALOG);
#endif


//         printf("detect_roll %d: line_left=%d line_right=%d roll=%d\n",
//             __LINE__,
//             line_left,
//             line_right,
//             roll);
    }
}

void detect_yaw()
{
    int left_edge = capture_w;
    int right_edge = -1;
    int top_edge = -1;
    int bottom_edge = -1;
    int range_left = -1;
    int range_right = -1;
    
    for(int y = capture_h - 1; y >= 1; y--)
    {
        int range_left2 = -1;
        int range_right2 = -1;
        for(int x = throttle_right + 1; x < pitch_left; x++)
        {
            uint8_t *pixel = row_data[y] + x;
            if(*pixel)
            {
                if(bottom_edge < 0)
                {
                    bottom_edge = y;
                }
                
                if(range_left2 < 0)
                {
// range is top left corner
                    range_left2 = x + 1;
                }
// range is top right corner
                range_right2 = x - 1;
                top_edge = y;
                
                if(x < left_edge)
                {
                    left_edge = x;
                }
                if(x > right_edge)
                {
                    right_edge = x;
                }
            }
        }

// went a row without finding anything
        if(bottom_edge >= 0 &&
            y < top_edge)
        {
            break;
        }
        else
        {
// range is last row
            range_left = range_left2;
            range_right = range_right2;
        }
    }


// printf("detect_yaw %d: left=%d right=%d top=%d bottom=%d\n",
// __LINE__,
// left_edge,
// right_edge,
// top_edge,
// bottom_edge);

    if(top_edge < bottom_edge &&
        range_right > range_left)
    {
// detect position by searching for longest horizontal line
        int line_right = -1;
        int line_left = -1;
        for(int y = (top_edge + bottom_edge) / 2; y <= bottom_edge; y++)
        {
            int line_right2 = -1;
            int line_left2 = -1;
            for(int x = left_edge; x <= right_edge; x++)
            {
                uint8_t *pixel = row_data[y] + x;
                if(*pixel)
                {
// got a line
                    if(line_left2 < 0)
                    {
                        line_left2 = x;
                    }
                    line_right2 = x + 1;
                    if(line_right2 - line_left2 > line_right - line_left)
                    {
// new longest line
                        line_left = line_left2;
                        line_right = line_right2;
                    }
                }
                else
                {
// line ended
// reset next line
                    line_right2 = -1;
                    line_left2 = -1;
                }
            }
        }
        
        int line_position = (line_left + line_right) / 2;
        yaw = (line_position - range_left) *
            (MAX_ANALOG - MIN_ANALOG) /
            (range_right - range_left);
        CLAMP(yaw, MIN_ANALOG, MAX_ANALOG);
        have_yaw = 1;
//         printf("detect_yaw %d: line_left=%d line_right=%d yaw=%d\n",
//             __LINE__,
//             line_left,
//             line_right,
//             yaw);
    }
    else
    {
        have_yaw = 0;
    }
}


void menu()
{
    static int prev_bits[16] = { 0 };
    int current_bits[16];
    current_bits[0] = roll;
    current_bits[1] = pitch;
    current_bits[2] = throttle;
    current_bits[2] = have_throttle;
    current_bits[3] = yaw;
    current_bits[3] = have_yaw;
    current_bits[4] = want_roll;
    current_bits[5] = want_pitch;
    current_bits[6] = want_throttle;
    current_bits[7] = want_yaw;
    current_bits[8] = yaw_trim;
    current_bits[9] = capture_x;
    current_bits[10] = capture_y;
    current_bits[11] = capture_w;
    current_bits[12] = capture_h;
    current_bits[13] = mouse_range;

    int got_it = 0;
    for(int i = 0; i < sizeof(current_bits) / sizeof(int); i++)
    {
        if(current_bits[i] != prev_bits[i])
        {
            got_it = 1;
            break;
        }
    }
    
    if(got_it)
    {
// clear screen
        printf("%c[2J", ESC);
// top left
        printf("%c[H", ESC);
        printf("ROLL=%d\n", roll);
        printf("PITCH=%d\n", pitch);
        printf("THROTTLE=%d\n", throttle);
        printf("HAVE THROTTLE=%d\n", have_throttle);
        printf("YAW=%d\n", yaw);
        printf("HAVE YAW=%d\n", have_yaw);
        printf("WANT ROLL=%d\n", want_roll);
        printf("WANT PITCH=%d\n", want_pitch);
        printf("WANT THROTTLE=%d\n", want_throttle);
        printf("WANT YAW=%d\n", want_yaw);
        printf("YAW TRIM=%d\n", yaw_trim);
        printf("CAPTURE X (a,d)=%d\n", capture_x);
        printf("CAPTURE Y (w,s)=%d\n", capture_y);
        printf("CAPTURE W (A,D)=%d\n", capture_w);
        printf("CAPTURE H (W,S)=%d\n", capture_h);
        printf("MOUSE RANGE ([,])=%d\n", mouse_range);
        memcpy(prev_bits, current_bits, sizeof(current_bits));
    }
}

void load_prefs()
{
    char string[TEXTLEN];
    sprintf(string, "%s/%s", getenv("HOME"), PREFS_FILE);
    FILE *fd = fopen(string, "r");
    if(fd)
    {
        fread(&yaw_trim, 1, sizeof(yaw_trim), fd);
        fread(&capture_x, 1, sizeof(capture_x), fd);
        fread(&capture_y, 1, sizeof(capture_y), fd);
        fread(&capture_w, 1, sizeof(capture_w), fd);
        fread(&capture_h, 1, sizeof(capture_h), fd);
        fread(&mouse_range, 1, sizeof(mouse_range), fd);
        fclose(fd);
    }

    CLAMP(capture_x, 0, root_w - capture_w);
    CLAMP(capture_y, 0, root_h - capture_h);
    CLAMP(capture_w, MIN_W, MAX_W);
    CLAMP(capture_h, MIN_H, MAX_H);
    CLAMP(mouse_range, MIN_MOUSE, MAX_MOUSE);
}

void save_prefs()
{
    char string[TEXTLEN];
    sprintf(string, "%s/%s", getenv("HOME"), PREFS_FILE);
    FILE *fd = fopen(string, "w");
    if(fd)
    {
        fwrite(&yaw_trim, 1, sizeof(yaw_trim), fd);
        fwrite(&capture_x, 1, sizeof(capture_x), fd);
        fwrite(&capture_y, 1, sizeof(capture_y), fd);
        fwrite(&capture_w, 1, sizeof(capture_w), fd);
        fwrite(&capture_h, 1, sizeof(capture_h), fd);
        fwrite(&mouse_range, 1, sizeof(mouse_range), fd);
        fclose(fd);
    }
    else
    {
        printf("save_prefs %d: couldn't open output file\n", __LINE__);
        perror("");
    }
}

// handle terminal commands
void terminal(void *ptr)
{
	struct termios info;
	tcgetattr(fileno(stdin), &info);
	info.c_lflag &= ~ICANON;
	info.c_lflag &= ~ECHO;
	tcsetattr(fileno(stdin), TCSANOW, &info);


    while(1)
    {
        char c = getchar();
        
        int got_it = 0;
        pthread_mutex_lock(&settings_lock);
        switch(c)
        {
            case 'a':
                capture_x -= 10;
                got_it = 1;
                break;
            case 'd':
                capture_x += 10;
                got_it = 1;
                break;
            case 'w':
                capture_y -= 10;
                got_it = 1;
                break;
            case 's':
                capture_y += 10;
                got_it = 1;
                break;
            case 'A':
                capture_w -= 10;
                got_it = 1;
                break;
            case 'D':
                capture_w += 10;
                got_it = 1;
                break;
            case 'W':
                capture_h -= 10;
                got_it = 1;
                break;
            case 'S':
                capture_h += 10;
                got_it = 1;
                break;
            case '[':
                mouse_range -= 100;
                got_it = 1;
                break;
            case ']':
                mouse_range += 100;
                got_it = 1;
                break;
        }
        
        if(got_it)
        {
            CLAMP(capture_x, 0, root_w - capture_w);
            CLAMP(capture_y, 0, root_h - capture_h);
            CLAMP(capture_w, MIN_W, MAX_W);
            CLAMP(capture_h, MIN_H, MAX_H);
            CLAMP(mouse_range, MIN_MOUSE, MAX_MOUSE);
            XMoveResizeWindow(display, 
			    left, 
			    capture_x, 
			    capture_y, 
			    1, 
			    capture_h);
            XMoveResizeWindow(display, 
			    right, 
			    capture_x + capture_w, 
			    capture_y, 
			    1, 
			    capture_h);
            XMoveResizeWindow(display, 
			    top, 
			    capture_x, 
			    capture_y, 
			    capture_w, 
			    1);
            XMoveResizeWindow(display, 
			    bottom, 
			    capture_x, 
			    capture_y + capture_h, 
			    capture_w, 
			    1);
            XFlush(display);
            save_prefs();
        }
        pthread_mutex_unlock(&settings_lock);
    }
}


// the event writing loop
void screencap(void *ptr)
{
    pthread_mutex_lock(&settings_lock);
    int prev_w = capture_w;
    int prev_h = capture_h;

    display = XOpenDisplay(":0");
    if(!display)
    {
        printf("screencap %d: failed to connect to X server\n", __LINE__);
        destroy();
        exit(0);
    }
    int screen = DefaultScreen(display);
    Window rootwin = RootWindow(display, screen);
    Visual *vis = DefaultVisual(display, screen);
    int default_depth = DefaultDepth(display, screen);
    XShmSegmentInfo shm_info;
    int bytes_per_line;
    Screen *screen_ptr = XDefaultScreenOfDisplay(display);
    uint8_t *chroma_key = 0;
    uint8_t **src_rows = 0;
    root_w = WidthOfScreen(screen_ptr);
    root_h = HeightOfScreen(screen_ptr);

// create outline out of 4 windows
    unsigned long mask;
    XSetWindowAttributes attr;
    mask = CWBackPixel | CWOverrideRedirect;
    attr.background_pixel = 0xffffff;
    attr.override_redirect = True;
    left = XCreateWindow(display, 
			rootwin, 
			capture_x, 
			capture_y, 
			1, 
			capture_h, 
			0, 
			default_depth, 
			InputOutput, 
			vis, 
			mask, 
			&attr);
    right = XCreateWindow(display, 
			rootwin, 
			capture_x + capture_w, 
			capture_y, 
			1, 
			capture_h, 
			0, 
			default_depth, 
			InputOutput, 
			vis, 
			mask, 
			&attr);
    top = XCreateWindow(display, 
			rootwin, 
			capture_x, 
			capture_y, 
			capture_w, 
			1, 
			0, 
			default_depth, 
			InputOutput, 
			vis, 
			mask, 
			&attr);
    bottom = XCreateWindow(display, 
			rootwin, 
			capture_x, 
			capture_y + capture_h, 
			capture_w, 
			1, 
			0, 
			default_depth, 
			InputOutput, 
			vis, 
			mask, 
			&attr);
    XMapWindow(display, left); 
    XMapWindow(display, right); 
    XMapWindow(display, top); 
    XMapWindow(display, bottom); 
    XFlush(display);

// get bits per pixel
    XImage *ximage;
    unsigned char *data = 0;
    ximage = XCreateImage(display, 
		vis, 
		default_depth, 
		ZPixmap, 
		0, 
		data, 
		16, 
		16, 
		8, 
		0);
    int bits_per_pixel = ximage->bits_per_pixel;
    int bytes_per_pixel = bits_per_pixel / 8;
	XDestroyImage(ximage);

    pthread_mutex_unlock(&settings_lock);




    struct timespec start_time;
    clock_settime(CLOCK_MONOTONIC, &start_time);
    int64_t start_msec = start_time.tv_sec * 1000 + start_time.tv_nsec / 1000000;

    uint8_t prev_report[REPORT_SIZE];
    bzero(prev_report, REPORT_SIZE);
    while(1)
    {
        pthread_mutex_lock(&settings_lock);
        
// resize the bitmap
        if(prev_w != capture_w ||
            prev_h != capture_h)
        {
            if(row_data)
            {
                XDestroyImage(ximage);
                XShmDetach(display, &shm_info);
                shmdt(shm_info.shmaddr);

                free(chroma_key);
                free(src_rows);
                free(row_data);
                chroma_key = 0;
                src_rows = 0;
                row_data = 0;
            }
        }
        
        prev_w = capture_w;
        prev_h = capture_h;
        
        if(!row_data)
        {
// create the bitmaps
            ximage = XShmCreateImage(display, 
                vis, 
                default_depth, 
                ZPixmap, 
                (char*)NULL, 
                &shm_info, 
                capture_w, 
                capture_h);
            shm_info.shmid = shmget(IPC_PRIVATE, 
                capture_h * ximage->bytes_per_line, 
                IPC_CREAT | 0777);
            data = (unsigned char *)shmat(shm_info.shmid, NULL, 0);
            shmctl(shm_info.shmid, IPC_RMID, 0);
            ximage->data = shm_info.shmaddr = (char*)data;  // setting ximage->data stops BadValue
            shm_info.readOnly = 0;
            XShmAttach(display, &shm_info);
            XSync(display, False);

            bytes_per_line = ximage->bytes_per_line;

        
        
            chroma_key = malloc(capture_w * capture_h);
	        src_rows = malloc(sizeof(uint8_t*) * capture_h);
	        for(int i = 0; i < capture_h; i++)
	        {
		        src_rows[i] = &data[i * bytes_per_line];
	        }
	        row_data = malloc(sizeof(uint8_t*) * capture_h);
	        for(int i = 0; i < capture_h; i++)
	        {
		        row_data[i] = &chroma_key[i * capture_w];
	        }
        }
        

        XShmGetImage(display, 
            rootwin, 
            ximage, 
            capture_x, 
            capture_y, 
            0xffffffff);

// chroma key it
        for(int i = 0; i < capture_h; i++)
        {
            uint8_t *src_row = src_rows[i];
            uint8_t *dst_row = row_data[i];
            for(int j = 0; j < capture_w; j++)
            {
#define THRESHOLD 64
                if(src_row[0] < THRESHOLD &&            // B
                    src_row[1] > 255 - THRESHOLD &&     // G
                    src_row[2] < THRESHOLD)             // R
                {
                    *dst_row = 0xff;
                }
                else
                {
                    *dst_row = 0x00;
                }
                dst_row++;
                src_row += 4;
            }
        }


// have to detect these 2 to detect roll & yaw
        detect_throttle();
        detect_pitch();

        detect_roll();
        detect_yaw();

        pthread_mutex_unlock(&settings_lock);





#ifndef USE_KEYS
// get pointer position for bounds checking
        Window temp_win;
        int cursor_x, cursor_y, win_x, win_y;
        unsigned int temp_mask;
	    XQueryPointer(display, 
	       rootwin, 
	       &temp_win, 
	       &temp_win,
           &cursor_x, 
	       &cursor_y, 
	       &win_x, 
	       &win_y, 
	       &temp_mask);

        int xdiff = want_roll - roll;
        int ydiff = want_pitch - pitch;

// clamp cursor to screen size
        if(cursor_x + xdiff > root_w)
        {
            xdiff = root_w - cursor_x;
        }
        else
        if(cursor_x + xdiff < 0)
        {
            xdiff = -cursor_x;
        }
        
        if(cursor_y + ydiff > root_h)
        {
            ydiff = root_h - cursor_y;
        }
        else
        if(cursor_y + ydiff < 0)
        {
            ydiff = -cursor_y;
        }

// limits are defined in the report descriptor
        CLAMP(xdiff, -127, 127);
        CLAMP(ydiff, -127, 127);
//        CLAMP(xdiff, -20, 20);
//        CLAMP(ydiff, -20, 20);
        if(xdiff || ydiff)
        {
            roll += xdiff;
            pitch += ydiff;
//printf("screencap %d xdiff=%d ydiff=%d roll=%d pitch=%d\n", 
//__LINE__, xdiff, ydiff, roll, pitch);

// multiply to defeat downsampling inside kernel
//            xdiff *= 6;
//            ydiff *= 6;
            uint8_t report[REPORT_SIZE];
            bzero(report, REPORT_SIZE);
            report[0] = 0x1;
            report[2] = (uint8_t)xdiff;
            report[3] = (uint8_t)ydiff;

            send_mouse(report, REPORT_SIZE);
        }



#endif // USE_KEYS





// only 1 code may fire at a time
        uint8_t analog_code = 0;
        if(have_throttle)
        {
            if(want_throttle > throttle)
            {
                analog_code = PGUP;
            }
            else
            if(want_throttle < throttle)
            {
                analog_code = PGDN;
            }
        }

        if(!analog_code && have_yaw)
        {
            if(want_yaw > yaw)
            {
    //printf("screencap %d: want_yaw=%d yaw=%d\n", __LINE__, want_yaw, yaw);
                analog_code = KP_ENTER;
            }
            else
            if(want_yaw < yaw)
            {
                analog_code = KP_INS;
            }
        }


#ifdef USE_KEYS
        if(!analog_code && want_pitch > pitch)
        {
            analog_code = UP;
        }
        else
        if(!analog_code && want_pitch < pitch)
        {
            analog_code = DN;
        }

        if(!analog_code && want_roll > roll)
        {
            analog_code = RIGHT;
        }
        else
        if(!analog_code && want_roll < roll)
        {
            analog_code = LEFT;
        }
#endif // USE_KEYS

//         printf("screencap %d want_throttle=%d want_yaw=%d want_pitch=%d want_roll=%d\n", 
//             __LINE__, 
//             want_throttle,
//             want_yaw,
//             want_pitch,
//             want_roll);
//         printf("screencap %d throttle=%d yaw=%d pitch=%d roll=%d\n", 
//             __LINE__, 
//             throttle,
//             yaw,
//             pitch,
//             roll);


        uint8_t report[REPORT_SIZE];
        bzero(report, sizeof(report));
        int key = 2;

        if(analog_code)
        {
// process analog events 1st since they're based on time
            report[key++] = analog_code;
        }
        else
        {
// update the persistent report state from the button events
            report_mod = 0;
            bzero(report_key, sizeof(report_key));
            int current_report_key = 0;
            int got_brake = 0;
            for(int i = 0; i < TOTAL_BUTTONS; i++)
            {
                if(buttons[i].is_down)
                {
                    int is_brake = 0;
                    if(buttons[i].code == PERIOD ||
                        buttons[i].code == COMMA)
                    {
                        is_brake = 1;
                        got_brake = 1;
                    }
                    
                    if(!got_brake || is_brake)
                    {
// reject all other keys if a brake key is pressed
// accept all keys if brake isn't pressed
                        report_mod = buttons[i].modifier;
                        report_key[current_report_key++] = buttons[i].code;
                    }

                    if((!is_brake && !got_brake) ||
                        current_report_key >= REPORT_KEYS)
                    {
// quit after 1 keypress unless the brake is pressed
                        break;
                    }
                }
            }
        }

        report[0] |= report_mod;
        for(int i = 0; i < REPORT_KEYS; i++)
        {
            if(report_key[i])
            {
                report[key++] = report_key[i];
            }
        }

// send the report if it changed
        if(memcmp(report, prev_report, REPORT_SIZE))
        {
//             printf("screencap %d: wrote %02x %02x %02x %02x %02x %02x %02x %02x\n", 
//                 __LINE__, 
//                 report[0],
//                 report[1],
//                 report[2],
//                 report[3],
//                 report[4],
//                 report[5],
//                 report[6],
//                 report[7]);
            send_keyboard(report, REPORT_SIZE);
            memcpy(prev_report, report, REPORT_SIZE);

        }

        menu();


        struct timespec current_time;
        clock_settime(CLOCK_MONOTONIC, &current_time);
        int64_t current_msec = current_time.tv_sec * 1000 + current_time.tv_nsec / 1000000;
        int64_t diff = 1000 / FRAMERATE - (current_msec - start_msec);
        start_msec = current_msec;
        if(diff > 0)
        {
            usleep(diff * 1000);
        }
    }
}



int main(int argc, char **argv)
{
	int ret;
	struct termios state;


    signal(SIGHUP, sig_catch);
    signal(SIGABRT, sig_catch);
    signal(SIGKILL, sig_catch);
    signal(SIGINT, sig_catch);
    signal(SIGQUIT, sig_catch);
    signal(SIGTERM, sig_catch);

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
    pthread_mutex_init(&settings_lock, &attr);


    load_prefs();

// probe thrustmaster raw device
    FILE *probe = fopen("/proc/bus/input/devices", "r");
    char string[TEXTLEN];
    int got_thrust = 0;
    int got_it = 0;
    char path2[TEXTLEN];
    char path3[TEXTLEN];
    char path4[TEXTLEN];
    int i;
    int prev_throttle = 0;
    while(!feof(probe))
    {
        char *x = fgets(string, TEXTLEN, probe);
        if(!x)
        {
            break;
        }

//printf("main %d %s\n", __LINE__, string);
        if(got_thrust)
        {
            char *ptr = strstr(string, "Sysfs=");
            if(ptr)
            {
                ptr += 6;
                sprintf(path2, "/sys/%s", ptr);
                got_it = 1;
                break;
            }
        }
        else
        if(strstr(string, "Thrustmaster"))
        {
            got_thrust = 1;
        }
    }
    fclose(probe);

    if(!got_it)
    {
        printf("Didn't find Thrustmaster\n");
        exit(0);
    }

// strip newlines
    while(path2[strlen(path2) - 1] == '\n')
    {
        path2[strlen(path2) - 1] = 0;
    }

    printf("main %d: path2=%s\n", __LINE__, path2);
    sprintf(path3, "ls %s/device/hidraw", path2);
    printf("main %d: path3=%s\n", __LINE__, path3);
    probe = popen(path3, "r");
    char *x = fgets(string, TEXTLEN, probe);
    fclose(probe);

// strip newlines
    while(string[strlen(string) - 1] == '\n')
    {
        string[strlen(string) - 1] = 0;
    }

    sprintf(path4, "/dev/%s", string);
    printf("main %d: path4=%s\n", __LINE__, path4);

// read thrustmaster
    int in_fd = open(path4, O_RDONLY);
    if(in_fd < 0)
    {
        printf("Couldn't open %s\n", path4);
        perror("");
        destroy();

	    return EXIT_SUCCESS;
    }




// create virtual keyboard

// create virtual keyboard
	create_keyboard();
// create virtual mouse
    create_mouse();
// must disable nonlinear acceleration after the X server detects it
    sleep(1);
    printf("main %d: disabling mouse acceleration\n", __LINE__);
    system("DISPLAY=:0 xset m 0 0");

// screencap thread
	pthread_attr_t attr2;
	pthread_attr_init(&attr2);
	pthread_t tid;
	pthread_create(&tid, 
		&attr2, 
		(void*)screencap, 
		0);


// terminal thread
    pthread_create(&tid,
        &attr2,
        (void*)terminal,
        0);




// Thrustmaster reading loop
    while(1)
    {
        int bytes_read = read(in_fd, string, TEXTLEN);
        
        if(bytes_read < 0)
        {
            printf("main %d: unplugged\n", __LINE__);
            destroy();
            exit(0);
        }
        
//         printf("main %d: read bytes_read=%d\n", __LINE__, bytes_read);
//         for(i = 0; i < bytes_read; i++)
//         {
//             printf("%02x ", (uint8_t)string[i]);
//         }
//         printf("\n");

// convert hat button to bitmask.  Goog doesn't support diagonal.
        switch(string[5])
        {
            case 0:
                string[63] = 0x04;
                break;
            case 2:
                string[63] = 0x02;
                break;
            case 4:
                string[63] = 0x08;
                break;
            case 6:
                string[63] = 0x01;
                break;
            default:
                string[63] = 0;
                break;
        }

// scan known buttons
        for(i = 0; i < TOTAL_BUTTONS; i++)
        {
            if((string[buttons[i].offset] & buttons[i].mask) != 0 &&
                !buttons[i].is_down)
            {
                buttons[i].is_down = 1;
            }
            else
            if((string[buttons[i].offset] & buttons[i].mask) == 0 &&
                buttons[i].is_down)
            {
                buttons[i].is_down = 0;
            }
        }


// analog
        want_throttle = (255 - (uint8_t)string[48]) * 
            (MAX_ANALOG - MIN_ANALOG) / 
            255 +
            MIN_ANALOG;
        want_yaw = (uint8_t)string[47] *
            (MAX_ANALOG - MIN_ANALOG) / 
            255 +
            MIN_ANALOG +
            yaw_trim;
        CLAMP(want_yaw, MIN_ANALOG, MAX_ANALOG);

// yaw trim
        int a = (uint8_t)string[49];
        if(a >= 0x40 && a <= 0xc0)
        {
            got_yaw_trim = 0;
        }
        else
        if(a > 0xc0 && !got_yaw_trim)
        {
            yaw_trim++;
            CLAMP(yaw_trim, (-MAX_ANALOG / 2), (MAX_ANALOG / 2));
            got_yaw_trim = 1;
            save_prefs();
        }
        else
        if(a < 0x40 && !got_yaw_trim)
        {
            yaw_trim--;
            CLAMP(yaw_trim, (-MAX_ANALOG / 2), (MAX_ANALOG / 2));
            got_yaw_trim = 1;
            save_prefs();
        }
        
#ifdef USE_KEYS
        want_pitch = (255 - (uint8_t)string[46]) *
            (MAX_ANALOG - MIN_ANALOG) / 
            255 +
            MIN_ANALOG;
        want_roll = (uint8_t)string[44] *
            (MAX_ANALOG - MIN_ANALOG) / 
            255 +
            MIN_ANALOG;
#else // USE_KEYS
        want_pitch = ((uint8_t)string[46]) *
            (mouse_range * 2) / 
            255 +
            -mouse_range;
        want_roll = (uint8_t)string[44] *
            (mouse_range * 2) / 
            255 +
            -mouse_range;
#endif // !USE_KEYS


    }
    
    
    destroy();

	return EXIT_SUCCESS;
}


