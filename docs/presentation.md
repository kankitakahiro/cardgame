# 演出(HUDのアニメーション・視覚フィードバック)

対戦画面(`UCGGameHUD`、実装の全体構成は[architecture.md](architecture.md)「カードUI
の設計」参照)で、プレイヤーに「今何が起きたか」を視覚的に伝えるための演出を
まとめる。ルールそのものは[game-rules-minimum.md](game-rules-minimum.md)、
キーワードの一覧は[keywords.md](keywords.md)を参照。

## 実装方式(共通)

- `UWidgetAnimation`/Blueprintタイムラインは使わず、`NativeTick()`で経過時間を
  貯めて毎フレーム値を補間する手動アニメーション方式に統一している
  (`ShowFloatingNumber()`が最初の実装で、以後の演出もすべて同じ形)。
- 「新しい出来事が起きた」ことは`ACGGameState`側のシーケンス番号
  (`AttackSequenceNumber`/`CardPlaySequenceNumber`)や勝者インデックスの変化で
  検知し、HUD側は直近に再生した値を覚えておいて1回だけ再生する
  (`RefreshUI()`のたびに同じ演出を再生し直さないため)。
- 盤面のカードは`RefreshUI()`のたびに`ClearChildren()`で作り直されるため、
  対象ウィジェットの座標(`GetCachedGeometry()`)がまだ有効でない(直前に
  作られたばかりでPaintを経ていない)ことがある。この場合は演出関数が`false`を
  返し、`NativeTick()`側が次のフレームで再試行する(最大10回)。

## 攻撃演出

`UCGGameHUD::PlayAttackAnimation()`。`ACGGameState::AttackSequenceNumber`の
変化を検知して再生する。

- **突進(Lunge)**: 攻撃したユニットが対象の方向へ一瞬動いて戻る
  (`UCGCardSlotWidget::PlayLungeTowards()`)。距離はそのまま使うと画面端で
  動きすぎるため最大80pxに丸める。
- **被弾フラッシュ**: 攻撃を受けたユニット(反撃を受けた側も含む)が白→赤→白と
  一瞬点滅する(`UCGCardSlotWidget::PlayHitFlash()`)。単純なフェードだと一瞬
  すぎて気づきにくいという反応を踏まえ、赤を一定時間保持してから戻す形にした。
- **ダメージ数値ポップアップ**: 対象の位置に赤字で"-N"が上へ浮かびながら
  フェードする(`UCGCardHostWidget::ShowFloatingNumber()`)。顔面(リーダー)への
  ダメージ/回復は、ライフオーブのHP差分検知(`LastSeenSelfHP`/`LastSeenEnemyHP`)
  側で表示するため、ここでは対象がユニットのときだけ表示する(二重表示防止)。
  回復は緑字で"+N"。

## カードプレイ演出

`UCGGameHUD::PlayCardPlayAnimation()`。`ACGGameState::CardPlaySequenceNumber`/
`LastCardPlayResult`の変化を検知して再生する(`ACGGameMode::RequestPlayCard()`が
プレイ成功のたびに更新)。「カードがプレイされたときの演出が無い」という
フィードバックへの対応。

- **Unit**: 着地した盤面のカードを金色に点滅させる(`PlayHitFlash()`を被弾用の
  赤ではなく金色`(1.0, 0.82, 0.15)`で流用)。点滅だけだと、直後の
  StateVersion追い更新(下記「注意点」)で盤面が作り直されて演出が数フレームで
  打ち切られ、見えないことがあったため、盤面ウィジェットに依存しないカード名の
  浮遊テキスト(下記)も必ず合わせて出す。
- **Spell**: 場に残らずすぐ捨て札へ行くため、代わりにプレイヤーの顔
  (ライフオーブ)の位置にカード名の浮遊テキストを表示する。
- 浮遊テキストは`UCGCardHostWidget::ShowFloatingText()`(`ShowFloatingNumber()`と
  同じ土台を文字列表示用に一般化したもの)。色はUnit/Spellとも金色で統一。

### 注意点: StateVersionの追い更新との競合

