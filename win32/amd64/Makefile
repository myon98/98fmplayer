vpath %.c ../
vpath %.c ../../fmdriver
vpath %.c ../../libopna
vpath %.c ../../fmdsp
vpath %.c ../../tonedata
vpath %.c ../../common
vpath %.c ../../fft
vpath %.c ../../pacc
vpath %.rc ..

include ../fmplayer.mak

OBJS=$(addsuffix .o,$(OBJBASE) $(RESBASE))
OBJS+=$(addsuffix .sse.o,$(SSEOBJBASE))
ARCH=x86_64
PREFIX=$(ARCH)-w64-mingw32-
CC=$(PREFIX)gcc
WINDRES=$(PREFIX)windres
STRIP=$(PREFIX)strip
CFLAGS=-std=c99 -O2 -Wall -Werror -Wextra -pedantic -I../.. \
       $(addprefix -D,$(DEFINES))
SSECFLAGS=-mssse3 -O3
LIBS=-mwindows -municode \
     $(addprefix -l,$(LIBBASE))

$(TARGET):	$(OBJS)
	@echo "  CCLD     $@"
	@$(CC) -o $@ $(OBJS) $(LIBS)
	@$(STRIP) $@

%.o:	%.c
	@echo "  CC       $@"
	@$(CC) $(CFLAGS) -c $< -o $@

%.sse.o:	%.c
	@echo "  CC       $@"
	@$(CC) $(CFLAGS) $(SSECFLAGS) -c $< -o $@

%.o:	%.rc $(ICON) $(MANIFEST)
	@echo "  WINDRES  $@"
	@$(WINDRES) -o $@ -i $<

$(MANIFEST):    $(MANIFEST).in ../../version.h
	sed -e "s/@VER@/$(shell ../../versionprint.c)/g" $< > $@

$(ICON):	$(ICONFILES)
	icotool -o $@ -c $^

clean:
	rm -f $(TARGET) $(OBJS)
