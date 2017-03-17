# Fmplayer (仮)
PC-98用のFM音源ドライバエミュレーション(予定)

## 現在の状況:
* 対応ドライバ: PMD, FMP (PLAY6含む)
* PMD: FM, SSG, リズムパートのみ対応、ADPCM, PPZ8 対応予定
* FMP: PPZ8, PDZF にほぼ対応
* UI: GTK3 で作った仮のものと、仮 Win32 版
![ncurses screenshot](/img/screenshot_ncurses.png?raw=true)
![gtk screenshot](/img/screenshot_gtk.png?raw=true)
* PMD, FMP の形式を解析するついでに作ったもので、手持ちのデータが少ないため再現性は PMDWin, WinFMP の劣化版
* FM は 55467Hz で合成, SSG は 249600Hz で合成した後 sinc でフィルタして混合 (高調波の多い矩形波に対して線形補間を行ったりはしません)
* CSM モード (効果音モードとの違いが分からない) と SSGEG とハードウェア LFO 未対応
* PPZ8 は線形補間のみ(オリジナルの無補完よりは…)
* libopna, fmdriver 部分は freestanding な c99 (のはず) なのでマイコンからFM音源の制御にも使えるかもしれない(但し曲データを全部読み込むバッファが必要なので SRAM が 64KB くらいは必要)

## 今後の予定:
* まともなUIを作る
* MDRV2, PLAY5などの対応
* 他人に見せられるくらいにはコードを綺麗にする

## (まだ使えるような状況じゃないけど) 使い方
### GTK 版の仮 UI
gtk3, portaudio を使用します。
```
$ cd gtk
$ autoreconf -i
$ ./configure
$ make
$ ./fmplayer
```
`$HOME/.local/share/fmplayer/ym2608_adpcm_rom.bin`からMAME互換のドラムサンプルを読み込みます。
現在のところタイトル表示は font.rom を `$HOME/.local/share/fmplayer/font.rom` に置かなければ表示されません。(2バイト半角文字、 Ambiguous Width など色々な問題があるのでわざわざ自力でフォントを読む構造にしてあります、そのうち font.rom がなくてもとりあえず表示できるようにはします)

### WIN32 版の仮 UI
MinGW でコンパイルします。MSVCでもコンパイルできると思います。
```
$ cd win32/x86
$ make
```
exe の置いてあるディレクトリの中の`ym2608_adpcm_rom.bin`からMAME互換のドラムサンプルを読み込みます。
DirectSound, WinMM の順に使用します。Windows 2000 までの API しか使用していないので、理論的には PC-98 実機でも動くはずですが、libopna 部の処理が最適化されてなく非常に重いため手持ちの PC-9821V12 (Pentium 120MHz) では半分くらいの速度でしか再生されませんでした。
