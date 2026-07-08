# Chain LED Simulation for CardputerZero — 設計・開発計画（Rev.1）

8x8 ドットアニメーション（最大10コマ）を CardputerZero 上で作成・シミュレーションし、
実機の Chain Mono / Chain RGB（M5Stack Chain シリーズ 8x8 LED マトリクスユニット）に
繋いで実際に表示できるアプリ。作成したアニメーションは BLE で配布・受信できる
（LoRa は将来対応、対象外）。

参考: [Battleship for CardputerZero](https://github.com/yutacar/Battleship)（同テンプレート・同進め方の先行プロジェクト）。
本文書は `docs/APP_DEV_INSTRUCTION_TEMPLATE.md` の指示書に基づく。

---

## 1. ターゲット環境・外部ハードウェア調査結果

### 1.1 CardputerZero 本体
Battleship プロジェクトと同一（§2 は指示書参照）。320×170 ST7789v3、46キー、
WiFi/BT4.2 内蔵、RAM 512MB、Grove HY2.0-4P / EXT 2.54-14P 拡張ポート。

### 1.2 Chain Mono / Chain RGB（今回新規調査）

| 項目 | Chain Mono (M5-U217) | Chain RGB (M5-U218) |
|---|---|---|
| 表示 | 8×8 モノクロ LED ドットマトリクス | 8×8 フルカラー LED ドットマトリクス（RGB565） |
| MCU | STM32G031G8U6 | STM32G031G8U6 |
| 通信 | UART デイジーチェーン 115200bps 8N1 | 同左 |
| コネクタ | HY2.0-4P ×2（Chain Bridge で連結） | 同左 |
| サイズ/電源 | 24×24×16mm、DC5V（動作時 max 22.29mA） | 同等 |

**UART デイジーチェーン・プロトコル**（[m5stack/M5Chain](https://github.com/m5stack/M5Chain) の公式 Arduino ライブラリのソースを解析して特定。PDF 版プロトコル文書は非公開のため実装コードが一次情報源）:

```
[0xAA][0x55][len_lo][len_hi][id][cmd][payload...][crc8][0x55][0xAA]
```
- `len` = `id`(1) + `cmd`(1) + `crc8`(1) + `payload`(nバイト) の合計（リトルエンディアン2バイト）
- `id`: チェーン内の位置（先頭から1,2,3...）。パケットは id を1減算しながら次段へ転送されるデイジーチェーン方式
- `crc8`: `id` から `payload` 末尾までの単純合計（mod 256）。CRCという名だが実体はチェックサム
- 新規接続時、デバイスは 180ms 間隔で3回の enumeration request (`CHAIN_ENUM_PLEASE` = 0xFC) を送出 → ホストが `CHAIN_ENUM`(0xFE)/`CHAIN_GET_DEVICE_TYPE`(0xFB) 等で応答・列挙
- 主要コマンド（Mono/RGB共通の値。Mono/RGBそれぞれ独自の enum だが値は同一）:
  - `SET_MODE`(0x10) / `SET_PIXEL`(0x30) / `SET_DISPLAY_BUFFER`(0x31) / `SET_CHAR`(0x34)
  - `SET_SCROLL_TEXT`(0x40) 系、`SET_ROTATION`(0xE0)、`SET_BRIGHTNESS`(0xE2)、`CLEAR`(0xE4)
  - Mono の `SET_DISPLAY_BUFFER` payload: 8バイト（1バイト/行、ビットが列に対応）
  - RGB の `SET_DISPLAY_BUFFER` payload: 64 × uint16 (RGB565)、64px 分
- 接続経路: CardputerZero の Grove(HY2.0-4P) ポートに UART で直結（追加ハード不要）。**Mac 上での実機 UART テストは不可** → デスクトップは仮想チェーン出力（オンスクリーン 8x8 描画）で代替し、実プロトコルはヘッドレスなエンコード/デコード単体テスト + 実機到着後の手動検証で担保する

---

## 2. アーキテクチャ設計

Battleship 同様、**ロジック（アニメーションコア）/ 出力（LED チェーン）/ 配布（BLE通信）/ UI を分離**し、
デスクトップでほぼ全機能を開発・検証できることを最優先する。

```
┌────────────────────────────────────────────────┐
│ View / ViewModel (LVGL v9)                     │  エディタ・シミュレータ・設定画面
├────────────────────────────────────────────────┤
│ AnimationController                            │  編集操作・再生タイマー
├──────────────┬─────────────────────────────────┤
│ AnimationCore │ IChainOutput                    │── RealChainOutput (Linux UART, 実プロトコル)
│ (純粋ロジック) │  8x8×最大10フレームを描画        │── SimulatorChainOutput (画面内 8x8 描画・開発既定)
│ フレーム編集   │                                  │
│ 判定・直列化   │ IAnimationTransport              │── LoopbackTransport (テスト)
│               │  アニメーションの配布/受信         │── TcpTransport (開発用・デスクトップ主経路)
│               │                                  │── BleTransport (実機・デフォルト配布経路)
└──────────────┴─────────────────────────────────┘
```

### 2.1 AnimationCore（純粋 C++・LVGL 非依存）

- **データモデル**: 1フレーム = 8×8 の `uint16_t`（RGB565）グリッド。アニメーション = フレーム配列（**最大10**）+ フレームごとの表示時間（ms）
  - **モノクロは色空間の判断ではなく出力ターゲットの設定**として扱う（下記2.2）。エディタ内部モデルは常に RGB565 に統一し、「実装を単純化しつつ Mono/RGB 両方に対応」という判断（設計判断・理由は本文書に記録）
- **編集操作**: ピクセル切替、フレーム追加/削除/複製/並べ替え、全消去、反転、シフト（上下左右）
- **直列化**: JSON で保存（ドットエディタのローカル保存用）。配布（BLE）用にも同一フォーマットを流用し、後述のチャンク分割で送信
- I/O・LVGL・スレッド非依存 → ユニットテストを厚く書く（フレーム操作・直列化往復・境界値：0フレーム/10フレーム超のガード）

### 2.2 出力レイヤ — IChainOutput

- `SimulatorChainOutput`: LVGL canvas に 8×8 セルを描画。**デスクトップ開発の既定経路**であり、Phase 2 の全機能はこれで検証完了する
- `RealChainOutput`: Linux 上で Grove ポートの UART（termios、`CHAINLED_UART_DEVICE` 環境変数で経路指定・ハードコード禁止）を開き、上記プロトコルで `SET_DISPLAY_BUFFER` を毎フレーム送信。Mono/RGB は起動時の enumeration（`GET_DEVICE_TYPE`）で自動判別し、Mono 接続時は RGB565→ON/OFF閾値変換（非ゼロ = ON）で描画
- 複数ユニットのデイジーチェーン接続（id 2,3,...への拡張）はプロトコル上は素通しで対応可能だが、**v1 では単一ユニット（id=1）のみを対象とし、複数枚連結は将来拡張として設計のみ残す**（過剰実装を避ける判断）

### 2.3 配布レイヤ — IAnimationTransport

Battleship の `ITransport` 抽象を踏襲。ただし本アプリはターン制対戦ではなく**アニメーションデータ（JSON、最大 ~1.3KB: 10フレーム×64px×2byte）のワンショット転送**のため、プロトコルは単純化する:

- フレーム: `[MAGIC][ver][type][seq][total_chunks][chunk_index][len][payload...][CRC16]`
- メッセージ種: `ANNOUNCE`（配布側が存在を通知）, `REQUEST`（受信側が取得要求）, `CHUNK`（分割データ）, `CHUNK_ACK`, `COMPLETE`, `ERROR`
- 信頼性層（seq+ACK+タイムアウト再送）を Battleship 同様アプリ側に持つ。BLE の MTU（既定20バイト程度）に合わせてチャンク分割
- 実装順: `LoopbackTransport`（テスト）→ `TcpTransport`（開発・デスクトップの主検証経路、LAN内P2P）→ `BleTransport`（実機・**デフォルトかつ唯一の配布方式**、BlueZ D-Bus、NUS互換GATT）
- **LoRa**: `ILoRaTransport` のインターフェースのみ用意し実装は将来対応（今回スコープ外・指示書§0の対象外に明記の通り）

### 2.4 UI（320×170・キーボードのみ）

| 画面 | 内容 |
|---|---|
| タイトル/メニュー | New Animation / Load / Simulate to Chain / Receive (BLE) / Settings |
| エディタ | 8×8 グリッド編集（矢印+Enterでピクセル切替）、フレームタイムライン（下部）、追加/削除/複製/並べ替え、再生プレビュー |
| シミュレータ | `SimulatorChainOutput` のオンスクリーン描画 + 出力ターゲット（Mono/RGB）切替 + 実機出力 ON/OFF |
| 配布 (Send/Receive) | BLE 相手探索 → 選択 → 転送進捗表示 |
| 設定 | 言語（EN/JA/ZH）、UART ポート情報表示（読み取り専用） |

- Esc 規約・キー操作ガイド常時表示は Battleship と同一方針

---

## 3. 開発フェーズ計画

**エミュレータファースト**。Phase 0–5 は Mac 上で完結。実機依存（RealChainOutput の実UART, BLE実通信）はヘッドレステスト+実機到着後の検証として明示的に残す。

| Phase | 内容 | 完了条件 |
|---|---|---|
| **0. 環境構築** | ✅ 完了。template 取得・git init・private repo 作成・SDL デスクトップビルド確認（fmt include 修正済み） | デスクトップでテンプレートアプリ起動 ✅ |
| **1. アニメーションコア** | `AnimationCore`（フレーム編集・直列化）実装 + GoogleTest/CTest 導入 | 全テスト green |
| **2. エディタ+シミュレータ UI** | エディタ画面・タイムライン・`SimulatorChainOutput` 描画 + APP_SCRIPT/screenshot 機構 | 通しでアニメーション作成→オンスクリーン再生確認、全画面スクショ目視OK |
| **3. Chain プロトコル + デバイス出力** | UART フレームのエンコード/デコード実装（ヘッドレステスト）、`RealChainOutput`（Linux termios）実装、Mono/RGB 判別・変換 | プロトコルのエンコード/デコード単体テスト green、Linux ビルド成功（実機との実通信検証は Phase 8） |
| **4. 配布プロトコル + TCP開発経路** | チャンク転送プロトコル + 再送層 + `LoopbackTransport`/`TcpTransport` 実装、Send/Receive UI | 2プロセス（Mac）でアニメーション転送が完走、ロス注入テスト green |
| **5. i18n + 設定永続化** | 文字列カタログ（EN/JA/ZH）、CJK フォントサブセット、設定ファイル永続化 | 3言語で全画面スクショOK、設定が再起動後も保持 |
| **6. BLE 配布（実機・デフォルト経路）** | `BleTransport` 実装（BlueZ D-Bus、NUS互換GATT） | ヘッドレステスト green + デバイスビルド成功（実機2台での実配布は Phase 8） |
| **7. パッケージング** | APPLaunch 準拠 .deb 化、app-builder.json、prepublish_check | prepublish 全パス |
| **8. 実機検証 → publish** | CardputerZero実機 + Chain Mono/RGB 実機での動作確認（UART出力・BLE配布・Esc規約等）、czdev publish | 実機チェックリスト完走、PR 提出 |

### フェーズ間の依存
- Phase 1 → 2 は直列。Phase 3（Chainプロトコル）は Phase 1 完了後、Phase 2 と並行着手可
- Phase 4（配布・TCP開発経路）は Phase 1 完了後、Phase 2/3 と並行着手可
- Phase 5（i18n）は Phase 2 完了後に着手（Phase 3/4 と並行可）
- Phase 6（BLE）は Phase 4 完了後
- Phase 7（パッケージング）は Phase 5・6 完了後。Phase 8（実機）はハードウェア到着後

### 必要な物品（ユーザー側で準備）
- CardputerZero 実機（Phase 8）
- Chain Mono ×1 + Chain RGB ×1（Phase 8。Chain Bridge 付属想定）
- ※Phase 0–7 は Mac 上のエミュレータ・ヘッドレステストのみで進行可能

---

## 4. リスクと対策

| リスク | 対策 |
|---|---|
| Chain プロトコルの一次情報がPDF非公開、Arduinoライブラリのソース解析に依存 | M5Chain（公式 Arduino ライブラリ）のソースコードから frame/CRC/コマンド値を直接確認済み（本文書§1.2）。実機到着後に実バイト列をロジックアナライザ等なしでも `RealChainOutput` のログ出力で検証 |
| デスクトップで実UART/実BLEが検証不可 | IChainOutput / IAnimationTransport の抽象化により、ロジック側は Simulator/Loopback/TCP でヘッドレステスト済みにしておき、実機依存部分のリスクを狭く限定する（Battleship の BLE 実装と同方針） |
| アニメーションデータ（~1.3KB）がBLE MTU（~20byte）よりずっと大きい | v1からチャンク分割+再送層で設計（Battleship の数バイトペイロード前提と異なる点に注意。§2.3のプロトコルはチャンク番号を持つ） |
| Mono(白黒)/RGB(フルカラー)の二種の出力ターゲットでエディタが二重実装になる懸念 | 内部モデルをRGB565に統一し、Mono出力時のみ閾値変換する設計で単一エディタに統一（§2.1/2.2の設計判断） |
| CJK フォント・.deb サイズ | Battleship の `tools/generate_cjk_fonts.py` 相当の手法（fontTools でグリフサブセット）を流用 |
| 複数 Chain ユニットのデイジーチェーン対応要求が後から出る可能性 | プロトコルの id フィールドは既に複数台対応済みのため、UIの拡張のみで対応可能な設計にとどめ、v1では実装しない（YAGNI） |

---

## 5. Battleship からの流用可能な既知の落とし穴

指示書§4に集約済み。特に本プロジェクトで即該当するもの:
- Homebrew fmt 12.2+ の `fmt/format.h` 修正（**Phase 0で既に適用済み**）
- `/dev/fb0` ハードコード禁止と同様、UART デバイスパスも `CHAINLED_UART_DEVICE` 環境変数で読む
- CJK フォントサブセット必須
- スクリーンショットファイル名のサニタイズ
