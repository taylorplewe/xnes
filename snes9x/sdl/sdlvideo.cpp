/***********************************************************************************
  Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.

  See CREDITS file to find the copyright owners of this file.

  SDL Input/Audio/Video code (many lines of code come from snes9x & drnoksnes)
  (c) Copyright 2011         Makoto Sugano (makoto.sugano@gmail.com)

  Snes9x homepage: http://www.snes9x.com/

  Permission to use, copy, modify and/or distribute Snes9x in both binary
  and source form, for non-commercial purposes, is hereby granted without
  fee, providing that this license information and copyright notice appear
  with all copies and any derived work.

  This software is provided 'as-is', without any express or implied
  warranty. In no event shall the authors be held liable for any damages
  arising from the use of this software or it's derivatives.

  Snes9x is freeware for PERSONAL USE only. Commercial users should
  seek permission of the copyright holders first. Commercial use includes,
  but is not limited to, charging money for Snes9x or software derived from
  Snes9x, including Snes9x or derivatives in commercial game bundles, and/or
  using Snes9x as a promotion for your commercial product.

  The copyright holders request that bug fixes and improvements to the code
  should be forwarded to them so everyone can benefit from the modifications
  in future versions.

  Super NES and Super Nintendo Entertainment System are trademarks of
  Nintendo Co., Limited and its subsidiary companies.
 ***********************************************************************************/

#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include "snes9x.h"
#include "memmap.h"
#include "ppu.h"
#include "controls.h"
#include "movie.h"
#include "logger.h"
#include "conffile.h"
#include "display.h"

#include "sdl_snes9x.h"
//#define GFX2X
//#define ASCII
#ifdef GFX2X
    #define GFX_SCALE 2
#else
    #define GFX_SCALE 1
#endif
#define BPP 32

#ifdef ASCII
#include "../../ansi/drawansi.h"
#endif

struct GUIData
{
    #ifdef USE_SDL
		SDL_Surface*  sdl_screen;
		SDL_Renderer* sdl_renderer;
		SDL_Window*   sdl_window;
		uint8*        blit_screen;
		uint32        blit_screen_pitch;
    #endif
    uint8*        snes_buffer;
	int           video_mode;
    bool8         fullscreen;
};
static struct GUIData GUI;

typedef	void (* Blitter) (uint8*, int, uint8*, int, int, int);

enum
{
	VIDEOMODE_BLOCKY = 1,
	VIDEOMODE_TV,
	VIDEOMODE_SMOOTH,
	VIDEOMODE_SUPEREAGLE,
	VIDEOMODE_2XSAI,
	VIDEOMODE_SUPER2XSAI,
	VIDEOMODE_EPX,
	VIDEOMODE_HQ2X
};

static void SetupImage (void);
static void TakedownImage (void);

void S9xExtraDisplayUsage (void)
{
	S9xMessage(S9X_INFO, S9X_USAGE, "-fullscreen                     fullscreen mode (without scaling)");
	S9xMessage(S9X_INFO, S9X_USAGE, "");
	S9xMessage(S9X_INFO, S9X_USAGE, "-v1                             Video mode: Blocky (default)");
	S9xMessage(S9X_INFO, S9X_USAGE, "-v2                             Video mode: TV");
	S9xMessage(S9X_INFO, S9X_USAGE, "-v3                             Video mode: Smooth");
	S9xMessage(S9X_INFO, S9X_USAGE, "-v4                             Video mode: SuperEagle");
	S9xMessage(S9X_INFO, S9X_USAGE, "-v5                             Video mode: 2xSaI");
	S9xMessage(S9X_INFO, S9X_USAGE, "-v6                             Video mode: Super2xSaI");
	S9xMessage(S9X_INFO, S9X_USAGE, "-v7                             Video mode: EPX");
	S9xMessage(S9X_INFO, S9X_USAGE, "-v8                             Video mode: hq2x");
	S9xMessage(S9X_INFO, S9X_USAGE, "");
}

void S9xParseDisplayArg (char** argv, int &i, int argc);

const char* S9xParseDisplayConfig (ConfigFile &conf, int pass)
{
    printf("parse display config\n");
	return ("Unix/SDL");
}

