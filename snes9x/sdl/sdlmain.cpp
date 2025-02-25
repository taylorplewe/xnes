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
#include <dirent.h>
#include <errno.h>
#include <string.h>

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "sdl_snes9x.h"

#include "snes9x.h"
#include "memmap.h"
#include "apu/apu.h"
#include "gfx.h"
#include "controls.h"
#ifdef FANCY
#include "snapshot.h"
#include "cheats.h"
#include "logger.h"
#include "conffile.h"
#endif
#include "display.h"

#ifdef HTML
#include <emscripten.h>
#endif


static const char	*s9x_base_dir        = NULL;

extern uint32           sound_buffer_size; // used in sdlaudio

static char		default_dir[PATH_MAX + 1];

static const char	dirNames[13][32] =
{
	"",				// DEFAULT_DIR
	"",				// HOME_DIR
	"",				// ROMFILENAME_DIR
	"rom",			// ROM_DIR
	"sram",			// SRAM_DIR
	"savestate",	// SNAPSHOT_DIR
	"screenshot",	// SCREENSHOT_DIR
	"spc",			// SPC_DIR
	"cheat",		// CHEAT_DIR
	"patch",		// IPS_DIR
	"bios",			// BIOS_DIR
	"log",			// LOG_DIR
	""
};


void S9xParseInputConfig(ConfigFile &, int pass); // defined in sdlinput

static void NSRTControllerSetup (void);

void _makepath (char *path, const char *, const char *dir, const char *fname, const char *ext)
{
	if (dir && *dir)
	{
		strcpy(path, dir);
		strcat(path, SLASH_STR);
	}
	else
		*path = 0;

	strcat(path, fname);

	if (ext && *ext)
	{
		strcat(path, ".");
		strcat(path, ext);
	}
}

void _splitpath (const char *path, char *drive, char *dir, char *fname, char *ext)
{
	*drive = 0;

	const char	*slash = strrchr(path, SLASH_CHAR),
				*dot   = strrchr(path, '.');

	if (dot && slash && dot < slash)
		dot = NULL;

	if (!slash)
	{
		*dir = 0;

		strcpy(fname, path);

		if (dot)
		{
			fname[dot - path] = 0;
			strcpy(ext, dot + 1);
		}
		else
			*ext = 0;
	}
	else
	{
		strcpy(dir, path);
		dir[slash - path] = 0;

		strcpy(fname, slash + 1);

		if (dot)
		{
			fname[dot - slash - 1] = 0;
			strcpy(ext, dot + 1);
		}
		else
			*ext = 0;
	}
}


void S9xExtraUsage (void) // domaemon: ExtraUsage -> ExtraDisplayUsage
{
	/*                               12345678901234567890123456789012345678901234567890123456789012345678901234567890 */

	S9xMessage(S9X_INFO, S9X_USAGE, "-multi                          Enable multi cartridge system");
	S9xMessage(S9X_INFO, S9X_USAGE, "-carta <filename>               ROM in slot A (use with -multi)");
	S9xMessage(S9X_INFO, S9X_USAGE, "-cartb <filename>               ROM in slot B (use with -multi)");
	S9xMessage(S9X_INFO, S9X_USAGE, "");

	S9xMessage(S9X_INFO, S9X_USAGE, "-buffersize                     Sound generating buffer size in millisecond");
	S9xMessage(S9X_INFO, S9X_USAGE, "");

	S9xMessage(S9X_INFO, S9X_USAGE, "-loadsnapshot                   Load snapshot file at start");
	S9xMessage(S9X_INFO, S9X_USAGE, "-dumpstreams                    Save audio/video data to disk");
	S9xMessage(S9X_INFO, S9X_USAGE, "-dumpmaxframes <num>            Stop emulator after saving specified number of");
	S9xMessage(S9X_INFO, S9X_USAGE, "                                frames (use with -dumpstreams)");
	S9xMessage(S9X_INFO, S9X_USAGE, "");

	S9xExtraDisplayUsage();
}

