vpath %.c ../
vpath %.c ../../fmdriver
vpath %.c ../../libopna
vpath %.c ../../fmdsp
vpath %.rc ..
TARGET=fmplayer.exe
ICON=../fmplayer.ico
ICONFILES=../fmplayer.png ../fmplayer32.png
FMDRIVER_OBJS=fmdriver_fmp.o \
              ppz8.o
LIBOPNA_OBJS=opna.o \
             opnatimer.o \
             opnafm.o \
             opnassg.o \
             opnadrum.o \
             opnaadpcm.o
FMDSP_OBJS=fmdsp.o font_rom.o
OBJS=main.o dsoundout.o soundout.o waveout.o uc.o lnf.o winfont.o \
     $(FMDRIVER_OBJS) \
     $(LIBOPNA_OBJS) \
     $(FMDSP_OBJS)
ARCH=x86_64
PREFIX=$(ARCH)-w64-mingw32-
CC=$(PREFIX)gcc
WINDRES=$(PREFIX)windres
STRIP=$(PREFIX)strip
CFLAGS=-std=c99 -Os -Wall -Wextra -pedantic -I../.. \
       -DUNICODE -D_UNICODE \
       -DWINVER=0x0500 -D_WIN32_WINNT=0x0500
LIBS=-nostdlib -s -Wl,-eentry \
     -Wl,--subsystem,windows \
     -lgcc -lntdll \
     -luser32 -lkernel32 -lole32 -ldxguid -luuid -lcomdlg32 \
     -lgdi32 -lshlwapi -lwinmm -lshell32

$(TARGET):	$(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBS)
	$(STRIP) $@

%.o:	%.rc $(ICON)
	$(WINDRES) -o $@ -i $<

$(ICON):	$(ICONFILES)
	convert $(ICONFILES) -colors 256 $@

clean:
	rm -f $(TARGET) $(OBJS)
