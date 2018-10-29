vpath %.c ..
vpath %.c ../../pacc
vpath %.c ../../fmdsp
vpath %.c ../../libopna
vpath %.c ../../common
vpath %.c ../../fmdriver
vpath %.c ../../fft
#XCRUN:=xcrun --sdk /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.11.sdk/
XCRUN:=xcrun
CC:=$(XCRUN) cc
OBJS:=main.o
OBJS+=pacc-gl.o
OBJS+=fmdsp-pacc.o font_fmdsp_small.o fmdsp_platform_mach.o font_rom.o
OBJS+=opna.o opnafm.o opnassg.o opnadrum.o opnaadpcm.o opnatimer.o opnassg-sinc-c.o opnassg-sinc-sse2.o
OBJS+=fmdriver_pmd.o fmdriver_fmp.o ppz8.o fmdriver_common.o
OBJS+=fmplayer_file.o fmplayer_work_opna.o fmplayer_file_unix.o fmplayer_drumrom_unix.o fmplayer_fontrom_unix.o
OBJS+=fft.o
TARGET:=98fmplayersdl
CFLAGS:=-Wall -Wextra -O2 -g
CFLAGS:=-DLIBOPNA_ENABLE_LEVELDATA
CFLAGS+=-DPACC_GL_3
#CFLAGS+=-DPACC_GL_ES
#CFLAGS+=-DPACC_GL_ES -DPACC_GL_3
CFLAGS+=-I. -I.. -I../..
SDLFW:=/Library/Frameworks/SDL2.framework
CFLAGS+=-I$(SDLFW)/Headers
LIBS:=-framework SDL2 -framework OpenGL -liconv
LIBS+=-F/Library/Frameworks

$(TARGET):	$(OBJS)
	$(CC) -o $@ $^ $(LIBS)

clean:
	rm -rf $(TARGET).app $(TARGET) $(OBJS)

bundle:	$(TARGET)
	rm -rf $(TARGET).app
	mkdir -p $(TARGET).app/Contents/MacOS
	cp Info.plist $(TARGET).app/Contents
	cp $(TARGET) $(TARGET).app/Contents/MacOS
	mkdir $(TARGET).app/Contents/Frameworks
	cp -r $(SDLFW) $(TARGET).app/Contents/Frameworks
