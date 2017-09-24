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
ARCH=i686
PREFIX=$(ARCH)-w64-mingw32-
CC=$(PREFIX)gcc
WINDRES=$(PREFIX)windres
STRIP=$(PREFIX)strip
CFLAGS=-std=c99 -O2 -Wall -Wextra -Werror -pedantic -I../.. \
       $(addprefix -D,$(DEFINES)) \
       -march=i586 -Wno-unused-parameter -Wno-missing-field-initializers
SSECFLAGS=-mssse3 -O3
LIBS=-s -mwindows -municode \
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

%.o:	%.rc $(ICON)
	@echo "  WINDRES  $@"
	@$(WINDRES) -o $@ -i $<

$(ICON):	$(ICONFILES)
	convert $(ICONFILES) -colors 256 $@

clean:
	rm -f $(TARGET) $(OBJS)
