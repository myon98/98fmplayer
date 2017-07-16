vpath %.c ../fmdriver
vpath %.c ../libopna
vpath %.c ../fmdsp
vpath %.c ../fft
vpath %.c ../soundout
vpath %.c ../common

TARGET:=fmplayer
OBJS:=main.o fmdsp_gl.o
OBJS+=fmdriver_common.o fmdriver_pmd.o fmdriver_fmp.o ppz8.o
OBJS+=opna.o opnafm.o opnassg.o opnassg-sinc-c.o opnadrum.o opnaadpcm.o opnatimer.o
OBJS+=fmdsp.o font_rom.o font_fmdsp_small.o fmdsp-vramlookup-c.o
OBJS+=fmdsp_platform_unix.o fft.o fmplayer_file_unix.o fmplayer_drumrom_unix.o fmplayer_file.o fmplayer_work_opna.o
OBJS+=soundout.o jackout.o
CFLAGS:=-std=c99 -Wall -Wextra -pedantic-errors -O2 -I.. -I. -pthread -I../soundout -DENABLE_JACK
LIBS:=-lglfw -lasound -pthread -lGL -lm -lsoxr -ljack

$(TARGET):	$(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f $(TARGET) $(OBJS)