/*
 * domaemon: arg is parsed as ParseArg -> ParseDisplayArg
 */
const char *S9xChooseMovieFilename (bool8 read_only){
    return NULL;
}

static void NSRTControllerSetup (void)
{
	if (!strncmp((const char *) Memory.NSRTHeader + 24, "NSRT", 4))
	{
		// First plug in both, they'll change later as needed
		S9xSetController(0, CTL_JOYPAD, 0, 0, 0, 0);
		S9xSetController(1, CTL_JOYPAD, 1, 0, 0, 0);

		switch (Memory.NSRTHeader[29])
		{
			case 0x00:	// Everything goes
				break;

			case 0x10:	// Mouse in Port 0
				S9xSetController(0, CTL_MOUSE,      0, 0, 0, 0);
				break;

			case 0x01:	// Mouse in Port 1
				S9xSetController(1, CTL_MOUSE,      1, 0, 0, 0);
				break;

			case 0x03:	// Super Scope in Port 1
				S9xSetController(1, CTL_SUPERSCOPE, 0, 0, 0, 0);
				break;

			case 0x06:	// Multitap in Port 1
				S9xSetController(1, CTL_MP5,        1, 2, 3, 4);
				break;

			case 0x66:	// Multitap in Ports 0 and 1
				S9xSetController(0, CTL_MP5,        0, 1, 2, 3);
				S9xSetController(1, CTL_MP5,        4, 5, 6, 7);
				break;

			case 0x08:	// Multitap in Port 1, Mouse in new Port 1
				S9xSetController(1, CTL_MOUSE,      1, 0, 0, 0);
				// There should be a toggle here for putting in Multitap instead
				break;

			case 0x04:	// Pad or Super Scope in Port 1
				S9xSetController(1, CTL_SUPERSCOPE, 0, 0, 0, 0);
				// There should be a toggle here for putting in a pad instead
				break;

			case 0x05:	// Justifier - Must ask user...
				S9xSetController(1, CTL_JUSTIFIER,  1, 0, 0, 0);
				// There should be a toggle here for how many justifiers
				break;

			case 0x20:	// Pad or Mouse in Port 0
				S9xSetController(0, CTL_MOUSE,      0, 0, 0, 0);
				// There should be a toggle here for putting in a pad instead
				break;

			case 0x22:	// Pad or Mouse in Port 0 & 1
				S9xSetController(0, CTL_MOUSE,      0, 0, 0, 0);
				S9xSetController(1, CTL_MOUSE,      1, 0, 0, 0);
				// There should be a toggles here for putting in pads instead
				break;

			case 0x24:	// Pad or Mouse in Port 0, Pad or Super Scope in Port 1
				// There should be a toggles here for what to put in, I'm leaving it at gamepad for now
				break;

			case 0x27:	// Pad or Mouse in Port 0, Pad or Mouse or Super Scope in Port 1
				// There should be a toggles here for what to put in, I'm leaving it at gamepad for now
				break;

			// Not Supported yet
			case 0x99:	// Lasabirdie
				break;

			case 0x0A:	// Barcode Battler
				break;
		}
	}
}

/*
 * domaemon: config is parsed as
 *
 * ParsePortConfig -> ParseInputConfig
 * ParsePortConfig -> ParseDisplayConfig
 */

void S9xParsePortConfig (ConfigFile &conf, int pass)
{
    #if 0
	s9x_base_dir                = conf.GetStringDup("Unix::BaseDir",             default_dir);
	sound_buffer_size           = conf.GetUInt     ("Unix::SoundBufferSize",     100);

	// domaemon: default input configuration
	S9xParseInputConfig(conf, 1);

	std::string section = S9xParseDisplayConfig(conf, 1);
    #endif
}


