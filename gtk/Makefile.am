bin_PROGRAMS=fmplayer

LIBOPNA_SRC=../libopna/opnaadpcm.c \
                ../libopna/opnadrum.c \
                ../libopna/opnafm.c \
                ../libopna/opnassg.c \
                ../libopna/opnatimer.c \
                ../libopna/opna.c

FMDRIVER_SRC=../fmdriver/fmdriver_fmp.c \
             ../fmdriver/ppz8.c

FMDSP_SRC=../fmdsp/fmdsp.c \
          ../fmdsp/font_rom.c \
          ../fmdsp/font_fmdsp_small.c

fmplayer_SOURCES=main.c \
                 toneview.c \
                 ../tonedata/tonedata.c \
                 $(LIBOPNA_SRC) \
                 $(FMDRIVER_SRC) \
                 $(FMDSP_SRC)

fmplayer_CPPFLAGS=-Wall -Wextra -pedantic \
                  -I.. \
                  $(GTK3_CFLAGS) $(PORTAUDIO_CFLAGS)
fmplayer_LDADD=$(GTK3_LIBS) $(PORTAUDIO_LIBS)
