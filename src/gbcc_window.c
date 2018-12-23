#include "gbcc.h"
#include "gbcc_debug.h"
#include "gbcc_input.h"
#include "gbcc_memory.h"
#include "gbcc_window.h"
#include "gbcc_time.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>

static int window_thread_function(void *window);

struct gbcc_window *gbcc_window_initialise(struct gbc *gbc, bool vsync)
{
	SDL_Init(0);

	struct gbcc_window *win = malloc(sizeof(struct gbcc_window));
	win->gbc = gbc;
	win->quit = false;
	win->vsync = vsync;

	if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
		gbcc_log_error("Failed to initialize SDL: %s\n", SDL_GetError());
	}

	win->thread = SDL_CreateThread(
			window_thread_function, 
			"RenderingThread", 
			(void *)(win));

	if (win->thread == NULL) {
		gbcc_log_error("Error creating rendering thread: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}
	return win;
}

void gbcc_window_destroy(struct gbcc_window *win)
{
	win->quit = true;
	SDL_WaitThread(win->thread, NULL);
	SDL_DestroyTexture(win->texture);
	SDL_DestroyRenderer(win->renderer);
	SDL_DestroyWindow(win->window);
}

static int window_thread_function(void *window)
{
	int err;
	struct gbcc_window *win = (struct gbcc_window *)window;


	win->window = SDL_CreateWindow(
			"GBCC",                    // window title
			SDL_WINDOWPOS_UNDEFINED,   // initial x position
			SDL_WINDOWPOS_UNDEFINED,   // initial y position
			GBC_SCREEN_WIDTH,      // width, in pixels
			GBC_SCREEN_HEIGHT,     // height, in pixels
			SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE // flags - see below
			);

	if (win->window == NULL) {
		gbcc_log_error("Could not create window: %s\n", SDL_GetError());
		SDL_Quit();
		exit(EXIT_FAILURE);
	}

	if (win->vsync) {
		SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
	} else {
		SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");
	}
	win->renderer = SDL_CreateRenderer(
			win->window,
			-1,
			SDL_RENDERER_ACCELERATED
			);

	if (win->renderer == NULL) {
		gbcc_log_error("Could not create renderer: %s\n", SDL_GetError());
		SDL_DestroyWindow(win->window);
		SDL_Quit();
		exit(EXIT_FAILURE);
	}

	win->texture = SDL_CreateTexture(
			win->renderer,
			SDL_PIXELFORMAT_RGB888,
			SDL_TEXTUREACCESS_STATIC,
			GBC_SCREEN_WIDTH, GBC_SCREEN_HEIGHT
			);

	if (win->texture == NULL) {
		gbcc_log_error("Could not create texture: %s\n", SDL_GetError());
		SDL_DestroyRenderer(win->renderer);
		SDL_DestroyWindow(win->window);
		SDL_Quit();
		exit(EXIT_FAILURE);
	}

	SDL_RenderSetLogicalSize(
			win->renderer,
			GBC_SCREEN_WIDTH, GBC_SCREEN_HEIGHT
			);
	SDL_RenderSetIntegerScale(win->renderer, true);

	for (size_t i = 0; i < GBC_SCREEN_SIZE; i++) {
		win->buffer[i] = 0;
	}

	//size_t last_frame = 0;

	/* Main rendering loop */
	while (!(win->quit)) {
		gbcc_input_process_all(win->gbc);
		/* Do the actual drawing */
		for (size_t i = 0; i < GBC_SCREEN_SIZE; i++) {
			win->buffer[i] = win->gbc->memory.sdl_screen[i];
		}
		/* Draw the background */
		err = SDL_UpdateTexture(
				win->texture, 
				NULL, 
				win->buffer, 
				GBC_SCREEN_WIDTH * sizeof(win->buffer[0])
				);
		if (err < 0) {
			gbcc_log_error("Error updating texture: %s\n", SDL_GetError());
			exit(EXIT_FAILURE);
		}

		err = SDL_RenderClear(win->renderer);
		if (err < 0) {
			gbcc_log_error("Error clearing renderer: %s\n", SDL_GetError());
			if (win->renderer == NULL) {
				gbcc_log_error("NULL Renderer!\n");
			}
			exit(EXIT_FAILURE);
		}

		err = SDL_RenderCopy(win->renderer, win->texture, NULL, NULL);
		if (err < 0) {
			gbcc_log_error("Error copying texture: %s\n", SDL_GetError());
			exit(EXIT_FAILURE);
		}

		SDL_RenderPresent(win->renderer);

		SDL_Delay(16);
		/*size_t fps = (win->gbc->frame - last_frame) * 60;
		gbcc_log_debug("FPS: %lu\n", fps);
		last_frame = win->gbc->frame;*/
	}
	//printf("QUIT SDL\n");
	return 0;
}