const char * S9xGetDirectory (enum s9x_getdirtype dirtype)
{
	static char	s[PATH_MAX + 1];

	if (dirNames[dirtype][0])
		snprintf(s, PATH_MAX + 1, "%s%s%s", s9x_base_dir, SLASH_STR, dirNames[dirtype]);
	else
	{
		switch (dirtype)
		{
			case DEFAULT_DIR:
				strncpy(s, s9x_base_dir, PATH_MAX + 1);
				s[PATH_MAX] = 0;
				break;

			case HOME_DIR:
				strncpy(s, getenv("HOME"), PATH_MAX + 1);
				s[PATH_MAX] = 0;
				break;

			case ROMFILENAME_DIR:
				strncpy(s, Memory.ROMFilename, PATH_MAX + 1);
				s[PATH_MAX] = 0;

				for (int i = strlen(s); i >= 0; i--)
				{
					if (s[i] == SLASH_CHAR)
					{
						s[i] = 0;
						break;
					}
				}

				break;

			default:
				s[0] = 0;
				break;
		}
	}

	return (s);
}

const char * S9xGetFilename (const char *ex, enum s9x_getdirtype dirtype)
{
	static char	s[PATH_MAX + 1];
	char		drive[_MAX_DRIVE + 1], dir[_MAX_DIR + 1], fname[_MAX_FNAME + 1], ext[_MAX_EXT + 1];

	_splitpath(Memory.ROMFilename, drive, dir, fname, ext);
	snprintf(s, PATH_MAX + 1, "%s%s%s%s", S9xGetDirectory(dirtype), SLASH_STR, fname, ex);

	return (s);
}

const char * S9xGetFilenameInc (const char *ex, enum s9x_getdirtype dirtype)
{
	static char	s[PATH_MAX + 1];
	char		drive[_MAX_DRIVE + 1], dir[_MAX_DIR + 1], fname[_MAX_FNAME + 1], ext[_MAX_EXT + 1];

	unsigned int	i = 0;
	const char		*d;
	struct stat		buf;

	_splitpath(Memory.ROMFilename, drive, dir, fname, ext);
	d = S9xGetDirectory(dirtype);

	do
		snprintf(s, PATH_MAX + 1, "%s%s%s.%03d%s", d, SLASH_STR, fname, i++, ex);
	while (stat(s, &buf) == 0 && i < 1000);

	return (s);
}

const char * S9xBasename (const char *f)
{
	const char	*p;

	if ((p = strrchr(f, '/')) != NULL || (p = strrchr(f, '\\')) != NULL)
		return (p + 1);

	return (f);
}

const char * S9xSelectFilename (const char *def, const char *dir1, const char *ext1, const char *title)
{
	static char	s[PATH_MAX + 1];
	char		buffer[PATH_MAX + 1];

	printf("\n%s (default: %s): ", title, def);
	fflush(stdout);

	if (fgets(buffer, PATH_MAX + 1, stdin))
	{
		char	drive[_MAX_DRIVE + 1], dir[_MAX_DIR + 1], fname[_MAX_FNAME + 1], ext[_MAX_EXT + 1];

		char	*p = buffer;
		while (isspace(*p))
			p++;
		if (!*p)
		{
			strncpy(buffer, def, PATH_MAX + 1);
			buffer[PATH_MAX] = 0;
			p = buffer;
		}

		char	*q = strrchr(p, '\n');
		if (q)
			*q = 0;

		_splitpath(p, drive, dir, fname, ext);
		_makepath(s, drive, *dir ? dir : dir1, fname, *ext ? ext : ext1);

		return (s);
	}

	return (NULL);
}

