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

## Phase 3: 残りコマ自動生成（完了 2026-07-09）

### AnimationGenerator / ProceduralCompleter
- `src/generator/`（`chainled::generator` 名前空間、`chainled_animation_core` のみに
  依存する純粋ロジック）: `AnimationGenerator` 抽象インターフェース
  （`generate(Animation&, target_frame_count, hint)`）+ `ProceduralCompleter` 実装
- アルゴリズム: シード（フレーム0）のバウンディングボックスを求め、8×8グリッド端で
  反転（バウンス）しながら平行移動を繰り返してフレーム1以降を生成する決定的処理。
  `generate()` は常にフレーム0をシードとして**全体を再構築**する（既存のフレーム1以降は
  破棄）ため、再実行しても同じシードなら同じ結果になる（冪等）
- 10テスト green（frame数境界・シード保持・決定性・空白シード・角付近での
  クラッシュなし・再生成での縮小等）
- Editor画面に `G` キーで割り当て（"Generate 10 frames from this one"）。
  `BaseViewModel::generate_remaining_frames()` が `kMaxFrames`（10枚）まで生成
- 任意テキストヒントの入力UIは**未実装のまま**（`hint` 引数は `ProceduralCompleter`
  では無視されるため、LLM実装まで意味を持たない。UI実装はLlmCompleter着手時に
  合わせて追加する）

### Go/No-Go 実測（ローカルLLM: 見送り）

**結論: 今回は No-Go。`LlmCompleter` は未実装のまま、インターフェース
（`AnimationGenerator`）のみ将来の拡張点として用意。**

実測手順・データ:
- Battleship の `models/SmolLM2-360M-Instruct-Q4_K_M.gguf`（258MB、既にダウンロード
  済みのものを流用）と `brew install llama.cpp`（CLI/`llama-bench`）で実測
- 本アプリの想定生成方式（パレット数値64桁への文法制約＝GBNF、
  `root ::= [0-9]{64}` 相当）で実際に1回生成を実行し成功を確認
  （`llama-cli --grammar-file` で64桁の数字列を正しく生成、構文的に妥当）
- `llama-bench -ngl 0`（GPU不使用、CPU実行、Mac Apple M2）: **104.67 t/s
  （2スレッド）/ 153.04 t/s（4スレッド）**（tg64、3反復平均）
- Battleship の既存実測（NOTES.md §9.3、同モデル、自由文48トークン生成）:
  910〜1440ms（2スレッド、Apple Silicon Mac）。今回のグラマー制約64トークン生成は
  それより高速（Mac上で64トークン ÷ 104.67 t/s ≈ 0.6秒/フレーム）

**判断根拠（速度よりRAMが決定的）**:
1. **モデルファイルだけで258MB**。CardputerZero の総RAM 512MBの**半分以上**を
   重みだけで占有する計算になり、OS・LVGL・アプリ本体・アニメーションバッファの
   余地を圧迫する。これは生成速度がどれだけ改善しても解決しない構造的な問題であり、
   実機無しでも十分に確度の高い判断材料となる
2. 速度面: Mac実測 0.6秒/フレーム × 最大9フレーム ≈ 5.4秒（Mac上）。
   CardputerZero（Cortex-A53 @1GHz、NEON はあるが Accelerate/BLAS 級の
   最適化なし）への外挿は Battleship の既存見積り「数倍〜10倍程度遅くなりうる」を
   踏襲すると 1フレームあたり数秒〜10秒弱、**アニメーション1本の生成に
   数十秒〜1分規模**になる可能性が高い。ワンショット処理として許容範囲内の
   可能性もあるが、上記(1)のRAM制約単体で不採用と判断するのに十分
3. Battleship の前例（非英語出力の10/10失敗という機能的な確定的失敗）ほど
   明確な失敗はないが、RAM論拠は実機無しでも十分に確信度が高いため、
   「実測して判断する」というPLAN.mdのコミットメントを満たしつつ、
   `LlmCompleter` に人手・実装コストをこれ以上投じない判断とした
   （Battleship が「実装してから結局無効化」したのと同じ轍を避ける）
- **再検討の条件**: (a) 実機が届き512MB環境で実測できる、または (b) 数十MB級の
  さらに小さいモデル（例: 1桁〜数十MBの超小型モデル）が実用品質で見つかった場合。
  `AnimationGenerator` インターフェースはそのまま再利用できる設計にしてある
- 使用した `llama.cpp`（brew）はローカル環境に残置（次回のGo/No-Go再検証に
  再利用可能。削除が必要ならユーザー判断で）

## Phase 4: Chainプロトコル + デバイス出力（完了 2026-07-09）

