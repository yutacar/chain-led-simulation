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

## Phase 5: 配布プロトコル + TCP開発経路（完了 2026-07-09）

### プロトコル・トランスポート層
- `src/net/`（`chainled::net` 名前空間、`chainled_animation_core` のみに依存）を
  Battleship の `src/net/` 実装から移植・adapt:
  - `frame.h/.cpp`: `[MAGIC(0xCE,0xD1)][ver][type][seq][len][payload][CRC16]`。
    CRC-16/CCITT-FALSE、Battleshipと同じ「ヘッダ探索→CRC検証→1バイトずつ
    リシンク」方式の `FrameDecoder`
  - `transport.h`: `ITransport`（poll駆動・非ブロッキング）。Battleshipと同一形状
  - `loopback_transport.h/.cpp`: フォールト注入付き（loss/duplicate/fragment_bytes、
    方向別・シード指定可）ループバック実装。ヘッドレステスト用
  - `tcp_transport.h/.cpp`: POSIX非ブロッキングソケット（listen/connect/poll/
    send/receive）。デスクトップ/LAN開発経路（実機デフォルトはPhase 7のBLE）
  - `animation_transfer.h/.cpp`: `AnimationSender`/`AnimationReceiver`。
    PLAN.md §2.3のメッセージ種（Announce/Request/Chunk/ChunkAck/Complete/Error）を
    それぞれ独立してストップ&ウェイト再送するチャンク転送プロトコル。
    Complete/Errorがロストしても、送信側がWaitingComplete中に最終チャンクを
    再送し続ける（受信側はチャンク重複受信時にComplete/Errorを再送する）ことで
    暗黙のping/pongとして機能する設計
- 26テスト追加（frame codec、fault injection、実TCPソケット、
  ロス/重複/分断ソークテスト5シード、実TCPソケット越しの完走を含む）、
  **全93テスト green**

### Send/Receive UI + 実2プロセス検証（見つけて直した実バグ2件）
- `AppPage::Send/Receive` を追加。Title画面に "Send Animation (LAN)" /
  "Receive Animation (LAN)" ボタン。開発用固定ポート `kDevTransferPort=35455`、
  Receiveは常に `127.0.0.1` に接続（LANホスト探索・手動IP入力はスコープ外、
  Phase 7のBLEで別経路になる想定）
- Battleship流の「実2プロセスをMac上で起動して検証する」を実施
  （APP_SCRIPTで2つの `chainled` 実プロセスを起動、Process A=Send、
  Process B=Receive）。**この過程で実バグを2件発見・修正**:
  1. `SendScreen::refresh()` がトランスポートの接続状態を転送結果より優先して
     表示していたため、転送成功後に受信側が切断すると「Waiting for
     connection...」に逆戻りして見えるバグ。判定順序を「まずsender status、
     InProgressかつ未接続の時だけwaiting表示」に修正
  2. **より深刻な実バグ**: `AnimationSender::poll()` が `status_` が
     Done/Failed に達した後も state machine を回し続けていたため、
     WaitingComplete のタイムアウト再送ループが完了後も永久に走り続け、
     十分待つと `max_retransmits` を超えて **成功していたはずの Done が
     Failed に巻き戻る**という潜在バグがあった。`poll()` 冒頭に
     `status_ != InProgress` なら即 return するガードを追加して解消。
     ユニットテスト（ロス注入・ラウンドトリップ等）ではどれも「完了したら
     即座にpollをやめる」書き方だったため検出できず、**実プロセスを長時間
     動かして初めて顕在化した**。回帰テスト
     `SenderStatusStaysDoneAfterReceiverDisconnects` を追加
  3. **バグではなく検証手順の教訓**: 上記2点の修正を確認する過程で
     「直しても直らない」ように見える期間があったが、原因はコードではなく
     検証スクリプトのタイミング想定が甘すぎたこと。このセッションで
     Docker/BSPビルド等を長時間行った後でシステム負荷が高く、2プロセス目
     （受信側）が接続を試みるまでの実時間が**十数秒**かかることがあった
     （`lv_tick_get()` ベースの内部ログで実測: 接続確立まで15秒以上）。
     固定ディレイでのワンショットscreenshotではなく、十分長いマージン
     （実測では25秒以上）を取るか、複数回の中間スクリーンショットで状態遷移を
     追う方が信頼できる。将来同様の検証をする際はこの点に注意
- 最終確認: Process A（Send画面）に "Done! Sent 2/2 chunks"、Process B が
  Editorに正しく反映された2ピクセルパターンを表示することを実スクリーン
  ショットで確認（`docs/screenshots/proc_a_done.png`,
  `docs/screenshots/proc_b_editor.png`）

## Phase 6: i18n（EN/JA/ZH） + 設定永続化（完了 2026-07-09）

### 文字列カタログ + フォント基盤
- `chainled_i18n`（headless: strings.cpp/strings_en/ja/zh.cpp/settings.cpp）と
  `chainled_i18n_fonts`（LVGL + AssetManager依存: fonts.cpp）に分割。
  Battleshipの `battleship_i18n`/`battleship_i18n_fonts` 分割と同じ理由
  ——ユニットテストバイナリがLVGLを引きずり込まずに文字列カタログだけ
  リンクできるようにするため
- `StrId` enum + `tr()`（プレーン文字列）/`trf()`（printf風テンプレート、
  可変長引数）の2関数構成。JA/ZHは語順がENと異なるため、呼び出し側が
  英語順で組み立てた断片を後から結合する設計を避け、「これはNスロットの
  テンプレートだ」とカタログ側で明示する方針（Battleshipと同じ設計判断）
- `SettingsLanguageEnglish`/`Japanese`/`Chinese` の3エントリだけは
  全言語テーブルで同じテキスト（"English"/"日本語"/"简体中文"）——
  現在のUI言語に関わらず、Settings画面の各言語ボタンは常にその言語自身の
  スクリプトで表示される（読めない言語のUIから自分の母語を探せるように）
- `i18n::font_for_size(size)`（現在言語用）と
  `i18n::font_for_language_and_size(lang, size)`（言語明示、Settings画面の
  言語ボタン用）の2関数。英語は一切FreeTypeに触れず常にビットマップの
  Montserratを使う設計に変更——既存画面が使っていた `assets().load_font(
  "inter-*.ttf", size)` パターンはこのフェーズで全廃し、英語表示もMontserrat
  に統一（デザイン上の後退ではなく、CJKフォールバックチェーンを
  `cjk_font->fallback = latin_font` で組む都合上、全言語で同じフォールバック
  先を使う方が実装がシンプルになるため）

### CJK フォントサブセット
- `tools/generate_cjk_fonts.py`（Battleshipの同名スクリプトを移植・簡略化。
  ChainLEDには複数言語混在のcommentaryファイルが無いため
  `SHARED_TEXT_SOURCES` 相当の処理は不要）。`strings_ja.cpp`/`strings_zh.cpp`
  内の全`"..."`リテラルからCJK文字集合を収集し、Google Noto Sans JP/SC の
  upstream SubsetOTFを`fonttools.subset`でさらに絞り込む
