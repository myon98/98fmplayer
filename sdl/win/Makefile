vpath %.c ..
vpath %.c ../../pacc
vpath %.c ../../fmdsp
vpath %.c ../../libopna
vpath %.c ../../common
vpath %.c ../../fmdriver
vpath %.c ../../fft
#ARCH=x86_64
ARCH=i686
SDLCONFIG:=$(ARCH)-w64-mingw32-sdl2-config
CC:=$(ARCH)-w64-mingw32-gcc
OBJS:=main.o
OBJS+=pacc-gl.o
OBJS+=fmdsp-pacc.o font_fmdsp_small.o fmdsp_platform_win.o font_rom.o
OBJS+=opna.o opnafm.o opnassg.o opnadrum.o opnaadpcm.o opnatimer.o opnassg-sinc-c.o opnassg-sinc-sse2.o
OBJS+=fmdriver_pmd.o fmdriver_fmp.o ppz8.o fmdriver_common.o
OBJS+=fmplayer_file.o fmplayer_work_opna.o fmplayer_file_win.o fmplayer_drumrom_win.o fmplayer_fontrom_win.o winfont.o
OBJS+=fft.o
TARGET:=98fmplayersdl.exe

CFLAGS:=-Wall -Wextra -O2
CFLAGS+=-DLIBOPNA_ENABLE_LEVELDATA
CFLAGS+=-DFMPLAYER_FILE_WIN_UTF8
CFLAGS+=-DPACC_GL_3
#CFLAGS+=-DPACC_GL_ES
#CFLAGS+=-DPACC_GL_ES -DPACC_GL_3
CFLAGS+=-I.. -I../..
CFLAGS+=$(shell $(SDLCONFIG) --cflags)
LIBS:=-lshlwapi
LIBS+=-static $(shell $(SDLCONFIG) --static-libs) -lopengl32

$(TARGET):	$(OBJS)
	$(CC) -o $@ $^ $(LIBS)

clean:
	rm -f $(TARGET) $(OBJS)

opnassg-sinc-sse2.o:	opnassg-sinc-sse2.c
	$(CC) -c $< $(CFLAGS) -msse2
