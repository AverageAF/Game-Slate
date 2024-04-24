#pragma once

#ifdef _DEBUG
#define ASSERT(Expression, Message) if (!(Expression)) { __ud2(); }
#else
#define ASSERT(Expression, Message) if (!(Expression)) { __ud2(); }
#endif

#define GAME_NAME "TEST"
#define GAME_VERSION "Alpha 0.0.1"
#define ASSET_FILE "C:\\Users\\Frankenstein\\source\\repos\\Game Project\\Assets.dat"		////a fullyqualified directory, TODO: change somehow to a relative directory

#define GAME_RES_WIDTH 384		//384
#define GAME_RES_HEIGHT 240		//240
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

#define FADE_DURATION_FRAMES 20

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

typedef enum GAMESTATE
{
	GAMESTATE_SPLASHSCREEN,
	GAMESTATE_MAINMENU,
	GAMESTATE_OPTIONS,
	GAMESTATE_SAVELOAD,
	//GAMESTATE_LOAD,
	GAMESTATE_PLAY,
	//GAMESTATE_FIGHT,
	//GAMESTATE_GAME,			//whatever type of game I want

} GAMESTATE;

GAMESTATE gCurrentGameState;
GAMESTATE gPreviousGameState;
GAMESTATE gDestinationGameState;

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

typedef enum INPUT_KEYS
{
	INPUT_ESCAPE = 1,
	INPUT_DEBUG = 2,
	INPUT_WUP = 4,
	INPUT_ALEFT = 8,
	INPUT_SDOWN = 16,
	INPUT_DRIGHT = 32,
	INPUT_E = 64,
	INPUT_H = 128,
	INPUT_X = 256,
	INPUT_Q = 512,
	INPUT_TAB = 1024,
	INPUT_CTRL = 2048,
	INPUT_DEL = 4096,

} INPUT_KEYS;

typedef struct GAMEINPUT
{
	int16_t EscapeKeyPressed;		////key inputs
	int16_t DebugKeyPressed;
	int16_t SDownKeyPressed;
	int16_t ALeftKeyPressed;
	int16_t DRightKeyPressed;
	int16_t WUpKeyPressed;
	int16_t EKeyPressed;
	int16_t HKeyPressed;
	int16_t XKeyPressed;
	int16_t TabKeyPressed;
	int16_t QKeyPressed;
	int16_t CtrlKeyPressed;
	int16_t DelKeyPressed;

	int16_t EscapeKeyAlreadyPressed;	////for pulse responces
	int16_t DebugKeyAlreadyPressed;
	int16_t SDownKeyAlreadyPressed;
	int16_t ALeftKeyAlreadyPressed;
	int16_t DRightKeyAlreadyPressed;
	int16_t WUpKeyAlreadyPressed;
	int16_t EKeyAlreadyPressed;
	int16_t HKeyAlreadyPressed;
	int16_t XKeyAlreadyPressed;
	int16_t TabKeyAlreadyPressed;
	int16_t QKeyAlreadyPressed;
	int16_t CtrlKeyAlreadyPressed;
	int16_t DelKeyAlreadyPressed;

} GAMEINPUT;

GAMEINPUT gGameInput;

typedef struct TILEMAP
{

	uint16_t Width;

	uint16_t Height;

	uint8_t** Map;

} TILEMAP;

typedef enum MENU_FLAGS
{
	MENU_BOX2x2 = 1,
	MENU_BOX3x3,
	MENU_BOX4x4,
	MENU_BOX2x3,
	MENU_BOX2x4,
	MENU_BOX1x2,
	MENU_BOX1x3,
	MENU_BOX1x4,
	MENU_BOX4x1,
	MENU_BOX8x1,

} MENU_FLAGS;

typedef enum MODIFYMENU_FLAGS
{
	MODIFYMENU_ACTIVATE,
	MODIFYMENU_DEACTIVATE,
	MODIFYMENU_NAME,
	MODIFYMENU_X,
	MODIFYMENU_Y,
	MODIFYMENU_WIDTH,
	MODIFYMENU_HEIGHT,
	MODIFYMENU_SELECTED,
	//fontsheet??

} MODIFYMENU_FLAGS;

