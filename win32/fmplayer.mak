TARGET=fmplayer.exe

ICON=../fmplayer.ico
ICONFILES=../fmplayer.png ../fmplayer32.png

DEFINES=UNICODE _UNICODE \
        WINVER=0x0500 _WIN32_WINNT=0x0500 \
        DIRECTSOUND_VERSION=0x0800

FMDRIVER_OBJS=fmdriver_pmd \
              fmdriver_fmp \
              fmdriver_common \
              ppz8
LIBOPNA_OBJS=opna \
             opnatimer \
             opnafm \
             opnassg \
             opnadrum \
             opnaadpcm
FMDSP_OBJS=fmdsp \
           font_rom \
           font_fmdsp_small
TONEDATA_OBJS=tonedata
OBJBASE=main \
        toneview \
        oscilloview \
        soundout \
        dsoundout \
        waveout \
        winfont \
        guid \
        fmplayer_file \
        fmplayer_file_win \
        $(FMDRIVER_OBJS) \
        $(LIBOPNA_OBJS) \
        $(TONEDATA_OBJS) \
        $(FMDSP_OBJS)
RESBASE=lnf
LIBBASE=user32 \
        kernel32 \
        ole32 \
        uuid \
        comdlg32 \
        gdi32 \
        shlwapi \
        winmm \
        shell32 \
        ksuser

