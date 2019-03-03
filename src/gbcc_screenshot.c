#include "gbcc.h"
#include "gbcc_debug.h"
#include "gbcc_screenshot.h"
#include "gbcc_window.h"
#include <errno.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#define MAX_NAME_LEN 4096

void gbcc_screenshot(struct gbcc_window *win)
{
	char *dir;
	char fname[MAX_NAME_LEN];

	dir = getenv("HOME");
	if (!dir) {
		dir = ".";
	}

	strncpy(fname, dir, MAX_NAME_LEN);

	time_t raw_time;
	struct tm *time_info;
	time(&raw_time);
	time_info = gmtime(&raw_time);
	strftime(fname + strlen(dir), MAX_NAME_LEN - strlen(dir), "/%G-%m-%dT%H%M%SZ-gbcc.png", time_info);

	FILE *fp = fopen(fname, "wbe");
	if (!fp) {
		gbcc_log_error("Couldn't open %s: %s\n", fname, strerror(errno));
		return;
	}
	png_structp png_ptr = png_create_write_struct(
			PNG_LIBPNG_VER_STRING,
			NULL, NULL, NULL);
	if (!png_ptr) {
		fclose(fp);
		gbcc_log_error("Couldn't create PNG write struct.\n");
		return;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_write_struct(&png_ptr, NULL);
		fclose(fp);
		gbcc_log_error("Couldn't create PNG info struct.\n");
		return;
	}

	if (setjmp(png_jmpbuf(png_ptr)) != 0) {
		png_destroy_write_struct(&png_ptr, NULL);
		fclose(fp);
		gbcc_log_error("Couldn't setjmp for libpng.\n");
		return;
	}

	int size_mult;
	uint8_t *buffer;
	if (win->raw_screenshot) {
		size_mult = 1;
	} else {
		size_mult = FBO_WIDTH / GBC_SCREEN_WIDTH;
		buffer = malloc(FBO_SIZE * 3 * sizeof(*buffer));
		glReadPixels(0, 0, FBO_WIDTH, FBO_HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, buffer);
	}
	/* Initialize rows of PNG. */
	png_bytepp row_pointers = png_malloc(png_ptr, size_mult * GBC_SCREEN_HEIGHT * sizeof(png_bytep));
	for (int y = 0; y < size_mult * GBC_SCREEN_HEIGHT; y++) {
		png_bytep row = png_malloc(png_ptr, size_mult * GBC_SCREEN_WIDTH * 3);
		row_pointers[y] = row;
		for (int x = 0; x < size_mult * GBC_SCREEN_WIDTH; x++) {
			if (win->raw_screenshot) {
				int idx = y * size_mult * GBC_SCREEN_WIDTH + x;
				uint32_t pixel = win->buffer[idx];
				*row++ = (pixel & 0xFF000000u) >> 24u;
				*row++ = (pixel & 0x00FF0000u) >> 16u;
				*row++ = (pixel & 0x0000FF00u) >> 8u;
			} else {
				int idx = 3 * ((size_mult * GBC_SCREEN_HEIGHT - y - 1) * size_mult * GBC_SCREEN_WIDTH + x);
				*row++ = buffer[idx++];
				*row++ = buffer[idx++];
				*row++ = buffer[idx++];
			}
		}
	}

	png_init_io(png_ptr, fp);
	png_set_IHDR(png_ptr, info_ptr,
			size_mult * GBC_SCREEN_WIDTH, size_mult * GBC_SCREEN_HEIGHT,
			8, PNG_COLOR_TYPE_RGB,
			PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
			PNG_FILTER_TYPE_DEFAULT);
	png_set_rows(png_ptr, info_ptr, row_pointers);
	png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

	for (int y = 0; y < size_mult * GBC_SCREEN_HEIGHT; y++) {
		png_free(png_ptr, row_pointers[y]);
	}
	png_free(png_ptr, row_pointers);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(fp);

	if (!win->raw_screenshot) {
		free(buffer);
	}

	gbcc_log_info("Saved screenshot %s\n", fname);
	char message[MAX_NAME_LEN];
	gbcc_window_show_message(win, message, 2);
	win->screenshot = false;
	win->raw_screenshot = false;
}