typedef enum MODIFYMENUITEM_FLAGS
{
	MODIFYITEM_NAME,
	MODIFYITEM_X,
	MODIFYITEM_Y,
	MODIFYITEM_WIDTH,
	MODIFYITEM_HEIGHT,
	MODIFYITEM_ENABLE,
	MODIFYITEM_DISABLE,
	MODIFYITEM_ACTION,

} MODIFYMENUITEM_FLAGS;

typedef enum MENUFUNC_FLAGS
{
	MENUFUNC_QUIT,
	MENUFUNC_GOTO_GS,
	MENUFUNC_PREV_GS,
	MENUFUNC_SFX_VOL,
	MENUFUNC_MUSIC_VOL,
	MENUFUNC_SCREENSCALE,
	MENUFUNC_ALLOWACTION,

	//MENUFUNC_WHATEVER,			////I will need a lot of these over time...

} MENUFUNC_FLAGS; 

typedef struct MENUPALLET
{
	PIXEL32 border1;

	PIXEL32 background1;

	PIXEL32 border2;

	PIXEL32 background2;

	PIXEL32 itemborder;

	PIXEL32 itembackground;

	PIXEL32 text;

	PIXEL32 cursor;

} MENUPALLET;

#define NAMELENGTH_MAX 32

typedef struct MENUITEM
{
	char Name[NAMELENGTH_MAX];

	int16_t x;

	int16_t y;

	uint16_t width;

	uint16_t height;

	BOOL Enabled;

	void(*Action)(void);

} MENUITEM;

typedef struct MENU
{
	BOOL Active;

	char Name[NAMELENGTH_MAX];

	int16_t x;

	int16_t y;

	uint16_t width;

	uint16_t height;

	uint8_t SelectedItem;

	uint8_t Rows;

	uint8_t Columns;

	GAMEBITMAP* FontSheet;

	MENUITEM** Items;

	MENUPALLET Pallet;

} MENU;

MENUITEM gMenuItem[256];
uint8_t gMenuItemCount;

#define MAX_MENUS 16

//there is a pointer here for every menu in MAX_MENUS
//16 is largest menu items NOT MAX_MENUS, leaving room for bigger menus potentially
MENUITEM* gMenuItemPtr0[16];
MENUITEM* gMenuItemPtr1[16];
MENUITEM* gMenuItemPtr2[16];
MENUITEM* gMenuItemPtr3[16];
MENUITEM* gMenuItemPtr4[16];
MENUITEM* gMenuItemPtr5[16];
MENUITEM* gMenuItemPtr6[16];
MENUITEM* gMenuItemPtr7[16];
MENUITEM* gMenuItemPtr8[16];
MENUITEM* gMenuItemPtr9[16];
MENUITEM* gMenuItemPtr10[16];
MENUITEM* gMenuItemPtr11[16];
MENUITEM* gMenuItemPtr12[16];
MENUITEM* gMenuItemPtr13[16];
MENUITEM* gMenuItemPtr14[16];
MENUITEM* gMenuItemPtr15[16];

uint8_t gActiveMenus;
MENU gMenuBuffer[MAX_MENUS];

#define MENUSTRLENGTH_MAX 32
char gMenuStrBuffer[MENUSTRLENGTH_MAX];	//for applying name changes to gMenuItem

uint8_t gSelectedMenuItem;
uint8_t gMenuIndexBuffer;
DWORD gMenuItemActionParam;
BOOL gPopMenu;

//TOREMOVE:??? do i need this??
//MENUPALLET gMenuPallets[MAX_MENUS];

/////audio


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
GAMEBITMAP g4x5Font;
GAMEBITMAP g4x5CAPSFont;

GAME_PERFORMANCE_DATA gGamePerformanceData;

HANDLE gAssetLoadingThreadHandle;

HANDLE gEssentialAssetsLoadedEvent;     ////event gets signaled after essential assets have been loaded (mostly req for splash screen)

REGISTRYPARAMS gRegistryParams;

BOOL gYesNoScreen;
BOOL gFade;
BOOL gInputEnabled;
BOOL gDialogueControls;
BOOL gFinishedDialogueTextAnimation;

HRESULT InitializeSoundEngine(void);

