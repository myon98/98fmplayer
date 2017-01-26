vpath %.c ../
vpath %.c ../../fmdriver
vpath %.c ../../libopna
vpath %.c ../../fmdsp
vpath %.rc ..

include ../fmplayer.mak

OBJS=$(addsuffix .obj,$(OBJBASE)) $(addsuffix .res,$(RESBASE))
CC=cl /nologo
RC=rc /nologo
CFLAGS=/W2 /Os /Oi /MT /I..\.. \
       $(addprefix /D,$(DEFINES))
LIBS=$(addsuffix .lib,$(LIBBASE)) \
     /link /SUBSYSTEM:WINDOWS

$(TARGET):	$(OBJS)
	@echo   CCLD     $@
	@$(CC) $(LDFLAGS) /Fe$@ $(OBJS) $(LIBS)

%.obj:	%.c
	@echo   CC       $@
	@$(CC) $(CFLAGS) /c $< /Fo$@

%.res:	%.rc $(ICON)
	@echo   RC       $@
	@$(RC) /fo$@ $<

clean:
	-del $(TARGET) $(OBJS)