static void FatalError (const char* str)
{
	fprintf(stderr, "%s\n", str);
	S9xExit();
}

void S9xInitDisplay (int argc, char** argv)
{
	#ifdef USE_SDL
		if (SDL_Init(SDL_INIT_VIDEO) != 0)
		{
			printf("Unable to initialize SDL: %s\n", SDL_GetError());
		}

		atexit(SDL_Quit);

		SDL_CreateWindowAndRenderer(
			SNES_WIDTH * GFX_SCALE,
			SNES_HEIGHT * GFX_SCALE,
			0,
			&GUI.sdl_window,
			&GUI.sdl_renderer
		);
		GUI.sdl_screen = SDL_CreateRGBSurface(
			0,
			SNES_WIDTH * GFX_SCALE,
			SNES_HEIGHT * GFX_SCALE,
			BPP,
			0, 0, 0, 0
		);
		// GUI.sdl_screen = SDL_SetVideoMode(SNES_WIDTH * GFX_SCALE, SNES_HEIGHT * GFX_SCALE, BPP, 0);

		if (GUI.sdl_screen == NULL) {
			printf("Unable to set video mode: %s\n", SDL_GetError());
			exit(1);
		}
	#endif
	SetupImage();
}

void S9xDeinitDisplay (void)
{
	TakedownImage();
	#ifdef USE_SDL
		SDL_Quit();
	#endif

}

static void TakedownImage (void)
{
	if (GUI.snes_buffer)
	{
		free(GUI.snes_buffer);
		GUI.snes_buffer = NULL;
	}
	S9xGraphicsDeinit();
}

static void SetupImage (void)
{
	TakedownImage();

	// domaemon: The whole unix code basically assumes output=(original * 2);
	// This way the code can handle the SNES filters, which does the 2X.

	GFX.Pitch = SNES_WIDTH * 2 * GFX_SCALE;

	GUI.snes_buffer = (uint8*)calloc(GFX.Pitch * ((SNES_HEIGHT_EXTENDED + 4) * GFX_SCALE), 1);

	if (!GUI.snes_buffer)
		FatalError("Failed to allocate GUI.snes_buffer.");

	// domaemon: Add 2 lines before drawing.
	GFX.Screen = (uint16*)(GUI.snes_buffer + (GFX.Pitch * 2 * GFX_SCALE));
	#ifdef USE_SDL
		GUI.blit_screen       = (uint8*)GUI.sdl_screen->pixels;
		GUI.blit_screen_pitch = SNES_WIDTH * GFX_SCALE * (BPP/8);
	#endif
	S9xGraphicsInit();
}

void S9xPutImage (int width, int height)
{
	#ifdef USE_SDL
		SDL_LockSurface(GUI.sdl_screen);
		SDL_ConvertPixels(
			width, height,
			SDL_PIXELFORMAT_RGB565,
			(uint32*)GFX.Screen,
			GFX.Pitch,
			SDL_PIXELFORMAT_RGB888,
			(uint32*)GUI.blit_screen,
			GUI.blit_screen_pitch
		);
		SDL_UnlockSurface(GUI.sdl_screen);

		SDL_Texture* sdl_screen_texture = SDL_CreateTextureFromSurface(GUI.sdl_renderer, GUI.sdl_screen);
		SDL_RenderClear(GUI.sdl_renderer);
		SDL_RenderCopy(GUI.sdl_renderer, sdl_screen_texture, NULL, NULL);
		SDL_RenderPresent(GUI.sdl_renderer);
		SDL_DestroyTexture(sdl_screen_texture);
	#endif
}


void S9xMessage (int type, int number, const char* message)
{
	const int	max = 36 * 3;
	static char	buffer[max + 1];

	fprintf(stdout, "%s\n", message);
	strncpy(buffer, message, max + 1);
	buffer[max] = 0;
	S9xSetInfoString(buffer);
}

const char* S9xStringInput (const char *message)
{
	static char	buffer[256];

	printf("%s: ", message);
	fflush(stdout);

	if (fgets(buffer, sizeof(buffer) - 2, stdin))
		return (buffer);

	return (NULL);
}

void S9xSetTitle (const char *string) { }

void S9xSetPalette (void) { }