- 実行には `pip install fonttools brotli` が必要（リポジトリには非同梱）。
  一時venvで実行し、生成後は削除。結果: `NotoSansJP-Subset.otf` 32.0KB
  （207文字）、`NotoSansSC-Subset.otf` 32.3KB（192文字）——
  Battleshipの実測（本文中「50KB未満」の見積もり）と同オーダー
- 生成物 (`assets/fonts/NotoSansJP-Subset.otf`, `NotoSansSC-Subset.otf`) は
  リポジトリにコミットする実ファイル。`.cache/`（ダウンロードキャッシュ）は
  既存の`.gitignore`で除外済み

### ScreenManager: 言語切り替え時の画面再構築
- `BaseViewModel::language_version_subject()`（dirty counterパターン、既存の
  `animation_dirty_subject()`と同じ設計）を新設。`set_language()`は
  `i18n::save_language()`で即座に適用+永続化した**後**にsubjectをbumpする
  ——ScreenManagerのreloadが走った時点で新言語が既に反映済みであることを
  保証するため
- `ScreenManager::reload_current_screen()`: 現在ロード中のページを同じ
  `AppPage`で`show_screen_for_page()`し直す（`BaseScreen`を新しいインスタンスで
  作り直す）。`request_page()`はページ値が変わらないとno-opなので、
  言語変更では使えない——これが専用メソッドを新設した理由
- `ScreenManager::start()`で`language_version_subject()`にも観測者を追加
  （`page_observer_`と同様、デストラクタでの解除も対称に追加）

### Settingsスクリーン + Title統合
- `SettingsScreen`新設: EN/JA/ZHの3言語ボタン + Back。現在の言語には
  `palette.primary`色の2pxボーダーを付けて選択中であることを明示
  （Editorのタイムラインで現在フレームを強調する既存パターンを流用）
- 各言語ボタンのラベルは`font_for_language_and_size(lang, 12)`で描画——
  UIが英語の状態でも「日本語」「简体中文」ボタンはちゃんとそのスクリプトで
  読める（`font_for_size(12)`だとEN文脈ではMontserratになりCJKがtofuになる
  ため、ここだけ明示的に言語を指定する必要がある）
- 選択は即座に反映（`set_language()`→`language_version_subject`
  bump→`reload_current_screen()`が同期的に走るため、ボタンを押した次の
  フレームで画面全体が新言語で再描画される。実機画面でも確認済み——
  `docs/screenshots/persist_select_ja.png`相当、確認後に削除）
- `model::AppPage::Settings`追加、Titleに"Settings"ボタン追加、
  `handle_short_esc()`にSettings→Titleのケース追加
- Titleスクリーンはボタンが最大6個（New/Load/Send/Receive/Settings/Quit）に
  増えたため、ボタン高さを20→18px、行間を3→2pxに詰めて320x170に収める
  よう調整（実スクリーンショットで確認、オーバーフローなし）

### 全画面の文言 i18n化
- Title/Editor/Simulator/Send/Receive の全ハードコード文字列を`tr()`/`trf()`
  呼び出しに置換。`snprintf`で組み立てていたテンプレート文字列
  （`"Editor   Frame %d/%d"`等）は`trf()`（内部で2パス`vsnprintf`）に統一
- `app.cpp`: `i18n::load_settings()`をUI構築前（`lv_init()`直後）に、
  `i18n::init_fonts(assets)`を`AssetManager`構築直後に追加

### 実機検証（3言語 × 5画面のスクリーンショット + 実プロセス永続化確認）
- APP_SCRIPTで EN/JA/ZH それぞれ Title/Editor/Simulator/Send/Settings
  （+ EN のみ Receive）をスクリーンショット確認。tofu・レイアウト崩れなし
  （`docs/screenshots/{title,editor,simulator,send,settings}_{en,ja,zh}.png`,
  `receive_en.png`）
- **検証手順の教訓（Phase 5と同種）**: 最初300msディレイでesc連打の
  スクリプトを組んだところ、2回目のescが「効かない」ように見えるバグに
  見えた（Simulator→Editorで止まり、Editor→Titleまで進まない）。
  デバッグの結果、SDLキーボードドライバのpress/release疑似生成
  （`lv_sdl_keyboard_read`のdummy_readパターン）自体は正しく動作しており、
  単に300ms×連続2回のescではスクリプト側のマージン不足だっただけと判明
  （600msに広げたら即座に正しく動作）。**コードのバグではなくテスト
  タイミングの問題**——Phase 5の教訓がここでも再現した形
- UI経由（コード手動操作ではなく実際にSettings画面でJAボタンをEnter）で
  言語選択→`settings.conf`に`language=ja`が書き込まれることを確認、
  同じ`XDG_CONFIG_HOME`で再起動して初期画面が自動的に日本語になることを
  確認（`load_settings()`のユニットテスト`SaveThenLoadRoundTripsLanguage`と
  同じ経路を実プロセスで再確認した形）
- 全107テストgreen（Phase5までの93 + i18nカタログ7 + 設定永続化7）

## Phase 7: BLE 配布（完了 2026-07-09）

### Battleship の実装調査 → 移植方針
- 着手前にサブエージェントで Battleship の `src/net/ble/ble_transport.{h,cpp}`
  （sd-bus/BlueZ D-Bus、NUS互換GATT、912行）を詳細調査。結論:
  **アーキテクチャはそのまま移植可能**（`ITransport`実装、Host=peripheral/
  Join=central の役割分担、NUS標準UUID採用、`BleScanner`による探索）だが、
  **ペイロードサイズの前提が違う**——Battleshipは対戦メッセージが最大13バイトで
  「MTU未交渉のBLE既定20バイトに必ず収まる」ため無分割設計（`kBleSingleWriteLimit
  =20`の`static_assert`）。ChainLEDのアニメーションChunkフレームは
  `kChunkDataSize=200`（animation_transfer.h）により実測最大約210バイト/フレーム
  ——単純にBattleshipのコードをコピーするだけでは送れない
- **フラグメンテーション設計判断**: `kChunkDataSize`を縮小して1フレームを
  20バイト以下に収める案は却下——`Announce`メッセージの`total_chunks`が
  uint8_t(最大255)のため、10フレーム全画素点灯時の実測シリアライズサイズ
  2720バイト（実測、`animation_to_text()`で確認）を20バイトチャンクに割ると
  272チャンクとなり**既存の255上限を超えて壊れる**。MTU交渉に賭ける案
  （BlueZは通常大きめのATT_MTUを自動交渉するが、実機で検証不能なため賭けは
  避けたい）も却下。**採用**: `BleTransport::send()`内部で
  `kBleFragmentSize=18`バイトごとに分割し、複数回のGATT
  Write/Notifyを発行する方式。フラグメント境界にヘッダは一切付けない
  ——ATT配送は単一コネクション上で順序保証されており、`FrameDecoder`は
  TCPのストリーム分割と同様バイト列の切れ目に一切依存しないため
  （magic+CRC16での自己同期）、これで正しく動作する。個々のフラグメントが
  ロスしても外側のChunk/ChunkAckタイムアウト再送層（Phase 5でロス注入
  ソークテスト済み）がフレーム単位で再送するため、BLE層自体には
  Indicate/確認応答などの追加信頼性機構は不要と判断（シンプルさ優先）

