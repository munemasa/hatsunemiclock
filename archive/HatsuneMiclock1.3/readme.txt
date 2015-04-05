■ 初音ミクロック(Hatsune Miclock) Version 1.4

「初音ミク」はクリプトン・フューチャー・メディア株式会社の著作物です。
(C) Crypton Future Media, Inc.

キャラクター利用のガイドライン D-3 に則って開発しています。


■ Disclaimer: 免責

このソフトウェアの不具合による損害は、何ら保証しません。


■ New Features: 1.4での新機能・変更点

・通信切断した時に、再接続を行うようにしました。


■ Minimum System Requirement: 動作環境

Windows 2000 or later.

Windows 2000で動かす場合は、GDI+を用意してください。

動作確認は、
Windows Vista(x64) SP2 + iTunes 9.1.0.79
Windows XP SP3 + iTunesなし
Windows 2000 SP4 + iTunesなし
で行っています。


■ How To Uninstall: アンインストールについて

設定項目の保存にレジストリを使用しています。

HKEY_CURRENT_USER\Software\Miku39.jp\HatsuneMiclock

完全にアンインストールしたい場合は、
レジストリエディタを使用の上、よく注意して前述のキーを削除してください。
ファイルはそのままゴミ箱へ。


■ Concept: コンセプト

F&CうぃずゆーTOYBOX(18禁)という古いバラエティゲームに含まれていた
「真奈美くろっく」というものに、設定することで毎分喋って時刻を教え
てくれる機能がありました。

この機能は、朝に布団の中で、時間がまだあるから「もうちょっと寝て
いよう…」とか、昼過ぎまで寝てしまってふと耳に入ってくる声から
「もう昼なのか…」と、わざわざ時計を見なくても現在時刻を把握でき
るというメリットだけで使い続けて早ン年。
真奈美くろっくが少し機能アップしたようなのが欲しいよなぁと思っても、
毎分喋る時計って見つからないんですよね。音声データをどうやって調達
するかという問題がありますし。

で、ふと、初音ミクに喋ってもらおうと思い立ったが吉日。
ちょうどミクゲー(ジャンル未定)を作ろうとちまちま部品開発していたこと
もあって、手頃な土台があるからさくさくっと同等品を作ってみた次第です。
ここまで出来上がれば、あとは好きなように機能追加できておいしい。
リマインダー機能を作ろう…。


■ Development Tools: 開発環境

・Microsoft Visual Studio 2008 Professional
・C/C++ with Win32 API
  ベースがゲームプログラムなので、MFC、ATL、その他フレームワークは
  一切使わず、APIだけでゴリゴリ。たまにはこういうのも楽しいよね。


■ Related Links: 関連リンク

http://miku39.jp/dists/software/miku/HatsuneMiclock.html
http://www.nicovideo.jp/watch/sm8312807 (ニコ生アラート機能取説動画)
http://www.nicovideo.jp/watch/sm8238084 (0.4取説動画)
http://www.nicovideo.jp/watch/sm8085362
http://www.crypton.co.jp/mp/pages/prod/vocaloid/cv01.jsp


■ History: 履歴

1.2 2009/10/10
    番組一覧(リストビュー)をカテゴリでグループ化できるようにしました(XP以降)。
    番組一覧(リストビュー)にエクスプローラのテーマを適用するようにしました。
    番組一覧ウィンドウにコミュ限定フラグを表示するようにしました。    
    二重通知をしないようにしました。

1.1 2009/10/3
    ブラウザ起動時に1秒のウェイトを設けました。
    コミュニティのプロパティで放送IDのコピー設定を設けました。
    コミュニティリストからコミュニティのページに移動できるようにしました。
    1.0でコマンド実行設定が保存されていなかったのを修正しました。

1.0 2009/9/24
    任意のコミュニティの生放送開始を通知できるようにしました。
    目に見えないバグ修正も少々。

0.9 2009/9/23
    放送開始時コマンド実行設定ダイアログを追加。
    レジストリに、キーワード一致で通知したコミュニティのデータを
    誤って書き込んでしまうのを修正。
    見知らぬコミュのサブキーがあったのでびっくりした…。

0.8 2009/9/22
    参加コミュニティの生放送とその他の通知履歴を個別にしました。
    通知ウィンドウにフォーカスが取られないようにしました。
    サウンド再生でメモリリークを修正した。

0.7 2009/9/21
    キーワードマッチによる通知をできるようにした。
    放送開始時にブラウザを自動起動できるようにした。
    設定画面はまだ作ってないが、レジストリを書き換えると
    放送開始時に lvXXXX の放送IDをパラメータにして任意のコマンドを
    実行できるようにした。
    RSS取得のところのメモリリークを修正した。

0.6 2009/9/18
    生放送一覧を 10分間隔 更新で表示するようにした。
    生放送一覧を正規表現でフィルタリングできるようにした。

0.5 2009/9/17
    サーバ接続時にRSS取得して放送中のものも通知するようにした。

0.4 2009/9/15
    ニコ生アラートの機能がついた。
    3分タイマーがついた。
    WAVファイルの音量を上げた。

0.3 2009/9/8
    iTunesプレイリストから楽曲リストを取得し、再生を行えるようにした。

0.2 2009/9/5
    タイトルバーとツールチップにiTunesで再生中の曲名を表示するようにした。
    ダブルクリックの機能を、iTunesの再生、ポーズに変更した。

0.1 2009/8/31
    1stリリース
    何気なく作ったらちょうどミクの誕生日じゃないですか。

-- 
Hail! Hatsune Miku!
Sat, 10 Oct 2009 02:54:13 JST
あまの <toshi [at] sheela.com>, <amano [at] miku39.jp>
Website: http://www.sheela.com/


■ License

このソフトではXMLパーサ(構文解析器)として
libxml(http://xmlsoft.org/)を使用しています。

 Copyright (C) 1998-2003 Daniel Veillard.  All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is fur-
nished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
DANIEL VEILLARD BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CON-
NECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Daniel Veillard shall not
be used in advertising or otherwise to promote the sale, use or other deal-
ings in this Software without prior written authorization from him.


-- 
正規表現ライブラリには
PCRE Perl Compatible Regular Expressions(http://www.pcre.org/)を
使用しています。

PCRE LICENCE
------------

PCRE is a library of functions to support regular expressions whose syntax
and semantics are as close as possible to those of the Perl 5 language.

Release 7 of PCRE is distributed under the terms of the "BSD" licence, as
specified below. The documentation for PCRE, supplied in the "doc"
directory, is distributed under the same terms as the software itself.

The basic library functions are written in C and are freestanding. Also
included in the distribution is a set of C++ wrapper functions.


THE BASIC LIBRARY FUNCTIONS
---------------------------

Written by:       Philip Hazel
Email local part: ph10
Email domain:     cam.ac.uk

University of Cambridge Computing Service,
Cambridge, England.

Copyright (c) 1997-2009 University of Cambridge
All rights reserved.


THE C++ WRAPPER FUNCTIONS
-------------------------

Contributed by:   Google Inc.

Copyright (c) 2007-2008, Google Inc.
All rights reserved.


THE "BSD" LICENCE
-----------------

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    * Neither the name of the University of Cambridge nor the name of Google
      Inc. nor the names of their contributors may be used to endorse or
      promote products derived from this software without specific prior
      written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

End
