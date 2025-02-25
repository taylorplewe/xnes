#!/bin/sh
set -o verbose
OUT=../../output
OBJECTS="sdlmain.cpp sdlinput.cpp sdlvideo.cpp sdlaudio.cpp ../apu/apu.cpp ../apu/SNES_SPC.cpp ../apu/SNES_SPC_misc.cpp ../apu/SNES_SPC_state.cpp ../apu/SPC_DSP.cpp ../apu/SPC_Filter.cpp ../bsx.cpp ../c4.cpp ../c4emu.cpp  ../clip.cpp  ../controls.cpp ../cpu.cpp ../cpuexec.cpp ../cpuops.cpp ../dma.cpp ../dsp.cpp ../dsp1.cpp ../dsp2.cpp ../dsp3.cpp ../dsp4.cpp ../fxinst.cpp ../fxemu.cpp ../gfx.cpp ../globals.cpp  ../memmap.cpp  ../obc1.cpp ../ppu.cpp ../reader.cpp ../sa1.cpp ../sa1cpu.cpp  ../sdd1.cpp ../sdd1emu.cpp ../seta.cpp ../seta010.cpp ../seta011.cpp ../seta018.cpp  ../snes9x.cpp ../spc7110.cpp ../srtc.cpp ../tile.cpp"
INCLUDES="-I. -I.. -I../apu/"
CCFLAGS="-U__linux -O3 -DLSB_FIRST  -fomit-frame-pointer -fno-exceptions -fno-rtti -pedantic -Wall -W -Wno-unused-parameter -I/usr/include/SDL -D_GNU_SOURCE=1 -D_REENTRANT -DHAVE_MKSTEMP -DHAVE_STRINGS_H -DHAVE_SYS_IOCTL_H -DHAVE_STDINT_H -DRIGHTSHIFT_IS_SAR -Wno-c++11-extensions"

emcc -O3 -s EXPORTED_FUNCTIONS="['_main', '_set_frameskip', '_set_transparency', '_run',  '_toggle_display_framerate', '_S9xAutoSaveSRAM', '_S9xReportButton' ]" \
 -s FORCE_FILESYSTEM=1 \
 --shell-file modern-ui-shell.html \
 -o $OUT/snes9x.html \
 -DUSE_SDL \
 -DHTML $INCLUDES $CCFLAGS $OBJECTS \
 -lm -lSDL -s TOTAL_MEMORY=50331648  \
 -DSOUND \
 -sEXPORTED_RUNTIME_METHODS=ccall,cwrap \
 -s MODULARIZE=1 \
 -s EXPORT_ES6 \
 -s ENVIRONMENT='web' \
 -lidbfs.js \
 --use-port=sdl2 \
 -v
