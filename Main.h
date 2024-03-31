#pragma once

#ifdef _DEBUG
#define ASSERT(Expression, Message) if (!(Expression)) { __ud2(); }
#else
#define ASSERT(Expression, Message) if (!(Expression)) { __ud2(); }
#endif

#define GAME_NAME "TEST"
#define ASSET_FILE "C:\\Users\\Frankenstein\\source\\repos\\Game Project\\Assets.dat"		////a fullyqualified directory, TODO: change somehow to a relative directory

#define GAME_RES_WIDTH 384		//maybe 400
#define GAME_RES_HEIGHT 240
#define GAME_BPP 32
#define GAME_DRAWING_AREA_MEMORY_SIZE (GAME_RES_WIDTH * GAME_RES_HEIGHT * (GAME_BPP/8))

#define NUMBER_OF_SFX_SOURCE_VOICES 8

#define CALCULATE_AVG_FPS_EVERY_X_FRAMES 25			//goal of 120fps
#define GOAL_MICROSECONDS_PER_FRAME 16667ULL		//16667 = 60fps

#include <Windows.h>

#include <xaudio2.h>                // Audio library

#pragma comment(lib, "XAudio2.lib") // Audio library.

#include <stdio.h>                  // String manipulation functions such as sprintf, etc.

#include <psapi.h>                  // Process Status API, e.g. GetProcessMemoryInfo

#include <stdint.h>                 // Nicer data types, e.g., uint8_t, int32_t, etc.

#pragma comment(lib, "Winmm.lib")   // Windows Multimedia library, we use it for timeBeginPeriod to adjust the global system timer resolution.

#define AVX                         // Valid options are SSE2, AVX, or nothing.

#ifdef AVX

#include <immintrin.h>              // AVX (Advanced Vector Extensions)

#elif defined SSE2

#include <emmintrin.h>              // SSE2 (Streaming SIMD Extensions)

#endif

#define SIMD

#define COLOR_BLACK (PIXEL32){ .Bytes = 0xFF000000 }
#define COLOR_DARK_RED (PIXEL32){ .Bytes = 0xFFa81000 }
#define COLOR_NES_RED (PIXEL32){ .Bytes = 0xFFB80020 }
#define COLOR_FORREST_GREEN (PIXEL32) { .Bytes = 0xFF007800 }
#define COLOR_DARK_BLUE (PIXEL32){ .Bytes = 0xFF0000bc }
#define COLOR_LIGHT_BLUE (PIXEL32){ .Bytes = 0xFF3cbcfc }
#define COLOR_NES_WHITE (PIXEL32){ .Bytes = 0xFFFCFCFC }
#define COLOR_DARK_WHITE (PIXEL32){ .Bytes = 0xFFDCDCDC }
#define COLOR_DARK_GRAY (PIXEL32){ .Bytes = 0xFF303030 }
#define COLOR_LIGHT_GRAY (PIXEL32){ .Bytes = 0xFFacacac }
#define COLOR_LIGHT_GREEN (PIXEL32){ .Bytes = 0xFF00b800 }
#define COLOR_DARK_GREEN (PIXEL32){ .Bytes = 0xFF005800 }
#define COLOR_NEON_BLUE (PIXEL32){ .Bytes = 0xFF3cbcfc }
#define COLOR_NES_PINK (PIXEL32){ .Bytes = 0xFFf85898 }
#define COLOR_GOLD (PIXEL32){ .Bytes = 0xFFFFD700 }
#define COLOR_NES_MAGENTA (PIXEL32){ .Bytes = 0xFFd800cc }

#define COLOR_NES_TAN (PIXEL32){ .Bytes = 0xFFfca044 }			
#define COLOR_NES_BROWN (PIXEL32){ .Bytes = 0xFF503000 }		
#define COLOR_NES_SKY_BLUE (PIXEL32){ .Bytes = 0xFFa4e4fc }		
#define COLOR_NES_ORANGE (PIXEL32){ .Bytes = 0xFFf83800 }		
#define COLOR_NES_BLUE (PIXEL32){ .Bytes = 0xFF0000fc }			
#define COLOR_NES_YELLOW (PIXEL32){ .Bytes = 0xFFf8b800 }		
#define COLOR_NES_GRAY (PIXEL32){ .Bytes = 0xFF7c7c7c }			
#define COLOR_NES_PURPLE (PIXEL32){ .Bytes = 0xFF940084 }		
#define COLOR_NES_LIGHT_GREEN (PIXEL32){ .Bytes = 0xFF00a844 }	
#define COLOR_NES_BLACK_RED (PIXEL32){ .Bytes = 0xFF160000}		

#define FONT_SHEET_CHARACTERS_PER_ROW 98

#define MAX_DIALOGUE_ROWS 7

typedef enum WINDOW_FLAGS
{
	WINDOW_FLAG_BORDERED = 1,
	WINDOW_FLAG_OPAQUE = 2,
	WINDOW_FLAG_HORIZ_CENTERED = 4,
	WINDOW_FLAG_VERT_CENTERED = 8,
	WINDOW_FLAG_SHADOWED = 16,
	WINDOW_FLAG_ROUNDED = 32,
	WINDOW_FLAG_THICK = 64

} WINDOW_FLAGS;

