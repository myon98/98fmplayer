vpath %.c ../
vpath %.c ../../fmdriver
vpath %.c ../../libopna
vpath %.c ../../fmdsp
vpath %.rc ..

include ../fmplayer.mak

OBJS=$(addsuffix .o,$(OBJBASE) $(RESBASE))
ARCH=x86_64
PREFIX=$(ARCH)-w64-mingw32-
CC=$(PREFIX)gcc
WINDRES=$(PREFIX)windres
STRIP=$(PREFIX)strip
CFLAGS=-std=c99 -Os -Wall -Wextra -pedantic -I../.. \
       $(addprefix -D,$(DEFINES))
LIBS=-s -mwindows -municode \
     $(addprefix -l,$(LIBBASE))

$(TARGET):	$(OBJS)
	@echo "  CCLD     $@"
	@$(CC) -o $@ $(OBJS) $(LIBS)
	@$(STRIP) $@

%.o:	%.c
	@echo "  CC       $@"
	@$(CC) $(CFLAGS) -c $< -o $@

%.o:	%.rc $(ICON)
	@echo "  WINDRES  $@"
	@$(WINDRES) -o $@ -i $<

$(ICON):	$(ICONFILES)
	convert $(ICONFILES) -colors 256 $@

clean:
	rm -f $(TARGET) $(OBJS)
