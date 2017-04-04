bin_PROGRAMS=fmplayer

LIBOPNA_SRC=../libopna/opnaadpcm.c \
            ../libopna/opnadrum.c \
            ../libopna/opnafm.c \
            ../libopna/opnassg.c \
            ../libopna/opnassg-sinc-c.c \
            ../libopna/opnatimer.c \
            ../libopna/opna.c

FMDRIVER_SRC=../fmdriver/fmdriver_fmp.c \
             ../fmdriver/fmdriver_pmd.c \
             ../fmdriver/fmdriver_common.c \
             ../fmdriver/ppz8.c

FMDSP_SRC=../fmdsp/fmdsp.c \
          ../fmdsp/fmdsp-vramlookup-c.c \
          ../fmdsp/font_rom.c \
          ../fmdsp/font_fmdsp_small.c

#fmplayer_CFLAGS=$(CFLAGS)
#CFLAGS=
fmplayer_CPPFLAGS=-Wall -Wextra -pedantic \
                  -I.. \
                  $(GTK3_CFLAGS) $(PORTAUDIO_CFLAGS)
fmplayer_LDADD=$(GTK3_LIBS) $(PORTAUDIO_LIBS)

if ENABLE_NEON
LIBOPNA_SRC+=../libopna/opnassg-sinc-neon.s
FMDSP_SRC+=../fmdsp/fmdsp-vramlookup-neon.s
fmplayer_CCASFLAGS=-march=armv8-a -mfpu=crypto-neon-fp-armv8
endif

if ENABLE_SSE
noinst_LIBRARIES=libsse.a
fmplayer_LDADD+=libsse.a
libsse_a_SOURCES=../libopna/opnassg-sinc-sse2.c \
                 ../fmdsp/fmdsp-vramlookup-ssse3.c
libsse_a_CPPFLAGS=$(fmplayer_CPPFLAGS)
libsse_a_CFLAGS=-mssse3
endif

fmplayer_SOURCES=main.c \
                 toneview.c \
                 ../tonedata/tonedata.c \
                 ../common/fmplayer_file.c \
                 ../common/fmplayer_file_gio.c \
                 $(LIBOPNA_SRC) \
                 $(FMDRIVER_SRC) \
                 $(FMDSP_SRC)

if ENABLE_OPENGL
fmplayer_SOURCES+=oscilloview-gl.c
fmplayer_LDADD+=-lGL
else
fmplayer_SOURCES+=oscilloview.c
endif
