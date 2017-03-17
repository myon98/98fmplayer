# Fmplayer (beta)
PC-98 FM driver emulation (very early version)

## Current status:
* Supported formats: PMD, FMP(PLAY6)
* PMD: FM, SSG, Rhythm part supported; ADPCM, PPZ8 not supported yet
* FMP: FM, SSG, Rhythm, ADPCM, PPZ8, PDZF supported
* This is just a byproduct of reverse-engineering formats, and its emulation is much worse than PMDWin, WinFMP
* FM always generated in 55467Hz (closest integer to 7987200 / 144), SSG always generated in 249600Hz and downsampled with sinc filter (Never linear interpolates harmonics-rich signal like square wave)
* SSGEG, Hardware LFO not supported
* PPZ8: linear interpolation only (same as PMDWin/WinFMP, much better than original ppz8.com which only did nearest-neighbor interpolation)

## Installation/Usage (not very usable yet)
### gtk
Uses gtk3, portaudio
```
$ cd gtk
$ autoreconf -i
$ ./configure
$ make
$ ./fmplayer
```
Reads drum sample from `$HOME/.local/share/fmplayer/ym2608_adpcm_rom.bin` (same format as MAME).
Currently needs `$HOME/.local/share/fmplayer/font.rom` to display titles/comments.

### win32
version 0.1.0:
https://github.com/takamichih/fmplayer/releases/tag/v0.1.0

Uses MinGW-w64 to compile.
```
$ cd win32/x86
$ make
```
Reads drum sample from the directory in which `fmplayer.exe` is placed.
Uses DirectSound (WinMM if there is no DirectSound) to output sound. This works on Windows 2000, so it is  theoretically possible to run this on a real PC-98. (But it was too heavy for my PC-9821V12 which only has Pentium 120MHz)