オンライン対戦のレプリケーション同期のため、`ACGGameState::StateVersion`の
変化を検知すると`RefreshUI()`を2回(即時1回+約0.15秒後に追い1回)呼ぶ。
盤面ウィジェットに直接アニメーション状態を持たせる演出(被弾フラッシュ・
カードプレイの点滅)は、この追い更新でウィジェットごと作り直されると演出が
途中で消えてしまう。浮遊テキスト系(`ShowFloatingNumber`/`ShowFloatingText`)は
最前面の専用レイヤー(`PreviewLayer`)に独立して乗るため、この影響を受けない。
**盤面が短時間に何度も作り直される可能性がある演出は、点滅だけに頼らず浮遊
テキストも併用する**のがこの実装での定石。

## 勝敗演出

`UCGGameHUD::PlayResultAnimation()`/`TickResultAnimation()`。
`ACGGameState::WinnerPlayerIndex`の変化(`LastAnimatedWinner`と比較)を検知して
1回だけ再生する。「勝利、敗北したときにもっと派手に演出を入れてほしい」という
フィードバックへの対応。

- 画面全体を最初の0.4秒でゆっくり暗く覆う(`ResultOverlayLayer`、最終アルファ0.72)。
- 中央に「勝利!」(金色)/「敗北...」(暗赤色)のバナーが、0→1.25→1.0と
  オーバーシュートしながら0.6秒かけて弾むように現れる(`ResultBannerText`)。
  導入後も常時ゆっくり明滅させ、演出が一瞬で終わった印象にならないようにする。
- 通信切断による不戦勝/不戦敗はこの演出の対象外。
- **画面のどこをクリックしてもロビーに戻れる**(`ResultClickCatcher`、全画面を
  覆う透明な`UButton`で`ResultOverlayLayer`ごと包んでいる)。以前は
  `ResultOverlayLayer`を`HitTestInvisible`にして下の小さな「ロビーへ戻る」
  ボタンへクリックを素通りさせる方式だったが、暗転演出の下で小さなボタンを
  探させるのは分かりにくく、「演出が出た後操作できなくなる」という体感に
  つながっていたため変更した。小さな「ロビーへ戻る」ボタン自体は互換のため
  残している。

## マーケットの購入可否表示

`UCGGameHUD::PopulateMarketColumn()`。現在のコイン+マナで届かない(購入割引後の
コストが上回る)枠は、カードの`RenderOpacity`を0.4に下げて薄く表示する
(「マーケットの買えないカードを分かりやすくしてほしい」というフィードバックへの
対応)。クリック自体は禁止しないが、購入は従来どおり拒否される。

## 行動ログ

`ACGGameState::ActionLog`(直近30件、`AppendActionLog()`で追記)+
`UCGGameHUD::PopulateActionLog()`。「CPUと対戦したときに何をされたのか分からない」
というフィードバックへの対応。AIのターンは`UCGAIOpponent::RunTurn()`が購入→
プレイ→攻撃→EndTurnまで一括で同期的に処理するため、個々の行動を目で追いにくい
ことから追加した。カードのプレイ・マーケットでの購入・攻撃・フィニッシャーの
登場を記録し、画面下部の「行動ログ」ボタンでいつでも開いて見返せる。新しい
行動が一番上に来る順で表示し、カードのプレイはカード名+効果テキスト、攻撃は
カード名+対象+ダメージ量を記録する。オンライン対戦でも`ActionLog`ごと複製
されるため、双方の画面で同じログが見られる。

## ホバー拡大

カードにマウスカーソルを重ねると、画面最前面の専用レイヤーに拡大コピーが
表示される。`UCGCardHostWidget`が担う共通機構で、実装の詳細(Z順序の扱い)は
[architecture.md](architecture.md)「ホバー拡大とZ順序」を参照。裏向きカード
(相手の手札等)は情報が無いため拡大表示自体を出さない。

## コンパクト表示

バトル画面は縦スクロール無しで6段(敵情報→敵の場→マーケット→自分の場→
自分情報→自分の手札)を収める必要があるため、自分の手札以外は縮小表示している。
内部レイアウトは変えず`RenderTransform`でまるごと縮小して見せる方式で、
詳細は[architecture.md](architecture.md)「縮小表示(コンパクト表示)」を参照。
