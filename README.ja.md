# 98Fmplayer (仮)
PC-98用のFM音源ドライバエミュレーション(予定)

*PMDWin のバグに悩まされているだけの人で、ビルド環境をお持ちの方には [PMDWin へのパッチ](https://github.com/takamichih/pmdwinbuild) の方がバグが少なくPCMの対応も豊富だと思われます*

## 現在の状況:
* 対応ドライバ: PMD, FMP (PLAY6含む)
* PMD: FM, SSG, リズムパート, ADPCM, PPZ8(一部)のみ対応, PPS, P86 対応は未定
* FMP: PPZ8, PDZF にほぼ対応
* UI: GTK3 で作った仮のものと、仮 Win32 版
![gtk screenshot](/img/screenshot_gtk.png?raw=true)
![gtk toneviewer screenshot](/img/screenshot_gtk.toneview.png?raw=true)
![gtk config screenshot](/img/screenshot_gtk.config.png?raw=true)
![w2k screenshot](/img/screenshotw2k.png?raw=true)
* PMD, FMP の形式を解析するついでに作ったもので、手持ちのデータが少ないため再現性は PMDWin, WinFMP の劣化版
* FM は 55467Hz で合成, SSG は 249600Hz で合成した後 sinc でフィルタして混合 (高調波の多い矩形波に対して線形補間を行ったりはしません)
* FM 合成は特定の条件下で実チップ OPNA/OPN3 の出力と 4 <= ALG の時のステレオ出力を含めて完全に一致 (エンベロープは完全でなく、AR >= 21 のときのみ一致)
* CSM モード (効果音モードとの違いが分からない) と SSGEG とハードウェア LFO 未対応
* PPZ8 は無補間、線形補間、 sinc 補間に対応
* ADPCM: 不正確 (実際の YM2608 は出力よりも低いサンプリング周波数/解像度でデコードしてから補間しているようだが、テスト回路上の YM2608 に秋月の 100 円 DRAM を繋げて動かすのに未だに成功していない。ちびおと搭載の86ボード上から取り込むか、ちびおとをもう一枚買って繋げるしかない? それにしても NMOS の IC は熱い…)
* libopna, fmdriver 部分は freestanding な c99 (のはず) なのでマイコンからFM音源の制御にも使える (但し曲データを全部読み込むバッファが必要なので SRAM が 64KB くらいは必要)

## 今後の予定:
* まともなUIを作る
* MDRV2, PLAY5などの対応
* 他人に見せられるくらいにはコードを綺麗にする

## (まだ使えるような状況じゃないけど) 使い方
### GTK 版の仮 UI
gtk3, pulseaudio/jack/alsa を使用します。
```
$ cd gtk
$ autoreconf -i
$ ./configure
$ make
$ ./98fmplayer
```
`$HOME/.local/share/98fmplayer/ym2608_adpcm_rom.bin`からMAME互換のドラムサンプルを読み込みます。

### WIN32 版の仮 UI
Releases:
https://github.com/takamichih/fmplayer/releases/
MinGW でコンパイルします。~~MSVCでもコンパイルできると思います。~~ c11 atomics を使い始めたので MSVC ではコンパイルできないと思います。
```
$ cd win32/x86
$ make
```
exe の置いてあるディレクトリの中の`ym2608_adpcm_rom.bin`からMAME互換のドラムサンプルを読み込みます。
DirectSound, WinMM の順に使用します。Windows 2000 までの API しか使用していないので、理論的には PC-98 実機でも動くはずですが、libopna 部の処理が最適化されてなく非常に重いため手持ちの PC-9821V12 (Pentium 120MHz) や PC-9821Ra300 (P6 Mendocino Celeron 300MHz) では半分くらいの速度でしか再生されませんでした。
