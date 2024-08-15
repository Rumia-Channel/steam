# krkrsteam: Steam 用プラグイン
## AUTHOR
合資会社ワムソフト 様  
渡邊剛 様

## MODIFIRE
Go Watanabe 様  
みあみい 様

## EDITOR
Rumia Channel

## WHAT IS THIS
krkrsteam は 吉里吉里のゲームをSteamに対応させるためのプラグインです.

このプロジェクトは, batファイルをクリックするだけで簡単にビルドすることが出来るを目標としています.

本リポジトリでは, [Go Watanabe 様](https://github.com/wtnbgo) と [みあみい 様](https://github.com/miahmie) による改良を取り込ませていただいています.

参考にしたページは[WORKS CITED](#works-cited)にて記述させていただいております.
## HOW TO BUILD
1. [STEAMWORKS](https://partner.steamgames.com/)から, Steamworks SDK のバージョン**1.45～1.49**[※1](#footnote1)をダウンロードし, 解凍したsdkフォルダのアドレスを, [システム環境変数 STEAMWORKS_SDK を新規に作成して設定する](https://support.borndigital.co.jp/hc/ja/articles/115010667707-%E7%92%B0%E5%A2%83%E5%A4%89%E6%95%B0%E3%81%AE%E8%BF%BD%E5%8A%A0%E3%81%99%E3%82%8B%E6%96%B9%E6%B3%95%E3%81%AB%E3%81%A4%E3%81%84%E3%81%A6-Windows10).
2. [VisualStudio](https://visualstudio.microsoft.com/ja/vs/)をダウンロード(無料で利用できるCommunityを推奨)し,  C++ によるデスクトップ開発 をインストール.
3. [Premake5](https://premake.github.io/download)をダウンロードし, 解凍したフォルダのアドレスを, [システム環境変数 PATH に追加する](https://atmarkit.itmedia.co.jp/ait/articles/1805/11/news035.html).
4. このリポジトリを[ダウンロード](https://github.com/Rumia-Channel/steam/archive/refs/heads/master.zip)して展開し, Build_Release.cmd をダブルクリック.



<a name="footnote1">※1</a> : 1.49までしか試していないが, 1.59と1.60 ではライブラリの更新により正常に動作しなくなっていた. 具体的には **k_iClientVideoCallbacks** が消えてしまったらしい.  
    そのほかも所々エラーが出たりするので, 対応するまでは 1.45～1.49.  
    [元となったリポジトリ]()では1.45を想定しているので, **推奨は1.45**.

## HOW TO USE
Release フォルダから krkrsteam.dll と steam_api.dll を利用したいプロジェクトの plugin フォルダにコピー.

first.ks に以下の文を追加.
```javascript
[eval exp="Steam.requestInitialize()"]
```
Initialize.tjs に以下の文を追加.
```javascript
/* startup.tjs とか Initialize.tjs とか return 一発で実行から即抜けられる場所に配置する */
var APPID = 0; // SteamのAPPIDの変数（別に直接埋め込みでも問題はないです）
Plugins.link("krkrsteam.dll");
if (Steam.RestartAppIfNecessary(APPID)) return System.terminate(1); // Steamクライアント経由で起動させる
```
次のようなマクロを作成すれば, Steam の実績解除を便利に利用できる.
```javascript
// 実績解除
[macro name=実績][eval exp="Steam.setAchievement(mp.name)"][endmacro]
```
そのほかの使い方に関しては, [manual.tjs](krkrsteam/steam/manual.tjs) を参考にした方が早い.

## CAUTION
このライブラリでは Go Watanabe 様 や みあみい 様 の変更を **勝手に取り入れさせてもらっている** だけなので, 彼らに動作不良のことを積極的に質問すべきではない.  
また, 今後 STEAMWORKS_SDK の新しいバージョンに対応させるための変更を行い, 動作不良を起こすことがあるかもしれないが, その際も御二方に問い合わせて迷惑をかけない事. 私による変更で問題が起きた場合は, 私に問い合わせること.  
要するに, **人の与り知らないところで想定していない人物に利用されているものに対する問い合わせで迷惑をかけるな**, ということ.  
オープンソースな以上, 自己責任でこれらのソフトウェアを扱うこと.

## LICENSE
[吉里吉里2 本体のライセンス](license.txt)に準拠.  
STEAMWORKS SDK は STEAMが定める規約に準拠して利用してください.

## WORKS CITED
* [Steam.RestartAppIfNecessary(APPID) を追加してみた, GitHub Gist](https://gist.github.com/miahmie/eb855fe738e76eaea683507742c9f58e)
* [実績解除の際に実績名でオープンする, X (旧Twitter)](https://x.com/miahmie/status/1182663036391383040)
* [Steamプラグインの初期化漏れ対策, X (旧Twitter)](https://x.com/miahmie/status/1182704749327446016)
* [吉里吉里のゲームをSTEAMに対応する方法 プラグイン編, holizonglow](https://horizonglow2.blogspot.com/2019/01/steam.html)