const char * S9xChooseFilename (bool8 read_only)
{
	char	s[PATH_MAX + 1];
	char	drive[_MAX_DRIVE + 1], dir[_MAX_DIR + 1], fname[_MAX_FNAME + 1], ext[_MAX_EXT + 1];

	const char	*filename;
	char		title[64];

	_splitpath(Memory.ROMFilename, drive, dir, fname, ext);
	snprintf(s, PATH_MAX + 1, "%s.frz", fname);
	sprintf(title, "%s snapshot filename", read_only ? "Select load" : "Choose save");

	S9xSetSoundMute(TRUE);
	filename = S9xSelectFilename(s, S9xGetDirectory(SNAPSHOT_DIR), "frz", title);
#ifdef SOUND
	S9xSetSoundMute(FALSE);
#else
    S9xSetSoundMute(TRUE);
#endif

	return (filename);
}


bool8 S9xOpenSnapshotFile (const char *filename, bool8 read_only, STREAM *file)
{
    printf("open snapshotfile\n");
    return FALSE;
}

void S9xCloseSnapshotFile (STREAM file)
{
    printf("close snapshotfile\n");
	//CLOSE_STREAM(file);
}

bool8 S9xInitUpdate (void)
{
	return (TRUE);
}

bool8 S9xDeinitUpdate (int width, int height)
{
	S9xPutImage(width, height);
	return (TRUE);
}

bool8 S9xContinueUpdate (int width, int height)
{
	return (TRUE);
}

void S9xAutoSaveSRAM (void)
{
		Memory.SaveSRAM(S9xGetFilename(".srm", SRAM_DIR));
    printf("auto save sram\n");
}
void S9xSyncSpeed (void)
{
#ifdef HTML
	IPPU.RenderThisFrame = (++IPPU.SkippedFrames >= Settings.SkipFrames) ? TRUE : FALSE;
        if (IPPU.RenderThisFrame)
		IPPU.SkippedFrames = 0;

#else
	static struct timeval	next1 = { 0, 0 };
	struct timeval			now;

	while (gettimeofday(&now, NULL) == -1) ;

	// If there is no known "next" frame, initialize it now.
	if (next1.tv_sec == 0)
	{
		next1 = now;
		next1.tv_usec++;
	}

	// If we're on AUTO_FRAMERATE, we'll display frames always only if there's excess time.
	// Otherwise we'll display the defined amount of frames.
	unsigned	limit = (Settings.SkipFrames == AUTO_FRAMERATE) ? (timercmp(&next1, &now, <) ? 10 : 1) : Settings.SkipFrames;

	IPPU.RenderThisFrame = (++IPPU.SkippedFrames >= limit) ? TRUE : FALSE;
    if (IPPU.RenderThisFrame)
		IPPU.SkippedFrames = 0;

	else
	{
		// If we were behind the schedule, check how much it is.
		if (timercmp(&next1, &now, <))
		{
			unsigned	lag = (now.tv_sec - next1.tv_sec) * 1000000 + now.tv_usec - next1.tv_usec;
			if (lag >= 500000)
			{
				// More than a half-second behind means probably pause.
				// The next line prevents the magic fast-forward effect.
				next1 = now;
			}
		}
	}

	// Delay until we're completed this frame.
	// Can't use setitimer because the sound code already could be using it. We don't actually need it either.

	while (timercmp(&next1, &now, >))
	{

		// If we're ahead of time, sleep a while.
		unsigned	timeleft = (next1.tv_sec - now.tv_sec) * 1000000 + next1.tv_usec - now.tv_usec;
		usleep(timeleft);

		while (gettimeofday(&now, NULL) == -1) ;
		// Continue with a while-loop because usleep() could be interrupted by a signal.
	}

	// Calculate the timestamp of the next frame.
	next1.tv_usec += Settings.FrameTime;
	if (next1.tv_usec >= 1000000)
	{
		next1.tv_sec += next1.tv_usec / 1000000;
		next1.tv_usec %= 1000000;
	}
#endif

}