### 実装
- `src/net/ble/ble_transport.{h,cpp}`: Battleshipの実装を
  `chainled::net`名前空間・`CHAINLED_`命名規約・`/com/chainled/gatt`
  オブジェクトパスに置き換えて移植。NUS UUID（`6e400001..003-...`）は
  標準規格のためそのまま流用。広告名プレフィックスは`"CHAINLED-"`
  （v1はニックネーム設定UIがないため、固定サフィックス`"cardputerzero"`を
  付与——複数台が同名で広告しても後述の「最初に見つかったホストに自動
  join」方針では問題にならない）
- `BleTransport`: `start_host()`(peripheral) / `start_join(device_path)`
  (central)。`send()`は`kBleFragmentSize`ごとに分割し、host役は
  `sd_bus_emit_properties_changed`によるNotify、join役は
  `WriteValue`（`type: command`、write-without-response）を繰り返す
- `BleScanner`: NUSサービスUUID or 名前プレフィックスでフィルタした
  LE discovery。1秒間隔で`GetManagedObjects`を再列挙
- CMake: `CHAINLED_BLE`オプション新設（Battleshipの`BATTLESHIP_BLE`と
  同一方針——`CMAKE_SYSTEM_NAME STREQUAL "Linux"`でON既定、それ以外は
  OFF既定・強制ONはFATAL_ERROR）。`src/net/ble/`を`chainled_net`の
  グロブから除外し`if(CHAINLED_BLE)`内で明示的に`target_sources`
  追加、デスクトップ(macOS)ビルドではsd-bus/BlueZヘッダに一切触れない
  ことをビルドで確認済み。sd-bus依存はCM0クロスビルド時はBSP
  sysroot内の`libsystemd-dev`相当（`systemd/sd-bus.h`, `libsystemd.so`）を
  `cm0_find_header`/`cm0_find_library`で解決——**Phase 4で展開した
  `.cache/bsp_sysroot/`に両方とも既に含まれていたため追加取得不要**
  （`find /path -iname "*sd-bus*"`で事前確認）
- Send/Receive画面: `ITransport`型の共通ポインタに一般化し
  （従来は`TcpTransport`の具象型だった）、`#if CHAINLED_BLE`で
  `BleTransport`/TCP経路を切替。TitleScreenのボタン文言も
  `#if CHAINLED_BLE`で"(LAN)"/"(BLE)"を切替（新規StrId4件追加、
  EN/JA/ZH全対応）
- ReceiveScreen（BLE時）は2段階状態機械: `scanner`が非nullの間は
  `BleScanner::poll()`で結果リストを監視し、**最初に見つかったホストへ
  自動join**（デバイス選択UIは v1 スコープ外——既存のTCP経路も
  127.0.0.1固定・手動IP入力なしという同方針の踏襲）。join成功後は
  `scanner`を破棄し`transport`/`receiver`を構築、以降は通常の
  `AnimationReceiver::poll()`ループに合流

### 検証
- **ヘッドレステスト**: 既存107テストは無変更のまま全green
  （`ble_transport.cpp`はデスクトップビルドのグロブから除外されるため
  コンパイル自体されない——BlueZ/sd-bus依存のD-Bus往復はそもそも
  ユニットテスト不可能な領域であり、Battleship同様この部分にBLE専用の
  単体テストは無い。上位のChunk/ChunkAck信頼性層は既にLoopbackTransport
  のフォルト注入で厚くテスト済み、というのが「テストの手が届く境界」の
  設計）
- **デバイスビルド**: Phase 4で確立したDocker手順（`debian:trixie`
  ネイティブarm64コンテナ + BSP sysroot）を再利用、`apt-get install`に
  `libdbus-1-dev libsystemd-dev`を追加（Battleshipの再現手順をそのまま
  流用）。`-DCHAINLED_BLE=ON -DUSE_DESKTOP=OFF`でconfigure→buildが
  639/639ターゲット完走、`ble_transport.cpp`も問題なくコンパイル。
  最終バイナリを`file`/`readelf -d`で確認: `ELF 64-bit ... ARM aarch64
  ... dynamically linked`、`NEEDED`に`libsystemd.so.0`を含む
  ——BLE機能が実際にリンクされていることを確認
- **実機2台での実配布は未実施**（Phase 9で実施予定、PLAN.md記載の通り）。
  Battleship自身も同じ理由（実機がない）で同じ制約を持ったまま
  Phase 6を完了させており、今回も同じ前提で進めた。BlueZの実際の
  ATT_MTU交渉結果・ペアリング無しでの接続可否・`bluetoothd`の
  D-Busポリシー（`bluetooth`グループ要件、Battleship NOTES.md参照）は
  実機到着後に要確認

## Title画面ロゴ: ASCII-artワードマーク（完了 2026-07-09、ユーザー指示）

- ユーザー指示: 「アプリタイトルは翻訳せずに英語で統一し、Battleshipのときと
  同じようにアスキーアートで表現してください」
- Battleshipの`tools/generate_title_logo.py`（"BATTLESHIP"を5x7ドットマトリクス
  フォント+等角投影ベベルで描画するPillowスクリプト）をそのまま移植。
  ChainLED版は`TEXT="CHAINLED"`（プロジェクト名・BLE広告名プレフィックス
  ("CHAINLED-")と統一、"Chain LED Simulation"というフル表示名ではなく
  ブランドの単語そのものを採用——Battleshipも"BATTLESHIP"というブランド語のみを
  ロゴにしており、説明的なサブタイトルは含めない方針と揃えた）
- 5x7フォント表はBattleshipのA/E/H/I/Lをそのまま流用し、CHAINLEDに必要な
  C・N・Dの3文字を同じクラシック5x7スタイルで追加
- 配色もBattleshipのvariant A（面色 `0x63e2b7`）をそのまま採用——これは
  たまたまではなく、ChainLED自身の`view::color::dark::kPrimary`が同じ
  `0x63e2b7`（テンプレート由来の既定アクセント色を両プロジェクトとも
  変更しなかったため）なので、「ロゴがアプリ自身のプライマリカラーに
  合わせてある」というBattleshipの元々の設計意図がそのまま成立する
- **レイアウト調整が必要だった**: Battleshipのロゴ高さ予算(MAX_H=48)を
  そのまま使うと、Phase 6で追加したSettings/BLEボタンにより最大6個に
  増えたTitle画面のボタンスタック(高さ118px)と実測で重なった
  （スクリーンショットで確認）。MAX_H=26に縮小することで
  320×170画面・6ボタン最大構成でも重なりなく収まることを確認
