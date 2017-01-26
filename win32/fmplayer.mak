TARGET=fmplayer.exe

ICON=../fmplayer.ico
ICONFILES=../fmplayer.png ../fmplayer32.png

DEFINES=UNICODE _UNICODE \
        WINVER=0x0500 _WIN32_WINNT=0x0500 \
        DIRECTSOUND_VERSION=0x0800

FMDRIVER_OBJS=fmdriver_fmp \
              ppz8
LIBOPNA_OBJS=opna \
             opnatimer \
             opnafm \
             opnassg \
             opnadrum \
             opnaadpcm
FMDSP_OBJS=fmdsp \
           font_rom
OBJBASE=main \
        soundout \
        dsoundout \
        waveout \
        winfont \
        $(FMDRIVER_OBJS) \
        $(LIBOPNA_OBJS) \
        $(FMDSP_OBJS)
RESBASE=lnf
LIBBASE=user32 \
        kernel32 \
        ole32 \
        dxguid \
        uuid \
        comdlg32 \
        gdi32 \
        shlwapi \
        winmm \
        shell32

