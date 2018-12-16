/*
 * main.c
 *
 *  Created on: 13.05.2014
 *      Author: Florian
 */

/*---------------------------------------------------------------------*
 *  include files                                                      *
 *---------------------------------------------------------------------*/

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "button.h"
#include "debug.h"
#include "ff.h"
#include "gfx.h"
#include "i2c.h"
#include "jpeg.h"
#include "led.h"
#include "main.h"
#include "sdram.h"
#include "spi.h"
#include "images.h"
#include "touch.h"
extern void D_DoomMain (void);

static void show_image (const uint8_t* img)
{
	gfx_image_t title;
	gfx_obj_t obj_title;

	title.width = 240;
	title.height = 320;
	title.pixel_format = GFX_PIXEL_FORMAT_RGB565;
	title.pixel_data = malloc (title.width * title.height * 2);

	obj_title.obj_type = GFX_OBJ_IMG;
	obj_title.coords.dest_x = 0;
	obj_title.coords.dest_y = 0;
	obj_title.coords.source_x = 0;
	obj_title.coords.source_y = 0;
	obj_title.coords.source_w = title.width;
	obj_title.coords.source_h = title.height;
	obj_title.data = &title;
    obj_title.enabled = 1;

	jpeg_decode ((uint8_t*)img, title.pixel_data, 0, 0);

	gfx_objects[0] = &obj_title;

	gfx_draw_objects ();
	gfx_delete_objects ();

	free (title.pixel_data);
}

/*---------------------------------------------------------------------*
 *  public functions                                                   *
 *---------------------------------------------------------------------*/

int d_main(void)
{
    gfx_init ();
    // show title as soon as possible
    //show_image (img_loading);
    touch_init ();
    button_init ();
    D_DoomMain ();
    return 0;
}

/*
 * Sleep for the specified number of milliseconds
 */
extern uint32_t systime;
void sleep_ms (uint32_t ms)
{
	uint32_t wait_start;

	wait_start = systime;

	while (systime - wait_start < ms)
	{
	}
}

uint32_t sec_time = 0;
uint32_t frames_count;
uint32_t fps_prev;
uint32_t msec_per_frame;
uint32_t msec_per_frame_start;
#define MS_PER_SEC 1000
void fps_update (void)
{
    if (systime - sec_time * MS_PER_SEC >= MS_PER_SEC) {
        sec_time++;
        fps_prev = frames_count;
        frames_count = 0;
    } else {
        frames_count++;
    }
}

void frame_start ()
{
    msec_per_frame_start = systime;
}

void frame_end ()
{
    msec_per_frame = systime - msec_per_frame_start;
}


/*
 * Show fatal error message and stop in endless loop
 */
void fatal_error (const char* message)
{
	while (1)
	{
	}
}

/*---------------------------------------------------------------------*
 *  eof                                                                *
 *---------------------------------------------------------------------*/
