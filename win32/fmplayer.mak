TARGET=98fmplayer.exe

ICON=../fmplayer.ico
ICONFILES=../fmplayer.png ../fmplayer32.png
MANIFEST=../lnf.manifest

DEFINES=UNICODE _UNICODE \
        WINVER=0x0500 _WIN32_WINNT=0x0500 \
        DIRECTSOUND_VERSION=0x0800 FMPLAYER_FILE_WIN_UTF16 \
	LIBOPNA_ENABLE_LEVELDATA LIBOPNA_ENABLE_OSCILLO

FMDRIVER_OBJS=fmdriver_pmd \
              fmdriver_fmp \
              fmdriver_common \
              ppz8
LIBOPNA_OBJS=opna \
             opnatimer \
             opnafm \
             opnassg \
             opnassg-sinc-c \
             opnadrum \
             opnaadpcm
FMDSP_OBJS=fmdsp-pacc \
	   pacc-d3d9 \
	   fmdsp_platform_win \
	   font_fmdsp_small
TONEDATA_OBJS=tonedata
SSEOBJBASE=opnassg-sinc-sse2
OBJBASE=main \
        fft \
        toneview \
        oscilloview \
        wavesave \
        wavewrite \
        soundout \
        dsoundout \
        waveout \
        winfont \
        guid \
        fmplayer_file \
        fmplayer_file_win \
        fmplayer_drumrom_win \
        fmplayer_fontrom_win \
        font_rom \
        fmplayer_work_opna \
        about \
        $(FMDRIVER_OBJS) \
        $(LIBOPNA_OBJS) \
        $(TONEDATA_OBJS) \
        $(FMDSP_OBJS) \
				configdialog
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

