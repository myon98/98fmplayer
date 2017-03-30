ACLOCAL_AMFLAGS=-Im4
bin_PROGRAMS=fmpc

LIBOPNA_SOURCES=../libopna/opnaadpcm.c \
                ../libopna/opnadrum.c \
                ../libopna/opnafm.c \
                ../libopna/opnassg.c \
                ../libopna/opnatimer.c \
                ../libopna/opna.c

FMDRIVER_SOURCES=../fmdriver/fmdriver_fmp.c \
                 ../fmdriver/fmdriver_common.c \
                 ../fmdriver/ppz8.c
fmpc_SOURCES=main.c \
             $(LIBOPNA_SOURCES) \
             $(FMDRIVER_SOURCES)

fmpc_CFLAGS=-Wall -Wextra -pedantic-errors \
            -I.. $(PORTAUDIO_CFLAGS) $(CURSES_CFLAGS) $(SAMPLERATE_CFLAGS)
fmpc_LDADD=$(PORTAUDIO_LIBS) $(CURSES_LIBS) $(LIBICONV) $(SAMPLERATE_LIBS)