void S9xExit (void)
{

	S9xSetSoundMute(TRUE);
	Settings.StopEmulation = TRUE;

#ifdef NETPLAY_SUPPORT
	if (Settings.NetPlay)
		S9xNPDisconnect();
#endif

  Memory.SaveSRAM(S9xGetFilename(".srm", SRAM_DIR));

#ifdef FANCY
	S9xSaveCheatFile(S9xGetFilename(".cht", CHEAT_DIR));
	S9xResetSaveTimer(FALSE);
#endif
	S9xUnmapAllControls();
	S9xDeinitDisplay();
	Memory.Deinit();
	S9xDeinitAPU();

	exit(0);
}

#ifdef DEBUGGER
static void sigbrkhandler (int)
{
	CPU.Flags |= DEBUG_MODE_FLAG;
	signal(SIGINT, (SIG_PF) sigbrkhandler);
}
#endif
void S9xParseArg (char **argv, int &i, int argc){
    printf("parse arg\n");
}


#ifdef HTML
extern "C" void toggle_display_framerate() __attribute__((used));
extern "C" void run(char*) __attribute__((used));
extern "C" int set_frameskip(int) __attribute__((used));
int set_frameskip(int n){
Settings.SkipFrames = n;
return n;
}
void toggle_display_framerate(){

    Settings.DisplayFrameRate = !Settings.DisplayFrameRate;
}
void mainloop(){
    S9xProcessEvents(FALSE);
    S9xMainLoop();
}
void reboot_emulator(char *filename){
  uint32 saved_flags = CPU.Flags;
	bool8	loaded = FALSE;
  loaded = Memory.LoadROM(filename);

	if (!loaded)
	{
		fprintf(stderr, "Error opening the ROM file. %s\n", filename);
		exit(1);
	}

	NSRTControllerSetup();

	printf("Attempting to load SRAM %s.\n", S9xGetFilename(".srm", SRAM_DIR));
	bool8 sramloaded = Memory.LoadSRAM(S9xGetFilename(".srm", SRAM_DIR));
	if (sramloaded)
	{
		printf("Load successful.\n");
	}
	else
	{
		printf("Load failed.\n");
	}

	CPU.Flags = saved_flags;
	Settings.StopEmulation = FALSE;

	S9xInitInputDevices('d', 'a', 's', 'w', 13, 1249, 'l', 'k', 'i', 'j', 'm', ';');
	S9xInitDisplay(NULL, NULL);
	sprintf(String, "\"%s\" %s: %s", Memory.ROMName, TITLE, VERSION);

    S9xSetTitle(String);
#ifdef SOUND
	S9xSetSoundMute(FALSE);
#else
    S9xSetSoundMute(TRUE);
#endif
}
extern "C" void* set_transparency(int i) __attribute__((used));
void* set_transparency(int i){
    Settings.Transparency=i;
    return (void *)&Settings;
}
void run(char *filename){
    reboot_emulator(filename);
    #ifdef SOUND
    printf("S9xSetSoundMute(FALSE)\n");
    S9xSetSoundMute(FALSE);
    #else
    printf("S9xSetSoundMute(TRUE)\n");
    S9xSetSoundMute(TRUE);
    #endif
    printf("start main loop\n");
	emscripten_cancel_main_loop();
    emscripten_set_main_loop(mainloop, 0, 0);
}


#endif