- `title_screen.cpp`: `lv_image_create()` + `assets().resolve("images/
  title_logo.png")`でPNGを表示、パス解決に失敗した場合は`lv_label`で
  "CHAINLED"のプレーンテキストにフォールバック（Battleshipと同一パターン）
- `i18n::StrId::AppTitle`は完全に不要になったため削除（enum・EN/JA/ZH
  3テーブル・該当テスト`test_i18n_catalog.cpp`のアサーションを全て更新）。
  ロゴは全UI言語で常に同じ英語ワードマークとして表示される
- 3言語×Title画面のスクリーンショットで再確認、`docs/screenshots/
  title_{en,ja,zh}.png`を更新。全107テストgreen

## Title画面: サブタイトル追加 + Send/Receiveボタン統合（完了 2026-07-09、ユーザー指示）

- ユーザー指示: 「Simulationの文字をロゴの下に小さく入れてほしい」
  「ボタンがギュウギュウなので、新規作成、送受信、設定、終了にし、
  送受信のなかで受信か送信を選ぶようにしてほしいです。文言は今のままで。」
- **サブタイトル**: ロゴ直下に固定英語リテラル"Simulation"を小さめの
  フォント・70%不透明度で追加（`lv_obj_align_to(subtitle, title,
  LV_ALIGN_OUT_BOTTOM_MID, 0, 1)`でロゴ画像/フォールバックラベルどちらに
  対しても相対配置）。ロゴ本体と同じ「翻訳しない」方針——i18nカタログには
  追加せず、直接リテラルとして埋め込み
- **Send/Receiveボタン統合**: Title画面に個別にあった"Send Animation"/
  "Receive Animation"の2ボタンを1つの"Send / Receive"（JA: 送受信、
  ZH: 收发）ボタンに統合。押すと新設の`SendReceiveScreen`
  （`AppPage::SendReceive`、`send_receive_screen.{h,cpp}`）に遷移し、
  そこで初めてSend/Receiveを選ぶ。**選択後の2ボタンの文言はTitleに
  あった時とバイト単位で同一**（`TitleSendAnimation(Ble)`/
  `TitleReceiveAnimation(Ble)`をそのまま再利用、新規文言は作っていない
  ——ユーザー指示「文言は今のままで」を字義通り満たす設計）
  - 新規i18n文字列は2つだけ: `TitleSendReceive`（Titleの統合ボタン自体の
    ラベル、ユーザーが直接指定した日本語「送受信」をJAテーブルに採用）と
    `SendReceiveGuide`（新画面下部のガイド文言、Settings画面と同一文言だが
    画面ごとに個別StrIdを持たせる既存の設計方針を踏襲）。`SendReceiveTitle`
    は`TitleSendReceive`と同じ文言のため独立StrIdにせず共有はせず重複させた
    （画面見出しとメニューボタンは意味的に別物という既存パターンに合わせた）
  - `ScreenManager`のEscルーティングを変更: 旧「Send/Receive画面から
    Esc→Title直行」を「Send/Receive画面から Esc→SendReceiveチューザー
    画面→Esc→Title」の2段階に変更（`model::AppPage::SendReceive`を
    Send/ReceiveとTitleの中間階層として追加）
  - 効果: Title画面の最大ボタン数が6個（New/Load/Send/Receive/Settings/
    Quit）→4〜5個（New/[Load]/Send・Receive/Settings/Quit）に減少。
    Phase 6で詰めていたボタン高さ・行間を元の余裕あるサイズに戻せる
    余地ができた（今回は現状維持のまま、レイアウトに十分な余白ができた
    ことをスクリーンショットで確認）
- 3言語×Title画面+新Send/Receive選択画面、および
  Title→SendReceive→Send画面→Esc→SendReceive→Esc→Titleの
  ナビゲーション往復をスクリーンショットで確認。`docs/screenshots/
  {title,send_receive}_{en,ja,zh}.png`を更新。全107テストgreen

## Title画面: Quitボタン削除 + ロゴサイズ/レイアウトの再調整（完了 2026-07-09、ユーザー指示）

- ユーザー指示: 「Quitボタンもいらないです。全体のバランスとロゴの大きさを
  調整してください」
- **Quitボタン削除**: Esc長押し/Homeキーで既に終了できる（ガイド文言に
  明記済み）ため、ボタン自体を削除。`quit_cb`も未使用になったため削除。
  `i18n::StrId::TitleQuit`も完全に不要になったため削除（enum・EN/JA/ZH
  3テーブル・参照していたテストのアサーションを`TitleSettings`に差し替え）
- **ロゴを大きく**: Send/Receive統合でボタンが最大4個（New/[Load]/
  Send・Receive/Settings、Quit削除で更に減）に減ったことで生まれた
  余白を使い、`MAX_H`を26→36→最終30に調整（36は次の一段落で説明する
  理由で大きすぎたため縮小）
- **レイアウトの実バグ2件を発見・修正**（いずれもスクリーンショットで発覚、
  コードを読むだけでは気づけなかった）:
  1. ロゴ＋サブタイトルの直下にメニューを画面中央基準の固定オフセットで
     配置していたところ、ロゴを大きくしたことでサブタイトルとメニュー最初の
     ボタンが重なった。**固定オフセットでの配置は壊れやすい**——
     `lv_obj_align_to(menu, subtitle, LV_ALIGN_OUT_BOTTOM_MID, 0, 4)`で
     サブタイトルの実際の描画位置を基準にした相対配置に変更し、
     フォントメトリクスに関わらず重ならないことを構造的に保証
  2. 上記修正後、今度は逆に「Load Animation」ありの4ボタン最大構成で
     メニュー下部がボトム固定の`TitleGuide`ガイド文言と重なった
     （ロゴを大きくした分、ボタン欄の高さ予算が圧迫されていた）。
     ボタン高さ20→18px、行間5→3px、ロゴMAX_H 36→30に縮小することで
     解消。EN/JA/ZH全言語・3〜4ボタン両方のケースをスクリーンショットで
     確認（JA/ZHのCJKフォントは行高が高く、最も重なりやすい組み合わせ
     として重点確認した）
- 教訓: 320×170という限られた縦解像度で「ロゴ・サブタイトル・可変個数の
  ボタン・固定位置のガイド」を共存させるレイアウトは、要素数や言語による
  フォントメトリクスの違いで簡単に破綻する。**固定オフセットではなく
  実要素基準の相対配置（align_to）を使う**、**最悪ケース（最大ボタン数×
  最も行高の高い言語）で必ずスクリーンショット確認する**、の2点を
  以後のUI変更でも徹底する
- `docs/screenshots/title_{en,ja,zh}.png`を4ボタン最大構成で再撮影・更新。
  全107テストgreen

## Title画面: ロゴ+ボタンのセンタリング + RGBダイオード配色（完了 2026-07-09、ユーザー指示）

