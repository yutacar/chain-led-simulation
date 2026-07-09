# Chain LED Simulation — 開発ノート

## Phase 0: 環境構築（完了 2026-07-08）

- [CardputerZero/template](https://github.com/CardputerZero/template) を取り込み、
  private repo [yutacar/chain-led-simulation](https://github.com/yutacar/chain-led-simulation) として新規 git 履歴で開始
- `cmake --preset darwin-arm64` → `cmake --build --preset darwin-arm64-dbg` でビルド成功
- **既知の落とし穴を早速踏んだ**: Homebrew fmt 12.2+ で `fmt/core.h` だけでは `fmt::format` が見えない
  （Battleship プロジェクトの既知の落とし穴と同一）。`src/logger/logger.h` の include を
  `fmt/format.h` に変更して解消
- テンプレートアプリがデスクトップ 320×170 で起動することを確認（stdout ログで LVGL 初期化・
  フォントロード・ボタン生成を確認。目視でのスクリーンショット確認は Phase 2 の
  APP_SCRIPT/screenshot 機構整備後に行う）
- Chain Mono / Chain RGB の UART デイジーチェーン・プロトコルを
  [m5stack/M5Chain](https://github.com/m5stack/M5Chain) 公式 Arduino ライブラリのソースから調査。
  詳細は PLAN.md §1.2 に記録。PDF版プロトコル文書は非公開のため、ソースコードが一次情報源

## 次フェーズへの引き継ぎ
- Phase 1（AnimationCore + テスト基盤）から着手
- GoogleTest/CTest はテンプレートに未導入のため FetchContent で新設が必要

## Rev.2 再計画（2026-07-09）

ユーザー指示: 「最初の1枚を書くと残りのコマをローカルLLMで作成する機能」を追加。
PLAN.md を Rev.2 に改訂（新設 Phase 3、以降 Chain/配布/i18n/BLE/パッケージング/実機のフェーズ番号を+1）。

- 設計判断: `AnimationGenerator` インターフェースの下に `ProceduralCompleter`（既定・
  依存ゼロ・平行移動/反転/回転/色相回転等の決定的変換）と `LlmCompleter`（任意・
  llama.cpp + GBNF 文法でパレット数値64桁に出力を制約）を用意
- Battleship で同一ハードウェア上の常駐LLM（対戦中リアルタイム解説）がRAM/速度面で
  不採用になった前例があるため、Phase 3 冒頭で Go/No-Go 実測を行う方針とした
  （詳細は PLAN.md §2.4・§4 リスク表）。本アプリのLLM利用はワンショット・オフライン
  生成でリアルタイム性が不要な点がBattleshipと異なるが、それでも実測を先にする
- ユーザー確認済み: 残りコマ生成時に任意のテキストヒント入力を追加する（空欄なら
  1枚目の絵柄のみから継続生成）

## Phase 1: アニメーションコア（完了 2026-07-09）

- CMake の `project()` をテンプレート既定の `template_app` から `chainled` に改名
  （Battleship が最初期に `battleship` へ改名したのと同じ判断。バイナリ/ライブラリ名が
  `${PROJECT_NAME}_xxx` 展開で自動的に揃う。`template_app` の直書き参照は
  `CMakeLists.txt` の `project()` 宣言1箇所のみだったため低リスクと判断）
- `src/core/`（`chainled::core` 名前空間、LVGL/プラットフォーム非依存）:
  - `Frame`: 8×8 RGB565グリッド。pixel get/set（範囲外は安全に無視）、clear/fill、
    flip_horizontal/vertical、shift（はみ出た部分は破棄、ラップしない）
  - `Animation`: `Frame` 列（1〜10、既定は1枚のブランクフレームから開始）+
    フレームごとの表示時間（既定200ms、50〜5000msにクランプ）。
    add_blank_frame/duplicate_frame/remove_frame/move_frame は境界（最大10・最小1）を守り
    違反時は false を返す（例外を投げない）
  - `animation_serializer`: 独自テキスト形式（JSONライブラリ非導入 — Battleshipも
    JSONライブラリなしで自前プロトコルを書いた前例に倣った判断）。ローカル保存用だが
    将来のBLE配布ペイロード（PLAN.md §2.3）としてもそのまま使う想定のため、
    壊れた入力は例外を投げず必ず `std::nullopt` を返す設計を徹底
- テスト基盤: `tests/CMakeLists.txt` で GoogleTest v1.15.2 を FetchContent 導入
  （Battleshipと同一バージョン・同一パターン）。`CHAINLED_BUILD_TESTS`
  オプション（既定 `USE_DESKTOP` に連動）でデバイスクロスビルドには含めない
- **37/37 テスト green**（`ctest -C Debug`、`build/darwin-arm64` にて確認）。
  境界値（0枚除去禁止・11枚目追加禁止・不正マジック/フレーム数/16進数/フレーム順序の
  拒否・ファイル未存在時のnullopt等）を含む
- フルアプリ（`chainled` 実行体）のビルドも改名後に再確認、問題なし

## Phase 2: エディタ+シミュレータUI + APP_SCRIPT/screenshot機構（完了 2026-07-09）

- テンプレートのデモ画面・chrome（Apple/Butterスクリーン、TitleBar/NavBar、
  IconButton）を全削除し、Battleship と同じ「フルスクリーン BaseScreen +
  共有 lv_group + Esc短押し=戻る/長押し(Home可)=終了」方式に置き換え
  （`base_screen.h/.cpp`, `screen_manager.h/.cpp`, `platform/linux_input.*` を
  Battleship の実装から移植・ChainLED 用に整理。ナビボタン方式は撤去）
- `BaseModel`/`BaseViewModel` を作り直し、`AppPage{Title, Editor, Simulator}` の
  3画面構成に。`BaseViewModel` が `chainled::core::Animation` を直接保持し、
  カーソル位置・現在フレーム・描画色（固定プリセット6色、`draw_palette.h`）を
  管理。編集操作はすべて `animation_dirty_subject_`（bump カウンタ）を発火し、
  各画面は1つの観測者で全体再描画する方式（Battleship の `board_dirty_subject`
  と同一パターン）
- **Title画面**: New Animation / Load Animation（保存済みアニメーションがある
  場合のみ表示）/ Quit。矢印キーでボタン間フォーカス移動
- **Editor画面**: 8×8ピクセルグリッド（12pxセル、矢印+Enterで色トグル）、
  最大10コマのタイムライン（2pxセルのミニプレビュー、現在コマを枠でハイライト）、
  C=色循環、F/V=水平/垂直反転、N/D/Backspace=追加/複製/削除、+/-=表示時間調整、
  S=保存、P=シミュレータへ
- **Simulator画面**: `view::widgets::PixelGrid`（`SimulatorChainOutput` 相当、
  Editor と共有部品）でオンスクリーン再生ループ（各フレーム自身の duration_ms で
  タイマー再設定）。M でRGB/Mono表示切替（Mono時は非ゼロ画素を白に閾値変換 —
  実機 Chain Mono での見え方を模擬）
- 保存先は `~/.config/chainled/animation.txt`（`app/paths.h`。Phase 6の
  設定ファイル本実装より前の最小実装として単一ファイルのみ扱う）
- **見つけて直したバグ**: タイムラインのミニサムネイル（セル2px）で毎セル1pxの
  境界線を引いていたため、境界色が塗り色を圧倒しサムネイル全体が白っぽく
  見える不具合。`refresh_pixel_grid()` に `draw_cell_borders` フラグを追加し
  サムネイルでは境界線を省略して解消（スクリーンショット目視で発見）
- APP_SCRIPT / APP_SCREENSHOT_SEQUENCE / F12 スクリーンショット機構を
  Battleship の実装から移植（`app/screenshot.*`, `app.cpp`）。環境変数名は
  `CHAINLED_WINDOW_ZOOM`（Battleshipの`BATTLESHIP_WINDOW_ZOOM`相当）
- `LV_USE_SNAPSHOT` を `lv_conf_desktop.h` で有効化（PNG::PNGは`lvgl`ターゲット
  経由で既にリンク済みのため追加のCMake変更は不要だった）
- **13枚のスクリーンショットで目視確認**（`docs/screenshots/`、リポジトリに
  コミット — Battleshipの前例に倣い開発記録として保持）: Title→New→ピクセル
  描画→色変更→フレーム追加→フレーム間タイムライン→Simulator(RGB/Mono)→
  Editorに戻る→保存→Titleに戻ってLoad Animationボタン出現、を一通り確認。
  Home キーでの終了（exit code 0、クラッシュなし）も確認
- 全38テスト green（Phase 1のAnimationCoreテストに変更なし。Phase 2はUI層のみで
  ヘッドレステストの対象外 — 挙動確認はAPP_SCRIPTスクリーンショットで代替）

## 次フェーズへの引き継ぎ
- Phase 3（残りコマ自動生成: ProceduralCompleter → Go/No-Go実測 → 任意でLlmCompleter）
  から着手。PLAN.md §2.4 参照
- Editorの「Generate」操作・任意テキストヒント入力UIはPhase 3で追加する
  （Phase 2のEditor画面には未実装）
- 現状カラーピッカーはプリセット6色循環のみ（フルRGBピッカーは意図的に
  Phase 2スコープ外。PLAN.md記載の設計判断）