int main (int argc, char **argv)
{
	printf("\n\nSnes9x " VERSION " for unix/SDL\n");

	snprintf(default_dir, PATH_MAX + 1, "%s%s%s", getenv("HOME"), SLASH_STR, ".snes9x");
	s9x_base_dir = default_dir;

	EM_ASM(
		console.log('Syncing file system...');
		FS.mkdir('/home/web_user/.snes9x');
		FS.mkdir('/home/web_user/.snes9x/sram');
		FS.mount(IDBFS, {}, '/home/web_user/.snes9x/sram');
		FS.syncfs(true, function(err) {
			if (err) {
				console.log(err);
			} else {
				console.log('File system synced.');
				//window.initSNES();
				window.initSnesReady = true;
			}
		});
	);

	ZeroMemory(&Settings, sizeof(Settings));
	Settings.MouseMaster = TRUE;
	Settings.SuperScopeMaster = TRUE;
	Settings.JustifierMaster = TRUE;
	Settings.MultiPlayer5Master = FALSE;
	Settings.FrameTimePAL = 20000;
	Settings.FrameTimeNTSC = 16667;
	Settings.SixteenBitSound = TRUE;
	Settings.Stereo = TRUE;
	Settings.SupportHiRes = TRUE;
	Settings.Transparency = TRUE;
	Settings.AutoDisplayMessages = FALSE;
	Settings.InitialInfoStringTimeout = 120;
	Settings.HDMATimingHack = 100;
	Settings.BlockInvalidVRAMAccessMaster = TRUE;
	Settings.StopEmulation = TRUE;
	Settings.WrongMovieStateProtection = TRUE;
	Settings.DumpStreamsMaxFrames = -1;
	Settings.DisplayFrameRate = FALSE;
	Settings.AutoDisplayMessages = TRUE;
	Settings.StretchScreenshots = 1;
	Settings.SnapshotScreenshots = TRUE;
	Settings.SkipFrames = 0;
	Settings.TurboSkipFrames = 15;
	Settings.CartAName[0] = 0;
	Settings.CartBName[0] = 0;
	Settings.NoPatch= TRUE;
	Settings.SoundSync =  FALSE;
	#ifdef SOUND
		Settings.Mute = FALSE;
		Settings.SoundPlaybackRate = 22100;
		Settings.SoundInputRate = 22100;
	#else
		Settings.Mute = TRUE;
		Settings.SoundPlaybackRate = 16000;
		Settings.SoundInputRate = 16000;
	#endif
	CPU.Flags = 0;    ;
	if (!Memory.Init() || !S9xInitAPU()) {
		fprintf(stderr, "Snes9x: Memory allocation failure - not enough RAM/virtual memory available.\nExiting...\n");
		Memory.Deinit();
		S9xDeinitAPU();
		exit(1);
	}
    sound_buffer_size = 100;
	S9xInitSound(sound_buffer_size, 0);
	S9xSetSoundMute(TRUE);

	S9xReportControllers();

	#ifdef GFX_MULTI_FORMAT
		S9xSetRenderPixelFormat(RGB565);
	#endif
	// domaemon: setting the title on the window bar

	#ifdef HTML
    	emscripten_exit_with_live_runtime();
	#else
    uint32	saved_flags = CPU.Flags;
	bool8	loaded = FALSE;

	if (rom_filename) {
		loaded = Memory.LoadROM(rom_filename);
	}

	if (!loaded) {
		fprintf(stderr, "Error opening the ROM file.\n");
		exit(1);
	}

	NSRTControllerSetup();

	printf("Attempting to load SRAM %s.\n", S9xGetFilename(".srm", SRAM_DIR));
	bool8 sramloaded = Memory.LoadSRAM(S9xGetFilename(".srm", SRAM_DIR));
	if (sramloaded) {
		printf("Load successful.\n");
	} else {
		printf("Load failed.\n");
	}

	CPU.Flags = saved_flags;
	Settings.StopEmulation = FALSE;

	S9xInitInputDevices('d', 'a', 's', 'w', 13, 1249, 'l', 'k', 'i', 'j', 'm', ';');
	S9xInitDisplay(argc, argv);
	sprintf(String, "\"%s\" %s: %s", Memory.ROMName, TITLE, VERSION);

    S9xSetTitle(String);
	#ifdef SOUND
		S9xSetSoundMute(FALSE);
	#else
    	S9xSetSoundMute(TRUE);
	#endif
    printf("before start\n");
    printf("registers.pcw=%x\n", Registers.PCw);
	for (int iters = 0; iters < 5000; iters++){
        S9xMainLoop();
        S9xProcessEvents(FALSE);
	}

#endif
	return (0);
}