- ユーザー指示: 「ボタンとロゴをセンタリングしてください。またロゴのLED部分は
  赤緑青のダイオードカラーにして、順番は任せます。」
- **センタリングの根本修正**: 直前の反復では「ロゴ→サブタイトル→メニュー」を
  それぞれ`lv_obj_align`/`lv_obj_align_to`で個別に画面座標基準・相対座標基準
  混在で配置しており、要素ごとの実サイズが変わるたびに重なりバグを2回
  出していた（前セクション参照）。今回は**ロゴ・サブタイトル・ボタン列を
  1つの入れ子flexコンテナ`group`にまとめ、`group`自体を`content`に対して
  `LV_ALIGN_CENTER`で配置**する構成に書き換えた。個々の子要素はflexの
  自動配置に任せる（`lv_obj_align`呼び出しを全廃）ため、ボタン数や
  フォントメトリクスが変わってもグループ内部で自動的に詰まり直し、
  グループ全体が画面中央を基準に一体としてセンタリングされる
  ——場当たり的なオフセット調整が今後も要らなくなる構造的な解決
  （下部のガイド文言だけは`group`の外に残し、画面下端に固定）
- **LEDのRGBダイオード配色**: `tools/generate_title_logo.py`を単色
  （`0x63e2b7`固定）から、各セルの絶対列位置を3で割った余りで
  赤→緑→青を循環させる配色に変更（`_cell_colors()`、単語全体を左から
  右へR,G,B,R,G,B...と流れるように）。ChainLEDが実際に駆動する
  Chain RGBのLEDマトリクスをそのままロゴ自身で体現する狙い
  （単一アクセントカラーより意図が明確なデザイン判断）。順番はユーザーから
  一任されたため、列インデックスでの循環という規則的だが単調にならない
  方式を採用。アウトライン（縁取り）は色相を混在させると散らかって見えるため
  全セル共通の単一の濃色のまま統一
- 3〜4ボタン×EN/JA/ZHの組み合わせをスクリーンショットで再確認、重なりが
  無いことを確認。`docs/screenshots/title_{en,ja,zh}.png`を更新
  （4ボタン最大構成、新配色で再撮影）。全107テストgreen

## Title/SendReceive/Editor 微調整3件（完了 2026-07-09、ユーザー指示）

- ユーザー指示: 「LEDの文字だけカラフルにし、CHAINはもともとの単色に
  してください」「設定画面だけ戻るがあるので送受信にも追加してください」
  「アニメーション編集画面で選択セルの枠線を黒で表現していますが、
  グレイにしてください。そうすればどのカラーになってもわかりやすく
  なるかと思います。」
- **ロゴ配色の絞り込み**: 前回は単語全体8文字を列位置で赤緑青循環させて
  いたが、今回「CHAIN」は単色（`0x63e2b7`、元のアクセントカラーに戻す）、
  「LED」の3文字だけ1文字＝1色（L=赤、E=緑、D=青）に変更。
  `tools/generate_title_logo.py`の`_cell_colors()`を列位置ベースから
  **文字インデックスベース**に変更（`letter_index < 5`→CHAIN色、
  それ以降→LED各色）。"LED"という単語そのものが3色ダイオードを暗示する
  ため、単語全体を虹色にするより意図が明確になる
- **SendReceive画面にBackボタン追加**: Settings画面には既にあった
  「戻る」ボタンがSendReceive画面には無く非対称だったため追加。
  新規StrId`SendReceiveBack`（EN "Back" / JA "戻る" / ZH "返回"）。
  Esc操作でも同じくTitleへ戻れるため機能的には冗長だが、Settings画面との
  一貫性を優先（ボタンでの明示的な操作手段も用意する、という既存UIの
  慣習に合わせた）
- **エディタのカーソル枠線色を黒→グレイに変更**: `pixel_grid.cpp`の
  `refresh_pixel_grid()`が選択セルの枠線色に`palette.text`
  （ライトテーマでは黒に近い色）を使っていたため、黒に近い色を描画した
  セルの上ではカーソル位置が視認しにくかった。テーマ非依存の固定グレー
  `kCursorBorderColor = 0x808080`に変更——ユーザー指摘の通り、どの描画色の
  上でもカーソルが判別しやすくなる。EditorとSimulatorで共有している
  `refresh_pixel_grid()`だが、Simulator側は`cursor_x/y`に`-1,-1`を渡すため
  この変更の影響を受けるのはEditor画面のみ
- EN/JA/ZH全言語でTitle・SendReceive画面のスクリーンショットを再撮影し
  `docs/screenshots/{title,send_receive}_{en,ja,zh}.png`を更新。
  全107テストgreen
- 追記（同日）: ユーザーから「もう少し白より」の追加指示を受け、
  `kCursorBorderColor`を`0x808080`→`0xC8C8C8`（より明るいグレー）に
  再調整。スクリーンショットで視認性向上を確認、全107テストgreen

## 保存アニメーション削除機能の追加（完了 2026-07-09、ユーザー指示）

- ユーザー指摘: 「アニメーションを削除するにはどうしたら良いですか？その機能が
  ないようなので保存したアニメーションを削除する機能を追加してください」
  ——保存(`S`キー)・読込(Load Animationボタン)はあったが削除する手段が
  存在しなかった実際の機能ギャップ
- `BaseViewModel::delete_saved_animation()`を追加
  （`std::filesystem::remove(app::default_animation_save_path(), ec)`。
  読込中のアニメーション本体には触れず、保存ファイルのみ削除）
- Title画面に"Delete Animation"ボタンを追加（`has_saved_animation()`が
  真の時のみ、Load Animationのすぐ下に表示——Loadと対になる操作として
  同じ条件で出し入れする設計）
- **確認ダイアログ無しの即時削除**を採用: Editorの Backspace（現在フレーム
  削除）や S（保存）も既に確認無しの即時実行であり、本アプリのキーボード
  操作主体・省スペースUIの一貫した設計方針に合わせた。ボタン文言
  "Delete Animation"自体で意図を明確にすることで代替
- **実装上の工夫**: 削除後にLoad/Deleteボタンを画面遷移なしで即座に隠す
  必要があったが、既存の`ScreenManager::reload_current_screen()`は
  言語切替用の`language_version_subject`監視でのみ駆動されており、
  「現在のページに再度show_page()」は同一ページのため`lv_subject`が
  変更なしと判定して発火しない（no-op）。新たに`TitleUi`という小さな
  ヒープ構造体（Load/Deleteボタンへの生ポインタを保持、
  `LV_EVENT_DELETE`でクリーンアップ——Editor/Send画面で既に使っている
  パターンを踏襲）を導入し、削除コールバック内で該当ボタンを
  `lv_group_remove_obj()`+`LV_OBJ_FLAG_HIDDEN`により直接非表示化する
  方式で解決（flexレイアウトはhidden要素を自動的に詰めてくれる）
