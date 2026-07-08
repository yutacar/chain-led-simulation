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