DWORD LoadWaveFromMem(_In_ void* Buffer, _Inout_ GAMESOUND* GameSound);
DWORD LoadTileMapFromMem(_In_ void* Buffer, _In_ uint32_t BufferSize, _Inout_ TILEMAP* TileMap);
DWORD LoadOggFromMem(_In_ void* Buffer, _In_ uint32_t BufferSize, _Inout_ GAMESOUND* GameSound);

DWORD LoadAssetFromArchive(_In_ char* Archive, _In_ char* AssetFileName, _Inout_ void* Resource);
DWORD AssetLoadingThreadProc(_In_ LPVOID lpParam);

LRESULT CALLBACK MainWindowProc(_In_ HWND WindowHandle, _In_ UINT Message, _In_ WPARAM WParam, _In_ LPARAM LParam);

DWORD CreateMainGameWindow(void);

BOOL GameIsAlreadyRunning(void);

void ProcessPlayerInput(void);

void ProcessGameTickCalculation(void);

DWORD Load32BppBitmapFromFile(_In_ char* FileName, _Inout_ GAMEBITMAP* GameBitmap);

void BlitBackgroundToBuffer(_In_ GAMEBITMAP* GameBitmap, _In_ int16_t BrightnessAdjustment);

void Blit32BppBitmapToBuffer(_In_ GAMEBITMAP* GameBitmap, _In_ int16_t x, _In_ int16_t y, _In_ int16_t BrightnessAdjustment);

void BlitStringToBuffer(_In_ char* String, _In_ GAMEBITMAP* FontSheet, _In_ PIXEL32* Color, _In_ uint16_t x, _In_ uint16_t y);

void RenderFrameGraphics(void);

DWORD LoadRegistryParameters(void);
DWORD SaveRegistryParameters(void);

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

void DrawDialogueBox(_In_ char* Dialogue, _In_opt_ uint64_t Counter, _In_opt_ DWORD Flags);

void ApplyFadeIn(_In_ uint64_t FrameCounter, _In_ PIXEL32 DefaultTextColor, _Inout_ PIXEL32* TextColor, _Inout_opt_ int16_t* BrightnessAdjustment);

void QuitGame(void);

void DrawMenu(_In_ MENU menu);

MENU CreateMenuObj(
	_In_ uint16_t menuX,
	_In_ uint16_t menuY,
	_In_ uint16_t widthX,
	_In_ uint16_t widthY,
	_In_ uint16_t itemWidthX,
	_In_ uint16_t itemWidthY,
	_In_opt_ MENUPALLET pallet,
	_In_opt_ GAMEBITMAP* fontsheet,
	_In_opt_ DWORD flags
);

BOOL StoreMenuObj(_In_ MENU menu, _In_opt_ uint8_t index);
MENU ReturnStoredMenuObj(_In_opt_ uint8_t index);
MENU PlayerInputToMenuObj(_Inout_ MENU menu, INPUT_KEYS input);
MENU ClearMenu(void);
BOOL DeleteGameMenu(uint8_t index);

int16_t WASDMenuNavigation(BOOL isactive);
BOOL PlayerInputWUp(void);
BOOL PlayerInputALeft(void);
BOOL PlayerInputSDown(void);
BOOL PlayerInputDRight(void);
BOOL PlayerInputEKey(void);
BOOL PlayerInputEscape(void);

void GoToDestGamestate(GAMESTATE destination);
void GoToPrevGamestate(void);

MENUITEM* AllocatePtrToMenuObj(_In_ MENU menu);

BOOL SetMenuStringBuffer(char* string);
MENU ModifyMenuItemData(MENU menu, uint8_t item16, DWORD flag, _In_opt_ uint16_t inputvalue);
MENU ModifyMenuObjData(MENU menu, DWORD flag, _In_opt_ uint16_t inputvalue);

void PlayGameSound(_In_ GAMESOUND* GameSound);
void PauseGameMusic(void);
void StopGameMusic(void);
void PlayGameMusic(_In_ GAMESOUND* GameSound, _In_ BOOL Looping, _In_ BOOL Immediate);
BOOL MusicIsPlaying(void);

uint8_t FindCurrentMenu(void);

void CreateYesNoMenu(void);
void DrawYesNoMenu(void);
void PPI_YesNoMenu(void);

void StoreSelectedMenuItem(uint8_t selecteditem, uint8_t menuindex, BOOL popmenu, _In_opt_ DWORD actionparam);
void SelectMenuItemFromStoredMenu(void);