- **レイアウトの再調整が必要だった**: Load+Delete追加で最大ボタン数が
  4→5個（New/Load/Delete/Send・Receive/Settings）に増加、既存の
  センタリング設定のままだと最悪ケース（5ボタン×JA/ZHのCJK行高）で
  再度ガイド文言と重なった。ボタン高さ18→16px・メニュー内行間3→2pxに
  縮小した上で、**グループの垂直オフセットを`has_saved_animation()`の
  真偽で条件分岐**（保存済みアニメーションあり: -18、無し: -6）——
  固定値1つで全ボタン数に対応しようとすると「3ボタン時は不自然に
  上寄り」か「5ボタン時に重なる」のどちらかに倒れてしまうため、
  ボタン数で変わる実際の高さに応じてオフセット自体を出し分ける方式に
  変更。3ボタン・5ボタン・EN/JA/ZH全組み合わせをスクリーンショットで
  確認し重なりが無いことを確認
- 削除→ボタン即時非表示→再度New Animationした場合にLoad/Deleteボタンが
  出ないこと（＝実際にファイルが消えている）を実プロセスで確認。
  `docs/screenshots/title_{en,ja,zh}.png`を5ボタン最大構成で更新。
  全107テストgreen

## 削除機能をTitleボタンからEditor画面のキー操作へ移設（完了 2026-07-09、ユーザー指示）

- ユーザー指摘: 「ボタンが増え、画面がくずれたのでアニメーション削除ボタン
  ではなく、アニメーション画面で削除するようにしてください。」
  ——直前のTitle画面"Delete Animation"ボタン追加が、レイアウト崩れ
  （5ボタン最大構成でのバランス調整の煩雑さ）を引き起こしたための巻き戻し
  ＋再配置指示
- **Title画面を完全に4ボタン構成へ復元**: `TitleUi`構造体・
  `title_ui_delete_cb`・`delete_animation_cb`・条件分岐オフセット
  （`has_saved_animation() ? -18 : -6`）を全て削除し、直前コミット
  （`850b8a3`）時点の`title_screen.cpp`を`git show`でそのまま復元。
  `i18n::StrId::TitleDeleteAnimation`も削除
- **Editor画面にXキーで削除を移設**: `BaseViewModel::delete_saved_animation()`
  自体は再利用（Titleに実装した時点のものをそのまま流用）。Editor画面の
  既存ショートカット体系（矢印/Enter/Bksp/C/F/V/N/D/G/+-/S/P、","/"."）に
  `X`を追加——Bksp（現在のフレームを削除）とは別物であることを明示する
  ため、コード上のコメントで区別を明記
- **確認なしの即時削除を維持**（Bksp・Sと同じ設計方針、Title版からの
  変更なし）。保存済みファイルが無い場合はサイレントに何もしない
- **ステータス表示**: 既存の`show_saved`（"Saved"表示）と対になる
  `show_deleted`フラグを追加、押した直後だけ"Deleted"を表示
  （Sと同じUXパターン）
- **ガイド行の実装で一度失敗し、その場で修正**: 当初は新しいガイド行を
  1行追加する形で実装したが、スクリーンショット確認で「+/-:Duration
  S:Save P:Play G:Generate...」の4行構成パネル（高さ固定96px、既に
  Phase 2から"最もタイトなレイアウト"として知られていた場所）に5行目を
  追加すると、パネル自身は高さで視覚的にクリップされないため、下の
  タイムラインサムネイル行（y=118）と衝突する危険域まで食い込んだ。
  **新しい行を追加するのではなく、既存の「+/-:Duration S:Save P:Play」
  行に"X:Del"を追記し「+/-:Dur S:Save P:Play X:Del」と短縮混みで
  1行に収める**方式に変更して解決（新規`EditorGuideDeleteSaved`
  StrIdは撤回、`EditorGuideDurationSave`の文言を更新するだけに変更）。
  この経緯も含め「新しい行を安易に追加する前に、まず既存行への統合を
  検討する」という教訓として記録
- EN/JA/ZH全言語でEditor画面（4行ガイド、保存→削除の実操作）・
  Title画面（4ボタン復元）をスクリーンショットで確認。
  `docs/screenshots/{editor,title}_{en,ja,zh}.png`を更新。全107テストgreen

## Phase 8: パッケージング（完了 2026-07-09）

ユーザー指示: 「フェーズ８，９も実施し、ドキュメントも更新してください」。
Battleship（同じCardputerZeroテンプレート・同じ提出プロセスの先行プロジェクト）
がこの段階を実際に完走していたため、着手前に専用サブエージェントで
Battleshipの`cmake/cm0-package.cmake`・`app-builder.json`・ラッパー
スクリプト・`.desktop`テンプレート・アイコン生成・`prepublish_check.py`
実行結果・クリーンルーム検証結果を詳細調査してから移植した。

### バージョン
- `project(chainled VERSION 0.0.1 → 1.0.0)`。Battleship自身も
  パッケージング段階で`1.0.0`を採用しており（実際に公開されたバージョン、
  本セッション冒頭のgit logで確認済み: "Battleship v1.0.0 for
  CardputerZero"）、同じ節目での同じ判断として踏襲

### `cmake/cm0-package.cmake` 全面書き換え
テンプレート初回インポート（`8c5328b`）以来一度も変更されていなかった
ことをgit logで確認した上で、Battleshipの実装をそのまま移植・アプリ名
差し替え:
- `APP_DISPLAY_NAME "ChainLED"`、package名（小文字）`chainled`
- `APP_MAINTAINER "yutacar <yutacar@yutacar.net>"`
  ——**GitHub確認済みメールアドレスと一致必須**（czdev/prepublish_checkが
  email_mismatchで拒否する）。gmailアドレスではなくこちらを使うことは
  以前のセッションでユーザーから明示された既知の注意点
  （[[user-github-email]]メモリ参照）
- **systemdサービスを完全撤去**（APPLaunchは常駐サービスを想定せず、
  審査上のリスク項目でもあるため）。`cmake/templates/app.service.in`を
  削除
- **ラッパースクリプト方式を新規導入**: `cmake/templates/app-wrapper.sh.in`
  を新設、`.desktop`の`Exec=`はバイナリ直接ではなくラッパー
  （`bin/chainled-launch`）を指す（提出ガイドの「引数・作業ディレクトリ・
  環境変数設定はExec行ではなくラッパースクリプトに書く」という要件通り）
- インストール先をAPPLaunch準拠のレイアウトに変更:
  `usr/share/APPLaunch/{applications,bin,share/images}/` （ランチャー向け）
  + `usr/share/chainled/{fonts,images}/`（アプリ自身の`AssetManager`が
  参照する絶対パス、既存のまま）
- 依存関係死蔵の削除: `assets/audio/click.wav`（`src/`を全文grepして
  参照ゼロと確認済み）と旧テンプレートアイコン
  （`template.png`/`template_80.png`/`template_100.png`）を削除