### プロトコル実装
- `src/chain/`（`chainled::chain` 名前空間、`chainled_animation_core` のみに依存する
  純粋ロジック部分と、POSIX termios を使う `RealChainOutput` の2部構成）
  - `protocol.h/.cpp`: パケットのエンコード（`encode_packet`）+ ストリーミング
    デコーダ（`PacketDecoder`）。M5Chain 公式ライブラリの `processBufferData()` と
    同じ「ヘッダ探索→長さ検証→CRC/フッタ検証→1バイトずつリシンク」方式を実装
  - `chain_commands.h`: オペコード定数（`SET_DISPLAY_BUFFER=0x31` 等）・
    デバイスタイプコード（Mono=0x000D, RGB=0x000E）
  - `frame_codec.h/.cpp`: `Frame` → Mono用8バイト（1バイト/行、ビット=列。
    ビット順はM5Stack公式docsに明記が無いため独自の選択—実機で鏡像なら要調整と
    コメントで明記）/ RGB用128バイト（64px × RGB565リトルエンディアン）変換
  - `chain_output.h`: `IChainOutput` 抽象インターフェース
  - `real_chain_output.h/.cpp`: `IChainOutput` の実装。ポートパスは
    `CHAINLED_UART_DEVICE` 環境変数のみから取得（ハードコード無し）。
    オープン失敗時は例外を投げず `is_connected()==false` のまま安全に動作継続
- **17テスト追加、全67テスト green**（プロトコルのラウンドトリップ・複数パケット・
  分割feed・ノイズ混入からのリシンク・CRC/フッタ破損検出・Frame⇔バッファ変換・
  RealChainOutputの「未接続時は安全にno-op」の4系統）

### Simulator画面への統合
- `R` キーで実機出力 ON/OFF をトグル（PLAN.md §2.5「実機出力 ON/OFF」）。
  `RealChainOutput` は初回トグル時に遅延生成（画面に入るだけでは実I/Oを起こさない
  設計）。ステータスを `R:Real[OFF|ON|no device]` として画面下部に表示

### Linux（CM0実機）クロスビルド検証（ユーザー指示で今回実施）
- Battleship の実績手順（NOTES.md §14.4）をそのまま踏襲: BSPアーカイブ
  （`M5CardputerZero-UserDemo` releases v0.0.4、435MB、認証不要）を
  `.cache/bsp_sysroot/` に展開し、`debian:trixie`（**bookworm不可** —
  BSPのglibc 2.41とヘッダ不整合）のネイティブarm64コンテナに読み取り専用マウント。
  pip版cmake 4.3.4（trixieのapt版は要求バージョンに満たない）でconfigure/build
- **見つけて直した実バグ**: `chainled_viewmodel`（`app::config_dir()`/
  `default_animation_save_path()` を呼ぶ）と `chainled_core`（`ScreenManager` が
  `BaseViewModel` を必要とし `chainled_viewmodel` にリンク）の間で循環参照が発生
  — `paths.cpp` を `src/app/` に置きつつ `chainled_core` のソースとしてビルドして
  いたのが原因。**macOSの ld64 は許容するが GNU ld（Linuxクロスビルド）は
  1パス左→右の解決のため未解決シンボルのままリンク失敗**。
  `CMakeLists.txt` で `paths.cpp` を `APP_SOURCES`（core向け）から除外し
  `VIEWMODEL_SOURCES` に追加することで解消（ヘッダの置き場所・include パスは
  変更なし）。**デスクトップビルドでは顕在化しない類のバグで、実際にクロス
  ビルドを実施しなければ発見できなかった**
- 修正後、`chainled` 実行体が **aarch64 ELF PIE**（`for GNU/Linux 3.7.0`）として
  正常にリンク完了。`file` コマンドで確認。実機接続・動作確認は Phase 9
- BSP展開済みディレクトリ（`.cache/bsp_sysroot/`、約1.3GB、gitignore対象）は
  Phase 8/9で再利用可能なため保持。アーカイブ本体（435MB）は削除済み。
  Dockerコンテナも作業完了後に削除済み

## 次フェーズへの引き継ぎ
- Phase 5（配布プロトコル + TCP開発経路）から着手。PLAN.md §2.3 参照
- 現状カラーピッカーはプリセット6色循環のみ（フルRGBピッカーは意図的に
  Phase 2スコープ外。PLAN.md記載の設計判断）
- `LlmCompleter` は未実装（Phase 3のGo/No-Go参照）。任意テキストヒント入力UIも
  同様に未実装のまま持ち越し
- Chain Mono/RGBとの実機疎通・Monoのビット順確認は Phase 9（実機到着後）
