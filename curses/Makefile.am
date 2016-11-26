bin_PROGRAMS=fmpc

LIBOPNA_SOURCES=../libopna/opnaadpcm.c \
                ../libopna/opnadrum.c \
                ../libopna/opnafm.c \
                ../libopna/opnassg.c \
                ../libopna/opnatimer.c \
                ../libopna/opna.c

FMDRIVER_SOURCES=../fmdriver/fmdriver_fmp.c \
                 ../fmdriver/ppz8.c
fmpc_SOURCES=main.c \
             $(LIBOPNA_SOURCES) \
             $(FMDRIVER_SOURCES)

fmpc_CFLAGS=-Wall -Wextra -pedantic \
            -I.. $(SDL_CFLAGS) $(NCURSES_CFLAGS)
fmpc_LDADD=$(SDL_LIBS) $(NCURSES_LIBS)