- `CPACK_DEBIAN_PACKAGE_DEPENDS`を実測値に更新（後述）

### 新規アイコン生成
`title_logo.png`（横長のワードマークバナー）とは別に、正方形の
ストア/ランチャーアイコンが必要だったため`tools/generate_app_icon.py`
を新規作成。デザイン: 8x8のハート型ピクセルパターンを、実際の
`pixel_grid.cpp`と同じ視覚言語（塗りつぶし正方形セル+小さな間隔、暗い
背景に対して明るい"点灯"色）で描画——「このアプリで作れるものそのもの」
を偽りなく体現するアイコンにする狙い。256×256、赤一色（小サイズでの
視認性を優先、ロゴのRGB多色化はここでは行わない）。
`assets/images/chainled.png`として生成、`app-builder.json`の
`store.icon`とAPPLaunchランチャーアイコンの両方に使用

### `app-builder.json` 新規作成
Battleshipと同じ構造（`package_name`/`app_name`/`version`/`description`/
`store{summary,categories,icon,screenshots,locales}`、localesは
zh-CN/en/ja）。`categories`は`["Graphics"]`（Battleshipの`["Games"]`とは
異なり、本アプリはゲームではなくドット絵/LEDアニメーション作成ツール
のため）。screenshots 4枚は既存の`docs/screenshots/`から
（editor/simulator/send_receive/settings の各`_en.png`、320×170の
クリーンなフレームバッファ画像そのまま）`store/screenshots/`にコピー
——追加の撮影は不要だった

### 実機ビルド + 依存関係の実測
Phase 4/7で確立済みのDocker手順（`debian:trixie`ネイティブarm64
コンテナ + キャッシュ済みBSP sysroot）をそのまま再利用し、今回は
`-DCMAKE_BUILD_TYPE=Release -DCHAINLED_BLE=ON`でconfigure→build→
`cpack -G DEB`まで実施。生成物: `dist/chainled_1.0.0_arm64.deb`
（784936バイト、sha256:
`19eb4b9d3ab6c4cee5742818be66af2f1fcd1ed040d42dd06c5f8f2d7f215cea`）。
`readelf -d`で実バイナリのNEEDEDを確認したところ
（`libsystemd.so.0`/`libfreetype.so.6`/`libpng16.so.16`/
`libstdc++.so.6`/`libgcc_s.so.1`/`libc.so.6`）、事前に見積もっていた
`CPACK_DEBIAN_PACKAGE_DEPENDS`の値と完全一致——fmtは静的リンク、
jpeg/zlibはリンカの`--as-needed`で自動的に外れる（Battleshipと同じ
パターン）ため追加の調整は不要だった

### `prepublish_check.py` 実行
`CardputerZero/skill`リポジトリ（`CardputerZero-AppBuilder`ではなく
こちら。Battleshipの調査結果通り）を取得し、実際に生成した`.deb`に対して
`cardputer-app-publish/scripts/prepublish_check.py --deb <deb> --app-dir .`
を実行。**結果: `Prepublish check passed.`（ERROR 0 / WARNING 0、
初回実行で一発合格——Battleshipは初回2件のWARNINGがあったのに対し、
今回はBattleshipの調査結果を先に踏まえてapp-builder.jsonに
`version`/`description`を最初から含めていたため、その分の手戻りが
無かった）**

### クリーンルーム install/remove 検証
ビルドに使ったコンテナとは別の、**新規`debian:trixie`コンテナ**で
`apt install ./chainled_1.0.0_arm64.deb`を実施——申告した`Depends`
（`libpng16-16`→実際は`libpng16-16t64`という移行用パッケージ名に
自動解決、`libbrotli1`も推移的依存として導入）が全て自動解決され
インストール成功。`dpkg -L chainled`で期待通りのファイル一覧を確認、
`ldd`で全共有ライブラリのロードを確認（未解決なし）。
`LV_LINUX_FBDEV_DEVICE`環境変数を設定した状態でラッパー経由起動を
試行——ヘッドレスコンテナのため`cannot open framebuffer device`で
失敗するのは想定通り（実機ならフレームバッファが存在するため問題ない、
Battleshipの同じ検証と同一の想定内エラー）。最後に`apt-get remove`で
APPLaunchエントリ含め完全にアンインストールされることを確認

### ドキュメント更新
README.md/README.ja.md/THIRD_PARTY_LICENSES.md（前回のドキュメント
整備パスで新規作成済み）の「まだパッケージングされていない」という
記述を、実際に`.deb`がビルド・prepublish合格・クリーンルーム検証済み
という現状に合わせて更新（後述「次フェーズへの引き継ぎ」の直前に
反映済み）

## Phase 9: 実機検証 → publish（publish完了、実機検証は未実施）

### czdev publish 完了（2026-07-10）
`czdev login`のOAuthデバイスフロー自体はユーザー本人の操作が必須のため
最初は完了できなかったが、ユーザーがログインを完了させ、続けて私が
`czdev publish`を試みたところ以下のエラーで失敗した:

```
Error: GET /user/emails status
Caused by: HTTP status client error (404 Not Found) for url (https://api.github.com/user/emails)
```

GitHub APIは`user:email`スコープ無しで`/user/emails`を叩くと403ではなく
404を返す仕様のため、ログイン時に取得したトークンのスコープ不足が濃厚
だった。2回リトライしたが解消せず、詳細な原因調査（認証情報ファイルの
中身の読み出し・czdevバイナリからのOAuthクライアント設定抽出）は
「認証情報を扱う操作」として環境の権限システム自身にブロックされたため
それ以上の追跡は行わなかった。

