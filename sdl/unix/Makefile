vpath %.c ..
vpath %.c ../../pacc
vpath %.c ../../fmdsp
vpath %.c ../../libopna
vpath %.c ../../common
vpath %.c ../../fmdriver
vpath %.c ../../fft
SDLCONFIG:=sdl2-config
OBJS:=main.o
OBJS+=pacc-gl.o
OBJS+=fmdsp-pacc.o font_fmdsp_small.o fmdsp_platform_unix.o font_rom.o
OBJS+=opna.o opnafm.o opnassg.o opnadrum.o opnaadpcm.o opnatimer.o opnassg-sinc-c.o opnassg-sinc-sse2.o
OBJS+=fmdriver_pmd.o fmdriver_fmp.o ppz8.o fmdriver_common.o
OBJS+=fmplayer_file.o fmplayer_work_opna.o fmplayer_file_unix.o fmplayer_drumrom_unix.o fmplayer_fontrom_unix.o
OBJS+=fft.o
TARGET:=98fmplayersdl

CFLAGS:=-Wall -Wextra -O2 -g
CFLAGS+=-DLIBOPNA_ENABLE_LEVELDATA
CFLAGS+=-DPACC_GL_3
#CFLAGS+=-DPACC_GL_ES
#CFLAGS+=-DPACC_GL_ES -DPACC_GL_3
CFLAGS+=-I.. -I../..
CFLAGS+=$(shell $(SDLCONFIG) --cflags)
LIBS:=-lGL -lm
LIBS+=$(shell $(SDLCONFIG) --libs)

$(TARGET):	$(OBJS)
	$(CC) -o $@ $^ $(LIBS)

clean:
	rm -f $(TARGET) $(OBJS)

