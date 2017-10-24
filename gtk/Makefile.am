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

FMDSP_SRC=../fmdsp/fmdsp-pacc.c \
          ../fmdsp/font_rom.c \
          ../fmdsp/font_fmdsp_small.c \
          ../fmdsp/fmdsp_platform_unix.c \
          ../pacc/pacc-gl.c

SOUNDOUT_SRC=../soundout/soundout.c

if ENABLE_JACK
SOUNDOUT_SRC+=../soundout/jackout.c
endif
if ENABLE_PULSE
SOUNDOUT_SRC+=../soundout/pulseout.c
endif
if ENABLE_ALSA
SOUNDOUT_SRC+=../soundout/alsaout.c
endif

#fmplayer_CFLAGS=$(CFLAGS)
#CFLAGS=
fmplayer_CPPFLAGS=-Wall -Wextra -pedantic \
                  -I$(top_srcdir)/.. -I$(top_srcdir)/../soundout \
                  $(GTK3_CFLAGS) $(JACK_CFLAGS) $(PULSE_CFLAGS) $(ALSA_CFLAGS) $(SNDFILE_CFLAGS) \
                  -DPACC_GL_3
fmplayer_LDADD=$(GTK3_LIBS) $(JACK_LIBS) $(PULSE_LIBS) $(ALSA_LIBS) $(SNDFILE_LIBS) -lm -lpthread

if ENABLE_NEON
LIBOPNA_SRC+=../libopna/opnassg-sinc-neon.s
FMDSP_SRC+=../fmdsp/fmdsp-vramlookup-neon.s
fmplayer_CCASFLAGS=-march=armv8-a -mfpu=crypto-neon-fp-armv8
endif

if ENABLE_SSE
noinst_LIBRARIES=libsse.a
fmplayer_LDADD+=libsse.a
libsse_a_SOURCES=../libopna/opnassg-sinc-sse2.c
libsse_a_CPPFLAGS=$(fmplayer_CPPFLAGS)
libsse_a_CFLAGS=-msse2
endif

fmplayer_SOURCES=main.c \
                 toneview.c \
                 wavesave.c \
                 ../tonedata/tonedata.c \
                 ../common/fmplayer_file.c \
                 ../common/fmplayer_file_gio.c \
                 ../common/fmplayer_work_opna.c \
                 ../common/fmplayer_drumrom_unix.c \
                 ../common/fmplayer_fontrom_unix.c \
                 ../fft/fft.c \
                 $(LIBOPNA_SRC) \
                 $(FMDRIVER_SRC) \
                 $(FMDSP_SRC) \
                 $(SOUNDOUT_SRC) \
								 configdialog.c

if ENABLE_OPENGL
fmplayer_SOURCES+=oscilloview-gl.c
fmplayer_LDADD+=-lGL
else
fmplayer_SOURCES+=oscilloview.c
endif