**この後、ユーザーがFable（別セッション/エージェント）に依頼して代わりに
`czdev publish`を実行してもらい、成功した。** 結果:
[CardputerZero/packages#62](https://github.com/CardputerZero/packages/pull/62)
（"publish: chainled 1.0.0"）がオープン。`pool/main/chainled/`に
`meta.json`・アイコン・スクリーンショット・
`chainled_1.0.0_arm64.deb.release.json`（sha256
`5cca53b98418b36ad7bf4245244d3ceb4a59652ad3d3857952773f58beae7eff`——
本リポジトリで検証済みのものと同一）が追加されている。Maintainer欄も
`yutacar <yutacar@users.noreply.github.com>`で反映されており、
下記のメール修正が正しく効いていることを確認できた。

### 事前に発見・修正した実バグ: Maintainerメールのemail_mismatchリスク
`czdev publish`を試す前に、Battleshipの`yutacar/packages`フォーク上の
実履歴（`publish/battleship-1.0.0-...`ブランチ、
[CardputerZero/packages#59](https://github.com/CardputerZero/packages/pull/59)）
を調査したところ、Battleship自身の`cmake/cm0-package.cmake`が
`APP_MAINTAINER`を`yutacar@yutacar.net`から`yutacar@users.noreply.github.com`
へローカルで（未コミットのまま）変更していたことを発見した。
`gh api user`で確認したところyutacarのGitHubアカウントには公開emailが
未設定（`email: null`）——`yutacar@yutacar.net`はczdevのMaintainerチェックが
実際に照合する対象と一致しないと判断し、ChainLEDも同じ
`yutacar <yutacar@users.noreply.github.com>`に修正、Docker
（`debian:trixie`, `CHAINLED_BLE=ON`）で`.deb`を再ビルド、
`prepublish_check.py`再パス・クリーンルームinstall/remove再検証（両方とも
問題なし、依存関係リストも`readelf -d`と完全一致で変化なし）を経てから
publishに臨んだ。この修正が無ければ同じemail_mismatchで却下されていた
可能性が高い。（[[user-github-email]]メモリを訂正済み）

### このセッションで確認できたこと（ヘッドレス範囲、publish以前）
- `czdev doctor`実行——**"All required dependencies present."**
  （C compiler/cmake/pkg-config/SDL2/SDL2_image/SDL2_mixer/freetype/git
  全てOK）

### ユーザー側でのPhase 9残作業チェックリスト
`czdev login`/`czdev publish`はPR #62として完了済み。残るのは実機検証のみ
（実機到着後、以下を順に実施）:

- [ ] CardputerZero実機に`dist/chainled_1.0.0_arm64.deb`を転送し
      `sudo apt install --no-install-recommends ./chainled_1.0.0_arm64.deb`
      でインストール
- [ ] APPLaunchから起動できること、アイコン・タイトルが正しく表示される
      ことを確認
- [ ] 320×170の実LCD上でレイアウト崩れが無いこと（本セッションでは
      デスクトップSDLシミュレータのみで確認、実LCD特有のガンマ/色味の
      違いは未検証）
- [ ] EN/JA/ZH全言語でCJK文字が豆腐化しないことを実機で確認
      （デスクトップでは確認済みだが、実機のFreeTypeレンダリングパスは
      別経路のため再確認推奨）
- [ ] Esc短押し/長押し・Homeキーの規約が実キーボードで意図通り動作する
      こと
- [ ] Chain Mono / Chain RGB実機をGroveポートに接続し、Simulator画面の
      `R`キーで実際にLEDマトリクスへ出力されること（`CHAINLED_UART_DEVICE`
      環境変数が必要な場合はラッパースクリプト経由で設定）。Monoユニットの
      ビット順が想定通りか実機で確認
- [ ] CardputerZero実機2台間でBLEアニメーション配布が実際に成立すること
      （送信→受信→エディタへの反映）。Phase 7で未検証だった実際の
      ATT_MTU交渉結果・ペアリング無しでの接続可否を確認
- [ ] アンインストール後（`apt remove chainled`）に実機上へ痕跡が
      残らないこと
- [x] `czdev login`（GitHub OAuthログイン、ユーザー操作）—完了
- [x] `czdev publish --deb dist/chainled_1.0.0_arm64.deb`—完了、
      [CardputerZero/packages#62](https://github.com/CardputerZero/packages/pull/62)
- [ ] PR #62の「Device / OS tested」欄に実機検証結果を追記してからマージ

- 現状カラーピッカーはプリセット6色循環のみ（フルRGBピッカーは意図的に
  Phase 2スコープ外。PLAN.md記載の設計判断）
- `LlmCompleter` は未実装（Phase 3のGo/No-Go参照）。任意テキストヒント入力UIも
  同様に未実装のまま持ち越し
- BLEのデバイス選択UI（現状は最初に見つかったホストへ自動join）・
  ニックネーム設定・ペアリング/ボンディングは意図的に v1 スコープ外
  （Phase 7設計判断）
- ReceiveScreenの初回connect()/join()は失敗時に自動リトライしない
  （TCP経路: 送信側が先にlisten()していない状態で受信側がconnect()すると
  繋がらないままになる。BLE経路: `start_join()`失敗時はスキャンを継続する
  よう対応済みだが、`scan_failed`（アダプタ無し等）からの自動リカバリは無い）

## 次フェーズへの引き継ぎ
- `czdev login`/`czdev publish`は完了し、
  [CardputerZero/packages#62](https://github.com/CardputerZero/packages/pull/62)
  がオープン中。残るのはPhase 9の実機検証のみ——ユーザー本人が
  ハードウェア到着後に実施し、結果をPR #62に追記してマージ（上記
  チェックリスト参照）。それ以外の開発作業は全フェーズ完了
- 新しいCJK文字（新規StrId追加時など）を`strings_ja.cpp`/`strings_zh.cpp`に
  加えたら`tools/generate_cjk_fonts.py`の再実行を忘れずに（`pip install
  fonttools brotli`が必要、CIでの自動チェックは無し）

## Phase 10: ストア再提出（2026-11-20）— 審査プロセスのリセットへの対応

- **経緯**: PR CardputerZero/packages#62 は、2026-10-12 のメンテナによる全アプリ削除
  （packages commit `e107f92b` "Delete all current apps to restart the submit and
  review process"）で無効になった。新プロセスで **packages#530（chainled 1.0.0-1）**
  として再提出し、CI の validate / capture は pass
- **新プロセスの要点**（Battleship リポジトリ NOTES.md §20 に詳細）: czdev は
  `CardputerZero/AppBuilder`（`~/work/AppBuilder`）、新規アプリの初回提出は人手審査で
  **PR コメントへの実機デモ動画（起動 → 主要機能 → 終了）が必須**。app-builder.json の
  `store` に `permissions`（7項目）/`author`/`license`/`share_code`/`source_repo` が必須に
- **今回の変更**:
  - カテゴリ `Graphics` が新しい列挙に無いため `["Creative & Office", "Hardware & IoT"]` に変更
  - permissions: `network` のみ true（実機ビルドは BLE のみ。TCP 経路は
    `CHAINLED_BLE=OFF` のデスクトップ開発ビルドにしか入らない）。Chain Mono/RGB は任意
    接続なので `additional_hardware` は false とし、説明文と PR に任意である旨を明記
  - 3言語の説明文から「WiFi LAN で送受信」を削除（実機ビルドの実態は BLE のみ）
  - share_code **`CLED`**（既存 121 アプリと重複なし）、author `yutacar`、license MIT
  - `APP_DEBIAN_REVISION`（既定 1）+ `CPACK_DEBIAN_PACKAGE_RELEASE` で Version `1.0.0-1`
  - BSP は v0.0.7（tarball 直下が sysroot。`sysroot/` サブディレクトリは無い）
- **検証**: store_meta_policy.py をライブ pool と照合し ERROR 0 / WARNING 0、
  NEEDED 6 = Depends 6、クリーン trixie で install / ldd / remove OK、デスクトップ
  ctest 107/107 green
- `chainled_1.0.0-1_arm64.deb`: 786224 バイト、sha256
  `72f08921727d6f053060a4cb505e2e0690d2efbd49219f235f6230cff5a30ed7`
- **残り（ユーザー作業）**: 実機デモ動画を #530 のコメントに投稿
