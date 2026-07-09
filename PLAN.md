# Chain LED Simulation for CardputerZero — 設計・開発計画（Rev.2）

8x8 ドットアニメーション（最大10コマ）を CardputerZero 上で作成・シミュレーションし、
実機の Chain Mono / Chain RGB（M5Stack Chain シリーズ 8x8 LED マトリクスユニット）に
繋いで実際に表示できるアプリ。作成したアニメーションは BLE で配布・受信できる
（LoRa は将来対応、対象外）。**1枚目のコマを描くと、残りのコマをローカルLLMが
自動生成する機能を持つ**（Rev.2 で追加。任意のテキストヒント対応、LLM 不可時は
プロシージャル生成にフォールバック）。

参考: [Battleship for CardputerZero](https://github.com/yutacar/Battleship)（同テンプレート・同進め方の先行プロジェクト）。
本文書は `docs/APP_DEV_INSTRUCTION_TEMPLATE.md` の指示書に基づく。

**Rev.2 変更点（2026-07-09、ユーザー指示）**: 「1枚目を描くと残りのコマをローカルLLMで
生成する」機能を追加。Battleship では同じハードウェア（RAM 512MB, A53@1GHz）上での
ローカルLLM常駐（対戦中のリアルタイム解説文生成）を速度/サイズ面から最終的に全廃した
経緯がある（[[battleship-cardputerzero-project]] 参照）。本アプリのLLM利用は
**対戦中のリアルタイム性が不要なワンショット・オフライン生成**である点が異なるため
再度採用するが、その教訓を踏まえ (a) Phase 3 冒頭で候補モデルのRAM/速度を実機借用前に
まず定量評価する Go/No-Go チェックポイントを設け、(b) LLM 不使用でも動作する
プロシージャル・フォールバックを既定の外枠として先に実装する、の2点を設計に反映した。

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
┌──────────────────────────────────────────────────────┐
│ View / ViewModel (LVGL v9)                           │  エディタ・シミュレータ・生成・設定画面
├──────────────────────────────────────────────────────┤
│ AnimationController                                  │  編集操作・生成トリガー・再生タイマー
├──────────────┬───────────────────┬────────────────────┤
│ AnimationCore │ AnimationGenerator │ IChainOutput      │── RealChainOutput (Linux UART, 実プロトコル)
│ (純粋ロジック) │  1枚目→残り自動生成 │  8x8を描画          │── SimulatorChainOutput (画面内描画・開発既定)
│ フレーム編集   │  ├ ProceduralCompleter(既定・依存ゼロ)│                    │
│ 判定・直列化   │  └ LlmCompleter(任意・llama.cpp+GBNF) │ IAnimationTransport│── LoopbackTransport (テスト)
│               │                                    │  配布/受信          │── TcpTransport (開発用主経路)
│               │                                    │                    │── BleTransport (実機デフォルト)
└──────────────┴───────────────────┴────────────────────┘
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

### 2.4 AnimationGenerator — 1枚目から残りコマを自動生成（Rev.2 追加）

**Phase 3実施結果（2026-07-09）: `ProceduralCompleter` 実装済み・稼働中
（Editor画面 `G` キー）。`LlmCompleter` は Go/No-Go実測の結果 No-Go 判定となり
未実装（判断根拠・実測データは NOTES.md「Phase 3: Go/No-Go 実測」参照。
決定的な理由は速度ではなく **モデルファイル単体で258MB — 総RAM512MBの半分以上を
占有する** という構造的な制約）。以下は Rev.2 時点の設計意図として残す
（`AnimationGenerator` インターフェースは将来 `LlmCompleter` を追加する場合の
拡張点として現存）。**

エディタで1枚目のコマを描いた状態で「Generate」操作（明示的なキー操作。描画中に
自動発火はしない — 途中経過での誤発火を避ける判断）を行うと、目標フレーム数
（2〜10、既定は指示書のUIで選択）まで残りのコマを自動生成する。

- **`AnimationGenerator` インターフェース**を切り、2実装を持つ:
  - **`ProceduralCompleter`（既定・常時利用可能・依存ゼロ）**: 1枚目のパターンに対し
    平行移動/反転/回転/色相回転などの決定的な変換を繰り返し適用してコマ列を生成する
    ヒューリスティック。LLM 不要でユニットテストも容易 — **Phase 3 はまずこれを実装し、
    LLM 抜きでも機能が成立する状態を最初に作る**
  - **`LlmCompleter`（任意・モデル要配置）**: ローカル小型 LLM（llama.cpp、候補
    SmolLM2-135M/360M クラス、量子化）を用いてコマを1枚ずつ逐次生成する
- **フレーム表現（LLMの出力を短く・確実にするための設計）**: 1枚目に使われている
  色から最大9色のパレットを抽出し、各フレームを「パレット番号1桁×64ピクセル」の
  64文字の数字列としてエンコードする（Mono は自然に "0"/"1" の2値になる）。
  llama.cpp の **GBNF 文法**で出力を「厳密に64桁の数字列」に制約し、フリーテキスト
  生成によるパース失敗・暴走を防ぐ
- **生成方式**: フレームごとの逐次生成（フレーム1→2→…→N、直前までのフレーム列を
  文脈として次の1枚を生成）。理由: (a) 1回あたりの出力長を64文字に固定できる、
  (b) 小型モデルでも文脈が短く安定しやすい、(c) 生成順に画面へプレビュー表示できる
- **任意のテキストヒント**: エディタ画面に空欄可の1行テキスト入力を追加（例:
  「跳ねるボール」「点滅」）。空欄なら1枚目の絵柄のみからの継続として生成する
  （ユーザー確認済み: Rev.2 質問への回答）
- **Go/No-Go 実測（Phase 3 冒頭で実施）**: Battleship で同一ハードウェア上の
  常駐LLM（対戦中のリアルタイム解説文生成）が RAM/速度面で不採用になった経緯が
  あるため、本機能を作り込む前に候補モデルの **RAM 使用量・1コマあたりの生成時間**
  を実機（またはエミュレータ上限に近い制約でのベンチ）で計測し、実用に耐えるか
  判断する。不採用となっても `ProceduralCompleter` のみで機能は完成しているため、
  アプリ自体の価値は損なわれない設計
- **配布判断**: Battleship 同様、**LLM モデルファイルは .deb に同梱しない**
  （256MB級は配布に不適切）。`LlmCompleter` はモデルファイルが設定で指定された
  パスに存在する場合のみ有効化され、無い場合は自動的に `ProceduralCompleter`
  にフォールバックする。ストア説明文にはこの挙動を正確に記載する（指示書§0.8）
- ビルドフラグ `CHAINLED_LLM`（既定 ON。デバイスで著しく非現実的と判明した場合は
  Battleship の `BATTLESHIP_LLM` 同様 OFF 既定への切り替えも選択肢として残す）

### 2.5 UI（320×170・キーボードのみ）

| 画面 | 内容 |
|---|---|
| タイトル/メニュー | New Animation / Load / Simulate to Chain / Receive (BLE) / Settings |
| エディタ | 8×8 グリッド編集（矢印+Enterでピクセル切替）、フレームタイムライン（下部）、追加/削除/複製/並べ替え、再生プレビュー、**Generate操作（目標コマ数・任意テキストヒント入力→残りコマ自動生成）** |
| シミュレータ | `SimulatorChainOutput` のオンスクリーン描画 + 出力ターゲット（Mono/RGB）切替 + 実機出力 ON/OFF |
| 配布 (Send/Receive) | BLE 相手探索 → 選択 → 転送進捗表示 |
| 設定 | 言語（EN/JA/ZH）、UART ポート情報表示（読み取り専用）、LLMモデルパス表示（読み取り専用、未配置なら「Procedural fallback active」等の表示） |

- Esc 規約・キー操作ガイド常時表示は Battleship と同一方針

---

## 3. 開発フェーズ計画

**エミュレータファースト**。Phase 0–8 は Mac 上で完結。実機依存（RealChainOutput の実UART, BLE実通信, LlmCompleter の実機RAM/速度）はヘッドレステスト+実機到着後の検証として明示的に残す。

| Phase | 内容 | 完了条件 |
|---|---|---|
| **0. 環境構築** | ✅ 完了。template 取得・git init・private repo 作成・SDL デスクトップビルド確認（fmt include 修正済み） | デスクトップでテンプレートアプリ起動 ✅ |
| **1. アニメーションコア** | `AnimationCore`（フレーム編集・直列化）実装 + GoogleTest/CTest 導入 | 全テスト green |
| **2. エディタ+シミュレータ UI** | エディタ画面・タイムライン・`SimulatorChainOutput` 描画 + APP_SCRIPT/screenshot 機構 | 通しでアニメーション作成→オンスクリーン再生確認、全画面スクショ目視OK |
| **3. 残りコマ自動生成（Rev.2 新設）** | ✅ 完了。`ProceduralCompleter` 実装 + Go/No-Go実測 → **No-Go判定**（`LlmCompleter` は未実装、詳細は NOTES.md） | `ProceduralCompleter` で1枚目→残りコマ生成のテスト green ✅（10テスト）。Go/No-Go実測・判断理由を NOTES.md に記録 ✅ |
| **4. Chain プロトコル + デバイス出力** | ✅ 完了。UART フレームのエンコード/デコード実装、`RealChainOutput`（Linux termios）実装、Mono/RGB 変換、Simulator画面に実機出力ON/OFF統合 | プロトコル単体テスト green ✅（17テスト）、CM0クロスビルド成功 ✅（Docker+BSP、循環リンクの実バグを発見・修正。詳細NOTES.md）。実機との実通信検証は Phase 9 |
| **5. 配布プロトコル + TCP開発経路** | ✅ 完了。チャンク転送プロトコル + 再送層 + `LoopbackTransport`/`TcpTransport` 実装、Send/Receive UI | 2プロセス（Mac）でアニメーション転送が完走 ✅、ロス注入テスト green ✅（実プロセス検証で実バグ2件発見・修正、詳細NOTES.md） |
| **6. i18n + 設定永続化** | 文字列カタログ（EN/JA/ZH）、CJK フォントサブセット、設定ファイル永続化 | 3言語で全画面スクショOK、設定が再起動後も保持 |
| **7. BLE 配布（実機・デフォルト経路）** | `BleTransport` 実装（BlueZ D-Bus、NUS互換GATT） | ヘッドレステスト green + デバイスビルド成功（実機2台での実配布は Phase 9） |
| **8. パッケージング** | APPLaunch 準拠 .deb 化、app-builder.json、prepublish_check、（Go判定時）LLMモデル同梱なし運用のREADME整備 | prepublish 全パス |
| **9. 実機検証 → publish** | CardputerZero実機 + Chain Mono/RGB 実機での動作確認（UART出力・BLE配布・Esc規約・LlmCompleter実測等）、czdev publish | 実機チェックリスト完走、PR 提出 |

### フェーズ間の依存
- Phase 1 → 2 は直列。Phase 3（残りコマ自動生成）は Phase 2 完了後（エディタが要る）に着手
- Phase 4（Chainプロトコル）は Phase 1 完了後、Phase 2/3 と並行着手可
- Phase 5（配布・TCP開発経路）は Phase 1 完了後、Phase 2/3/4 と並行着手可
- Phase 6（i18n）は Phase 3 完了後に着手（Phase 4/5 と並行可。Generate画面の文言も対象のため）
- Phase 7（BLE）は Phase 5 完了後
- Phase 8（パッケージング）は Phase 6・7 完了後。Phase 9（実機）はハードウェア到着後

### 必要な物品（ユーザー側で準備）
- CardputerZero 実機（Phase 9）
- Chain Mono ×1 + Chain RGB ×1（Phase 9。Chain Bridge 付属想定）
- （Go判定時）ローカルLLM検証用の量子化モデルファイル（候補: SmolLM2-135M/360M クラスのGGUF。入手先はPhase 3着手時に選定）
- ※Phase 0–8 は Mac 上のエミュレータ・ヘッドレステストのみで進行可能

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
| ~~ローカルLLM（`LlmCompleter`）がRAM512MB/A53@1GHzで実用速度に達しない~~ | **解消済み（2026-07-09、Phase 3 Go/No-Go実測でNo-Go判定）**: モデルファイル単体で258MB（総RAM512MBの半分以上）という構造的制約が決定打。`LlmCompleter`は未実装のまま、`ProceduralCompleter`のみで機能完成。実測データはNOTES.md参照。実機到着時 or 大幅に小さいモデルが見つかった場合に再検討 |

---

## 5. Battleship からの流用可能な既知の落とし穴

指示書§4に集約済み。特に本プロジェクトで即該当するもの:
- Homebrew fmt 12.2+ の `fmt/format.h` 修正（**Phase 0で既に適用済み**）
- `/dev/fb0` ハードコード禁止と同様、UART デバイスパスも `CHAINLED_UART_DEVICE` 環境変数で読む
- CJK フォントサブセット必須
- スクリーンショットファイル名のサニタイズ
