#pragma once

// Window生成とGameTimerで共有する、コンパイル時に確定した設定値を定義する。
// 設定ファイル読込・実行時変更・Resize後のサイズ保持は担当しない。公開名・型・値は変更しない。

//========= C++標準ライブラリ インクルード=========
#include <cstdint> // 起動時の幅・高さを、符号なし32bit整数のstd::uint32_tで定義する。

// アプリケーション共通のWindow設定と、ゲーム時間へ適用する上限をまとめる。
// Configと各定数名はmain.cpp・GameTimer.cppの既存参照を維持するための命名例外。
// 今回は改名せず、namespaceのアッパースネークケース化と定数のns_追加は参照更新を伴う別工程とする。
namespace Config
{
	//========= ウィンドウ設定定数=========
	// 起動時のクライアント領域とGraphicsSystemの初期描画先へ指定する幅。1280pixelという既存設定。
	// main.cppは外枠を含むWindow寸法へAdjustWindowRectで換算する。Resize後の現在幅を表す値ではない。
	constexpr std::uint32_t WINDOW_WIDTH = 1280;
	// 同じ初期領域の高さ。720pixelという既存設定で、幅と合わせた初期比率は16:9。
	// 1280×720を採用した経緯は未確認。ここで値を変えてもHudCanvasの基準サイズは自動では変わらない。
	constexpr std::uint32_t WINDOW_HEIGHT = 720;
	// Window Class登録・Window生成・登録解除で同じClassを識別する終端付きワイド文字列。
	// 画面上のタイトルとは別の用途。識別文字列は既存値を維持し、このHeader内に固定データとして定義する。
	constexpr wchar_t WINDOW_CLASS_NAME[] = L"GunfireLikeWindowClass";
	// CreateWindowExWへ渡す初期タイトルの終端付きワイド文字列。Class名や実行ファイル名を変更する設定ではない。
	constexpr wchar_t WINDOW_TITLE[] = L"GunfireLike DX11";

	//========= フレーム時間設定定数=========
	// GameTimerが実測フレーム秒数に適用する上限。0.1秒=100ミリ秒という既存値を維持する。
	// min(実時間,この値)へTimeScaleを掛けるため、倍率が1より大きいと最終DeltaTimeはこの値を超え得る。
	// FPS集計にはこの制限を使わない。フレームを待機させる処理や、10FPSへ固定する設定でもない。
	// 具体値0.1の採用経緯は未確認。処理の用途と単位を示し、性能・安全性の保証は追加しない。
	constexpr float MAX_DELTA_TIME = 0.1f;
}