typedef enum LOGLEVEL
{
	LL_NONE = 0,
	LL_ERROR = 1,
	LL_WARNING = 2,
	LL_INFO = 3,
	LL_DEBUG = 4
} LOGLEVEL;

#define LOG_FILE_NAME GAME_NAME ".log"


typedef LONG(NTAPI* _NtQueryTimerResolution) (OUT PULONG MinimumResolution, OUT PULONG MaximumResolution, OUT PULONG CurrentResolution);

_NtQueryTimerResolution NtQueryTimerResolution;

typedef struct GAMEBITMAP
{
	BITMAPINFO BitmapInfo;
	void* Memory;
} GAMEBITMAP;

typedef union PIXEL32
{
	struct Colors {

		uint8_t Blue;
		uint8_t Green;
		uint8_t Red;
		uint8_t Alpha;

	} Colors;

	DWORD Bytes;

} PIXEL32;

typedef struct GAME_PERFORMANCE_DATA
{
	uint64_t TotalFramesRendered;
	float RawFPSAverage;
	float CookedFPSAverage;
	int64_t PerfFrequency;

	MONITORINFO MonitorInfo;
	BOOL DisplayDebugInfo;
	BOOL DisplayControlsHelp;
	LONG MinimumTimerResolution;		//if warning C4057 make ULONG instead
	LONG MaximumTimerResolution;		//c4057
	LONG CurrentTimerResolution;		//C4057

	DWORD HandleCount;
	PROCESS_MEMORY_COUNTERS_EX MemInfo;
	SYSTEM_INFO SystemInfo;
	int64_t CurrentSystemTime;
	int64_t PreviousSystemTime;
	double CPUPercent;

	uint8_t MaxScaleFactor;
	uint8_t CurrentScaleFactor;

} GAME_PERFORMANCE_DATA;

typedef struct TILEMAP
{

	uint16_t Width;

	uint16_t Height;

	uint8_t** Map;

} TILEMAP;

typedef struct MENUITEM
{
	char* Name;

	int16_t x;

	int16_t y;

	BOOL Enabled;

	void(*Action)(void);

} MENUITEM;

typedef struct MENU
{
	char* Name;

	uint8_t SelectedItem;

	uint8_t ItemCount;

	MENUITEM** Items;

} MENU;

IXAudio2SourceVoice* gXAudioSFXSourceVoice[NUMBER_OF_SFX_SOURCE_VOICES];
IXAudio2SourceVoice* gXAudioMusicSourceVoice;

uint8_t gSFXVolume;
uint8_t gMusicVolume;

BOOL gMusicPaused;

typedef struct GAMESOUND
{
	WAVEFORMATEX WaveFormat;
	XAUDIO2_BUFFER Buffer;

} GAMESOUND;

GAMESOUND gSoundMenuNavigate;
GAMESOUND gSoundMenuChoose;
GAMESOUND gSoundSplashScreen;

typedef struct REGISTRYPARAMS
{
	DWORD LogLevel;

	DWORD SFXVolume;

	DWORD MusicVolume;

	DWORD ScaleFactor;

	BOOL FullScreen;

	DWORD TextSpeed;

} REGISTRYPARAMS;


HWND gGameWindow;

BOOL gGameIsRunning;

GAMEBITMAP gBackBuffer;

GAMEBITMAP g6x7Font;

GAME_PERFORMANCE_DATA gGamePerformanceData;

HANDLE gAssetLoadingThreadHandle;

HANDLE gEssentialAssetsLoadedEvent;     ////event gets signaled after essential assets have been loaded (mostly req for splash screen)

REGISTRYPARAMS gRegistryParams;


LRESULT CALLBACK MainWindowProc(_In_ HWND WindowHandle, _In_ UINT Message, _In_ WPARAM WParam, _In_ LPARAM LParam);

DWORD CreateMainGameWindow(void);

BOOL GameIsAlreadyRunning(void);

void ProcessPlayerInput(void);

DWORD Load32BppBitmapFromFile(_In_ char* FileName, _Inout_ GAMEBITMAP* GameBitmap);

void BlitBackgroundToBuffer(_In_ GAMEBITMAP* GameBitmap, _In_ int16_t BrightnessAdjustment);

void Blit32BppBitmapToBuffer(_In_ GAMEBITMAP* GameBitmap, _In_ int16_t x, _In_ int16_t y, _In_ int16_t BrightnessAdjustment);

void BlitStringToBuffer(_In_ char* String, _In_ GAMEBITMAP* FontSheet, _In_ PIXEL32* Color, _In_ uint16_t x, _In_ uint16_t y);

void RenderFrameGraphics(void);

DWORD LoadRegistryParameters(void);

void LogMessageA(_In_ DWORD LogLevel, _In_ char* Message, _In_ ...);

void DrawDebugInfo(void);

#ifdef SIMD
void ClearScreenColor(_In_ __m128i* Color);
#else
void ClearScreenColor(_In_ PIXEL32* Color);
#endif


void DrawWindow(
	_In_opt_ uint16_t x,
	_In_opt_ uint16_t y,
	_In_ int16_t Width,
	_In_ int16_t Height,
	_In_opt_ PIXEL32* BorderColor,
	_In_opt_ PIXEL32* BackgroundColor,
	_In_opt_ PIXEL32* ShadowColor,
	_In_ DWORD Flags);

void InitializeGlobals(void);
