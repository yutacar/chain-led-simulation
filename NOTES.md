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
