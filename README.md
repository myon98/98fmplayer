# 98Fmplayer (beta)
PC-98 FM driver emulation (very early version)
![gtk screenshot](/img/screenshot_gtk.png?raw=true)
![gtk toneviewer screenshot](/img/screenshot_gtk.toneview.png?raw=true)
![gtk config screenshot](/img/screenshot_gtk.config.png?raw=true)
![w2k screenshot](/img/screenshotw2k.png?raw=true)

*If you are just annoyed by some specific bugs in PMDWin, [patched PMDWin](https://github.com/takamichih/pmdwinbuild) might have less bugs and more features than this.*

## Current status:
* Supported formats: PMD, FMP(PLAY6)
* PMD: FM, SSG, Rhythm, ADPCM, PPZ8(partially) supported; PPS, P86 not supported yet
* FMP: FM, SSG, Rhythm, ADPCM, PPZ8, PDZF supported
* This is just a byproduct of reverse-engineering formats, and its emulation is much worse than PMDWin, WinFMP
* FM always generated in 55467Hz (closest integer to 7987200 / 144), SSG always generated in 249600Hz and downsampled with sinc filter (Never linear interpolates harmonics-rich signal like square wave)
* FM generation bit-perfect with actual OPNA/OPN3 chip under limited conditions including stereo output when 4 <= ALG (Envelope is not bit-perfect yet, attack is bit-perfect only when AR >= 21)
* SSGEG, Hardware LFO not supported
* PPZ8: support nearest neighbor, linear and sinc interpolation
* ADPCM: inaccurate (actual YM2608 seems to decode ADPCM at lower samplerate/resolution than any YM2608 emulator around, but I still couldn't get my YM2608 work with the DRAM)

## Installation/Usage (not very usable yet)
### gtk
Uses gtk3, pulseaudio/jack/alsa
```
$ cd gtk
$ autoreconf -i
$ ./configure
$ make
$ ./98fmplayer
```
Reads drum sample from `$HOME/.local/share/98fmplayer/ym2608_adpcm_rom.bin` (same format as MAME).

### win32
Releases:
https://github.com/takamichih/fmplayer/releases/

Uses MinGW-w64 to compile.
```
$ cd win32/x86
$ make
```
Reads drum sample from the directory in which `98fmplayer.exe` is placed.
Uses DirectSound (WinMM if there is no DirectSound) to output sound. This works on Windows 2000, so it is  theoretically possible to run this on a real PC-98. (But it was too heavy for my PC-9821V12 which only has P5 Pentium 120MHz, or on PC-9821Ra300 with P6 Mendocino Celeron 300MHz)
