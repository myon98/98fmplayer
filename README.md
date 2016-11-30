# Fmplayer (仮)
PC-98用のFM音源ドライバエミュレーション(予定)

## 現在の状況:
* FMP (PLAY6含む) のみ対応
* UI がデバッグ用に ncurses で作ったデバッグ用のものと、GTK3 で作った仮のもの
![ncurses screenshot](/img/screenshot_ncurses.png?raw=true)
![gtk screenshot](/img/screenshot_gtk.png?raw=true)
* PDZF部分が不完全 (LFO, ピッチベンドなど)
* PDZF判定も未実装 (using PDZF, 4行コメントとか関係なくエンハンスドモード)
* FM は 55467Hz で合成, SSG は 249600Hz で合成した後 sinc でフィルタして混合
* PPZ8 は線形補間のみ(オリジナルの無補完よりは…)
* libopna, fmdriver 部分は freestanding な c99 (のはず)

## 今後の予定:
* まともなUIを作る
* PDZF の完全な対応
* PMD, MDRV2, PLAY5などの対応
* 他人に見せられるくらいにはコードを綺麗にする

## (まだ使えるような状況じゃないけど) 使い方
### ncurses 版のデバッグ用 UI
ncurses, SDL2 を使用します。
```
$ cd curses
$ autoreconf -i
$ ./configure
$ make
$ ./fmpc foo.ozi
```
q で終了します。
下の方にコメントを適当に iconv で変換したものを出力しているので端末が80行以上あると見えます。

PCMファイルが必要な場合、そのファイルのディレクトリから大文字、小文字の順に読み込みます。(PCMファイル名の文字コードは今のところ考慮していません)

`$HOME/.local/share/fmplayer/ym2608_adpcm_rom.bin`からMAME互換のドラムサンプルを読み込みます。

### GTK 版の仮 UI
gtk3, portaudio を使用します。
```
$ cd gtk
$ autoreconf -i
$ ./configure
$ make
$ ./fmplayer
```
ncurses 版と同じ場所からドラムサンプルを読み込みます。
現在のところタイトル表示は font.rom を `$HOME/.local/share/fmplayer/font.rom` に置かなければ表示されません。(2バイト半角文字、 Ambiguous Width など色々な問題があるのでわざわざ自力でフォントを読む構造にしてあります、そのうち font.rom がなくてもとりあえず表示できるようにはします)
