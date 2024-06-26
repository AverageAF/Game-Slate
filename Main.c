#pragma warning(push, 3)
#include <stdio.h>
#include <windows.h>
#include <psapi.h>
#include <emmintrin.h>
#include <xaudio2.h>
#include <stdint.h>
#include "stb_vorbis.h"
#include <string.h>
#pragma warning(pop)

#include "Main.h"
#include "miniz.h"
#include "SplashScreen.h"
#include "MainMenu.h"
#include "OptionsScreen.h"

CRITICAL_SECTION gLogCritSec;

const int16_t gFadeBrightnessGradient[] = {
    -255, -255, -255, -255, -255, //-255, -255, -255, -255, -255,
    -128, -128, -128, -128, -128, //-128, -128, -128, -128,-128,
    -64, -64, -64, -64, -64, //-64, -64, -64, -64, -64,
    -32, -32, -32, -32, -32 //-32, -32, -32, -32, -32
};

BOOL gWindowHasFocus;

IXAudio2* gXAudio;

IXAudio2MasteringVoice* gXAudioMasteringVoice;

uint8_t gSFXSourceVoiceSelector;

//////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////

int WinMain(HINSTANCE Instance, HINSTANCE PreviousInstance, PSTR CommandLine, int CmdShow)
{
    UNREFERENCED_PARAMETER(Instance);
    UNREFERENCED_PARAMETER(PreviousInstance);
    UNREFERENCED_PARAMETER(CommandLine);
    UNREFERENCED_PARAMETER(CmdShow);

    MSG Message = { 0 };

    int64_t FrameStart = 0;
    int64_t FrameEnd = 0;
    int64_t ElapsedMicroseconds = 0;
    int64_t ElapsedMicrosecondsPerFrameAccumulatorRaw = 0;
    int64_t ElapsedMicrosecondsPerFrameAccumulatorCooked = 0;

    FILETIME ProcessCreationTime = { 0 };
    FILETIME ProcessExitTime = { 0 };
    int64_t CurrentUserCPUTime = 0;
    int64_t CurrentKernelCPUTime = 0;
    int64_t PreviousUserCPUTime = 0;
    int64_t PreviousKernelCPUTime = 0;

    HANDLE ProcessHandle = GetCurrentProcess();
    HMODULE NtDllModuleHandle = NULL;

    InitializeGlobals();

    //this crit section is used to sync access to log file with LogMessageA when used by multiple threads
#pragma warning(suppress: 6031)
    InitializeCriticalSectionAndSpinCount(&gLogCritSec, 0x400);

    gEssentialAssetsLoadedEvent = CreateEventA(NULL, TRUE, FALSE, "gEssentialAssetsLoadedEvent");

    if (LoadRegistryParameters() != ERROR_SUCCESS)
    {
        goto Exit;
    }

    LogMessageA(LL_INFO, "[%s] Starting %s version %s", __FUNCTION__, GAME_NAME, GAME_VERSION);

    if (GameIsAlreadyRunning() == TRUE)
    {
        LogMessageA(LL_WARNING, "[%s] Another instance is already running!", __FUNCTION__);
        MessageBoxA(NULL, "Another instance of this program is already running!", "Error!", MB_ICONERROR | MB_OK);
        goto Exit;
    }

    if ((NtDllModuleHandle = GetModuleHandleA("ntdll.dll")) == NULL)
    {
        LogMessageA(LL_ERROR, "[%s] Couldn't load ntdll.dll! Error 0x%08lx!", __FUNCTION__, GetLastError());
        MessageBoxA(NULL, "Couldn't load ntdll.dll!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    if ((NtQueryTimerResolution = (_NtQueryTimerResolution)GetProcAddress(NtDllModuleHandle, "NtQueryTimerResolution")) == NULL)
    {
        LogMessageA(LL_ERROR, "[%s] Couldn't find function NtQueryTimerResolution in ntdll.dll! GetProcAddress Failed! Error 0x%08lx!", __FUNCTION__, GetLastError());
        MessageBoxA(NULL, "Couldn't find function NtQueryTimerResolution in ntdll.dll!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    NtQueryTimerResolution(&gGamePerformanceData.MinimumTimerResolution, &gGamePerformanceData.MaximumTimerResolution, &gGamePerformanceData.CurrentTimerResolution);

    GetSystemInfo(&gGamePerformanceData.SystemInfo);

    switch (gGamePerformanceData.SystemInfo.wProcessorArchitecture)
    {
        case PROCESSOR_ARCHITECTURE_INTEL:
        {
            LogMessageA(LL_INFO, "[%s] CPU Architecture: x86", __FUNCTION__);
            break;
        }
        case PROCESSOR_ARCHITECTURE_IA64:
        {
            LogMessageA(LL_INFO, "[%s] CPU Architecture: Itanium", __FUNCTION__);
            break;
        }
        case PROCESSOR_ARCHITECTURE_ARM64:
        {
            LogMessageA(LL_INFO, "[%s] CPU Architecture: ARM64", __FUNCTION__);
            break;
        }
        case PROCESSOR_ARCHITECTURE_ARM:
        {
            LogMessageA(LL_INFO, "[%s] CPU Architecture: ARM", __FUNCTION__);
            break;
        }
        case PROCESSOR_ARCHITECTURE_AMD64:
        {
            LogMessageA(LL_INFO, "[%s] CPU Architecture: x64", __FUNCTION__);
            break;
        }
        default:
        {
            LogMessageA(LL_INFO, "[%s] CPU Architecture: Unknown", __FUNCTION__);
        }
    }

    GetSystemTimeAsFileTime((FILETIME*)&gGamePerformanceData.PreviousSystemTime);

    if (timeBeginPeriod(1) == TIMERR_NOCANDO)
    {
        LogMessageA(LL_ERROR, "[%s] Failed to set global timer resolution!", __FUNCTION__);
        MessageBoxA(NULL, "Failed to set timer resolution!", "Error!", MB_ICONERROR | MB_OK);
        goto Exit;
    }

    if (SetPriorityClass(ProcessHandle, HIGH_PRIORITY_CLASS) == 0)
    {
        LogMessageA(LL_ERROR, "[%s] Failed to set process priority! Error 0x%08lx!", __FUNCTION__, GetLastError());
        MessageBoxA(NULL, "Failed to set process priority!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    if (SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST) == 0)
    {
        LogMessageA(LL_ERROR, "[%s] Failed to set thread priority! Error 0x%08lx!", __FUNCTION__, GetLastError());
        MessageBoxA(NULL, "Failed to set thread priority!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    if (CreateMainGameWindow() != ERROR_SUCCESS)
    {
        LogMessageA(LL_ERROR, "[%s] CreateMainGameWindow failed!", __FUNCTION__);
        goto Exit;
    }

    ////thread for loading assets
    if ((gAssetLoadingThreadHandle = CreateThread(NULL, 0, AssetLoadingThreadProc, NULL, 0, NULL)) == NULL)
    {
        MessageBoxA(NULL, "CreateThread failed!", "Error!", MB_ICONERROR | MB_OK);
        goto Exit;
    }

    if (InitializeSoundEngine() != S_OK)
    {
        MessageBoxA(NULL, "InitializeSoundEngine failed!", "Error!", MB_ICONERROR | MB_OK);
        goto Exit;
    }

    QueryPerformanceFrequency((LARGE_INTEGER*)&gGamePerformanceData.PerfFrequency);

    gBackBuffer.BitmapInfo.bmiHeader.biSize = sizeof(gBackBuffer.BitmapInfo.bmiHeader);
    gBackBuffer.BitmapInfo.bmiHeader.biWidth = GAME_RES_WIDTH;
    gBackBuffer.BitmapInfo.bmiHeader.biHeight = GAME_RES_HEIGHT;
    gBackBuffer.BitmapInfo.bmiHeader.biBitCount = GAME_BPP;
    gBackBuffer.BitmapInfo.bmiHeader.biCompression = BI_RGB;
    gBackBuffer.BitmapInfo.bmiHeader.biPlanes = 1;
    if ((gBackBuffer.Memory = VirtualAlloc(NULL, GAME_DRAWING_AREA_MEMORY_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)) == NULL)
    {
        LogMessageA(LL_ERROR, "[%s] Failed to allocate memory for backbuffer! Error 0x%08lx!", __FUNCTION__, GetLastError());
        MessageBoxA(NULL, "Failed to allocate memory for drawing surface!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    /*if (InitializeSprites() != ERROR_SUCCESS)
    {
        LogMessageA(LL_ERROR, "[%s] Failed to initialize sprites!", __FUNCTION__);
        MessageBoxA(NULL, "Failed to Initialize NPC sprites!", "Error!", MB_ICONERROR | MB_OK);
        goto Exit;
    }

    if (InitializePlayer() != ERROR_SUCCESS)
    {
        LogMessageA(LL_ERROR, "[%s] Failed to initialize player!", __FUNCTION__);
        MessageBoxA(NULL, "Failed to Initialize Player Sprite!", "Error!", MB_ICONERROR | MB_OK);
        goto Exit;
    }*/


    __stosd(gBackBuffer.Memory, 0xFF000000, GAME_DRAWING_AREA_MEMORY_SIZE / sizeof(DWORD));
    //memset(gBackBuffer.Memory, 0x00, GAME_DRAWING_AREA_MEMORY_SIZE);

    gGameIsRunning = TRUE;

    while (gGameIsRunning == TRUE) //basic message loop
    {
        QueryPerformanceCounter((LARGE_INTEGER*)&FrameStart);

        while (PeekMessageA(&Message, gGameWindow, 0, 0, PM_REMOVE))
        {
            DispatchMessageA(&Message);
        }
        ProcessPlayerInput();
        ProcessGameTickCalculation();
        RenderFrameGraphics();

        QueryPerformanceCounter((LARGE_INTEGER*)&FrameEnd);
        ElapsedMicroseconds = FrameEnd - FrameStart;  //calc elapsed microseconds during frame draw and player input
        ElapsedMicroseconds *= 1000000;
        ElapsedMicroseconds /= gGamePerformanceData.PerfFrequency;
        gGamePerformanceData.TotalFramesRendered++;               //increment count every frame
        ElapsedMicrosecondsPerFrameAccumulatorRaw += ElapsedMicroseconds;

        while (ElapsedMicroseconds < GOAL_MICROSECONDS_PER_FRAME)      //loop when overshooting framerate
        {
            ElapsedMicroseconds = FrameEnd - FrameStart;  //recalculate
            ElapsedMicroseconds *= 1000000;
            ElapsedMicroseconds /= gGamePerformanceData.PerfFrequency;
            QueryPerformanceCounter((LARGE_INTEGER*)&FrameEnd);

            if (ElapsedMicroseconds < (GOAL_MICROSECONDS_PER_FRAME * 0.75f))
            {
                Sleep(1);       //can be between 1ms and one system tick (~15.625ms)
            }
        }
        ElapsedMicrosecondsPerFrameAccumulatorCooked += ElapsedMicroseconds;

        if ((gGamePerformanceData.TotalFramesRendered % CALCULATE_AVG_FPS_EVERY_X_FRAMES) == 0)
        {
            GetSystemTimeAsFileTime((FILETIME*)&gGamePerformanceData.CurrentSystemTime);
            GetProcessTimes(ProcessHandle, &ProcessCreationTime, &ProcessExitTime, (FILETIME*)&CurrentKernelCPUTime, (FILETIME*)&CurrentUserCPUTime);

            gGamePerformanceData.CPUPercent = (double)(CurrentKernelCPUTime - PreviousKernelCPUTime) + (CurrentUserCPUTime - PreviousUserCPUTime);
            gGamePerformanceData.CPUPercent /= (gGamePerformanceData.CurrentSystemTime - gGamePerformanceData.PreviousSystemTime);
            gGamePerformanceData.CPUPercent /= gGamePerformanceData.SystemInfo.dwNumberOfProcessors; //kept returning 0 processors and then dividing by 0
            gGamePerformanceData.CPUPercent *= 100;

            GetProcessHandleCount(ProcessHandle, &gGamePerformanceData.HandleCount);
            K32GetProcessMemoryInfo(ProcessHandle, (PROCESS_MEMORY_COUNTERS*)&gGamePerformanceData.MemInfo, sizeof(gGamePerformanceData.MemInfo));

            gGamePerformanceData.RawFPSAverage = 1.0f / ((ElapsedMicrosecondsPerFrameAccumulatorRaw / CALCULATE_AVG_FPS_EVERY_X_FRAMES) * 0.000001f);
            gGamePerformanceData.CookedFPSAverage = 1.0f / ((ElapsedMicrosecondsPerFrameAccumulatorCooked / CALCULATE_AVG_FPS_EVERY_X_FRAMES) * 0.000001f);
            ElapsedMicrosecondsPerFrameAccumulatorRaw = 0;
            ElapsedMicrosecondsPerFrameAccumulatorCooked = 0;

            PreviousKernelCPUTime = CurrentKernelCPUTime;
            PreviousUserCPUTime = CurrentUserCPUTime;
            gGamePerformanceData.PreviousSystemTime = gGamePerformanceData.CurrentSystemTime;
        }
    }

Exit:
    return (0);
}


LRESULT CALLBACK MainWindowProc(
    _In_ HWND WindowHandle,        // handle to window
    _In_ UINT Message,        // message identifier
    _In_ WPARAM WParam,    // first message parameter
    _In_ LPARAM LParam)    // second message parameter
{
    LRESULT Result = 0;

    switch (Message)
    {
        case WM_CLOSE:      //close game stops EXE
        {   
            if (SaveRegistryParameters() != ERROR_SUCCESS)
            {
                LogMessageA(LL_ERROR, "[%s] Save Registry Parameters failed from Options>Back!", __FUNCTION__);
            }
            gGameIsRunning = FALSE;
            PostQuitMessage(0);
            break;
        }
        case WM_ACTIVATE:
        {
            if (WParam == 0)
            {
                gWindowHasFocus = FALSE;        //lost window focus (tabbed out)
            }
            else
            {
                //ShowCursor(FALSE);          //remove mouse cursor when mouse is over game surface
                gWindowHasFocus = TRUE;         //gained focus (tabbed in)
                break;
            }
        }
        default:
        {   Result = DefWindowProcA(WindowHandle, Message, WParam, LParam);
        }
    }
    return (Result);
}

DWORD CreateMainGameWindow(void)
{
    DWORD Result = ERROR_SUCCESS;

    WNDCLASSEXA WindowClass = { 0 };

    WindowClass.cbSize = sizeof(WNDCLASSEXA);
    WindowClass.style = CS_VREDRAW || CS_HREDRAW;
    WindowClass.lpfnWndProc = MainWindowProc;
    WindowClass.cbClsExtra = 0;
    WindowClass.cbWndExtra = 0;
    WindowClass.hInstance = GetModuleHandleA(NULL);
    WindowClass.hIcon = LoadIconA(NULL, IDI_APPLICATION);
    WindowClass.hIconSm = LoadIconA(NULL, IDI_APPLICATION);
    WindowClass.hCursor = LoadCursorA(NULL, IDC_ARROW);
#ifdef _DEBUG
    WindowClass.hbrBackground = CreateSolidBrush(RGB(255, 0, 255));
#else
    WindowClass.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));
#endif
    WindowClass.lpszMenuName = NULL;
    WindowClass.lpszClassName = GAME_NAME "_WINDOWCLASS";

    if (RegisterClassExA(&WindowClass) == 0)
    {
        Result = GetLastError();
        LogMessageA(LL_ERROR, "[%s] Window Registration Failed! RegisterClassExa Failed! Error 0x%08lx!", __FUNCTION__, Result);
        MessageBoxA(NULL, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    gGameWindow = CreateWindowExA(0, WindowClass.lpszClassName, "Window Title Name", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, NULL, NULL, GetModuleHandleA(NULL), NULL);

    if (gGameWindow == NULL)
    {
        Result = GetLastError();
        LogMessageA(LL_ERROR, "[%s] CreateWindowExA failed! Error 0x%08lx!", __FUNCTION__, Result);
        MessageBoxA(NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    gGamePerformanceData.MonitorInfo.cbSize = sizeof(MONITORINFO);

    if (GetMonitorInfoA(MonitorFromWindow(gGameWindow, MONITOR_DEFAULTTOPRIMARY), &gGamePerformanceData.MonitorInfo) == 0)
    {
        Result = ERROR_MONITOR_NO_DESCRIPTOR;       // GetMonitorInfoA cannot Result = GetLastError
        LogMessageA(LL_ERROR, "[%s] GetMonitorInfoA(MonitorFromWindow()) failed! Error 0x%08lx!", __FUNCTION__, Result);
        goto Exit;
    }

    for (uint8_t Counter = 1; Counter < 12; Counter++)
    {
        if (GAME_RES_WIDTH * Counter > (gGamePerformanceData.MonitorInfo.rcMonitor.right - gGamePerformanceData.MonitorInfo.rcMonitor.left) ||
            GAME_RES_HEIGHT * Counter > (gGamePerformanceData.MonitorInfo.rcMonitor.bottom - gGamePerformanceData.MonitorInfo.rcMonitor.top))
        {
            gGamePerformanceData.MaxScaleFactor = Counter - 1;
            break;
        }
    }

    if (gRegistryParams.ScaleFactor == 0)
    {
        gGamePerformanceData.CurrentScaleFactor = gGamePerformanceData.MaxScaleFactor;
    }
    else
    {
        gGamePerformanceData.CurrentScaleFactor = (uint8_t)gRegistryParams.ScaleFactor;
    }


    LogMessageA(LL_INFO, "[%s] Current screen scale factor is %d. Max scale factor is %d.", __FUNCTION__, gGamePerformanceData.CurrentScaleFactor, gGamePerformanceData.MaxScaleFactor);
    LogMessageA(LL_INFO, "[%s] Will draw at %dx%d", __FUNCTION__, gGamePerformanceData.CurrentScaleFactor * GAME_RES_WIDTH, gGamePerformanceData.CurrentScaleFactor * GAME_RES_HEIGHT);

    if (SetWindowLongPtrA(gGameWindow, GWL_STYLE, (WS_OVERLAPPEDWINDOW | WS_VISIBLE) & ~WS_OVERLAPPEDWINDOW) == 0)
    {
        Result = GetLastError();
        LogMessageA(LL_ERROR, "[%s] SetWindowLongPtrA failed! Error 0x%08lx!", __FUNCTION__, Result);
        goto Exit;
    }

    if (gRegistryParams.FullScreen == TRUE)     //fullscreen
    {
        if (SetWindowPos(gGameWindow,
            HWND_TOP,
            gGamePerformanceData.MonitorInfo.rcMonitor.left,
            gGamePerformanceData.MonitorInfo.rcMonitor.top,
            gGamePerformanceData.MonitorInfo.rcMonitor.right - gGamePerformanceData.MonitorInfo.rcMonitor.left,
            gGamePerformanceData.MonitorInfo.rcMonitor.bottom - gGamePerformanceData.MonitorInfo.rcMonitor.top,
            SWP_NOOWNERZORDER | SWP_FRAMECHANGED) == 0)
        {
            Result = GetLastError();
            LogMessageA(LL_ERROR, "[%s] SetWindowPos failed! Error 0x%08lx!", __FUNCTION__, Result);
            goto Exit;
        }
    }
    else                    //Windowed
    {
        if (SetWindowPos(gGameWindow,
            HWND_TOP,
            ((gGamePerformanceData.MonitorInfo.rcMonitor.right - gGamePerformanceData.MonitorInfo.rcMonitor.left) / 2) - (GAME_RES_WIDTH * gGamePerformanceData.CurrentScaleFactor / 2),
            ((gGamePerformanceData.MonitorInfo.rcMonitor.bottom - gGamePerformanceData.MonitorInfo.rcMonitor.top) / 2) - (GAME_RES_HEIGHT * gGamePerformanceData.CurrentScaleFactor / 2),
            //gGamePerformanceData.MonitorInfo.rcMonitor.left,
            //gGamePerformanceData.MonitorInfo.rcMonitor.top,
            GAME_RES_WIDTH * gGamePerformanceData.CurrentScaleFactor,
            GAME_RES_HEIGHT * gGamePerformanceData.CurrentScaleFactor,
            SWP_NOOWNERZORDER | SWP_FRAMECHANGED) == 0)
        {
            Result = GetLastError();
            LogMessageA(LL_ERROR, "[%s] SetWindowPos failed! Error 0x%08lx!", __FUNCTION__, Result);
            goto Exit;
        }
    }

Exit:
    return(Result);
}

BOOL GameIsAlreadyRunning(void)
{
    HANDLE Mutex = NULL;

    Mutex = CreateMutexA(NULL, FALSE, GAME_NAME "_Mutex");

    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        return(TRUE);
    }
    else
    {
        return(FALSE);
    }
}

void ProcessPlayerInput(void)
{
    gGameInput.EscapeKeyPressed = GetAsyncKeyState(VK_ESCAPE);
    gGameInput.DebugKeyPressed = GetAsyncKeyState(VK_F1);
    gGameInput.XKeyPressed = GetAsyncKeyState('X');
    gGameInput.HKeyPressed = GetAsyncKeyState('H');
    gGameInput.TabKeyPressed = GetAsyncKeyState(VK_TAB);


    gGameInput.SDownKeyPressed = GetAsyncKeyState('S') | GetAsyncKeyState(VK_DOWN);
    gGameInput.ALeftKeyPressed = GetAsyncKeyState('A') | GetAsyncKeyState(VK_LEFT);       // WASD and ArrowKey movement
    gGameInput.DRightKeyPressed = GetAsyncKeyState('D') | GetAsyncKeyState(VK_RIGHT);
    gGameInput.WUpKeyPressed = GetAsyncKeyState('W') | GetAsyncKeyState(VK_UP);
    gGameInput.DelKeyPressed = GetAsyncKeyState(VK_BACK) | GetAsyncKeyState(VK_DELETE);
    gGameInput.EKeyPressed = GetAsyncKeyState('E') | GetAsyncKeyState(VK_RETURN);

    if (gGameInput.DebugKeyPressed && !gGameInput.DebugKeyAlreadyPressed)
    {
        gGamePerformanceData.DisplayDebugInfo = !gGamePerformanceData.DisplayDebugInfo;
    }

    if (gGameInput.HKeyPressed && !gGameInput.HKeyAlreadyPressed)
    {
        gGamePerformanceData.DisplayControlsHelp = !gGamePerformanceData.DisplayControlsHelp;
    }

    ////

    if ((gInputEnabled == FALSE) || (gWindowHasFocus == FALSE))
    {
        goto InputDisabled;     //Jump over GAMESTATE INPUTS
    }

    ////GAMESTATE INPUTS

    switch (gCurrentGameState)
    {
        case GAMESTATE_SPLASHSCREEN:
        {
            PPI_SplashScreen();
            break;
        }
        case GAMESTATE_MAINMENU:
        {
            PPI_MainMenu();
            break;
        }
        case GAMESTATE_OPTIONS:
        {
            PPI_OptionsScreen();
            break;
        }
        case GAMESTATE_SAVELOAD:
        {
            break;
        }
        case GAMESTATE_PLAY:
        {
            break;
        }
        default:
        {
            ASSERT(FALSE, "gCurrentGameState was an unrecognized value in ProcessPlayerInput()");
            break;
        }
    }

    ////

    InputDisabled:

    gGameInput.DebugKeyAlreadyPressed = gGameInput.DebugKeyPressed;
    gGameInput.HKeyAlreadyPressed = gGameInput.HKeyPressed;
    gGameInput.ALeftKeyAlreadyPressed = gGameInput.ALeftKeyPressed;
    gGameInput.DRightKeyAlreadyPressed = gGameInput.DRightKeyPressed;
    gGameInput.WUpKeyAlreadyPressed = gGameInput.WUpKeyPressed;
    gGameInput.SDownKeyAlreadyPressed = gGameInput.SDownKeyPressed;
    gGameInput.XKeyAlreadyPressed = gGameInput.XKeyPressed;
    gGameInput.TabKeyAlreadyPressed = gGameInput.TabKeyPressed;
    gGameInput.QKeyAlreadyPressed = gGameInput.QKeyPressed;
    gGameInput.DelKeyAlreadyPressed = gGameInput.DelKeyPressed;
    gGameInput.EKeyAlreadyPressed = gGameInput.EKeyPressed;
    gGameInput.EscapeKeyAlreadyPressed = gGameInput.EscapeKeyPressed;
}

DWORD Load32BppBitmapFromFile(_In_ char* FileName, _Inout_ GAMEBITMAP* GameBitmap)
{
    DWORD Error = ERROR_SUCCESS;

    HANDLE FileHandle = INVALID_HANDLE_VALUE;

    WORD BitmapHeader = 0;

    DWORD PixelDataOffset = 0;

    DWORD NumberOfBytesRead = 2;

    if ((FileHandle = CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
    {
        Error = GetLastError();
        goto Exit;
    }

    if (ReadFile(FileHandle, &BitmapHeader, 2, &NumberOfBytesRead, NULL) == 0)
    {
        Error = GetLastError();
        goto Exit;
    }

    if (BitmapHeader != 0x4d42)     //0x4d42 is "BM" backwards
    {
        Error = ERROR_FILE_INVALID;
        goto Exit;
    }

    if (SetFilePointer(FileHandle, 0xA, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
    {
        Error = GetLastError();
        goto Exit;
    }

    if (ReadFile(FileHandle, &PixelDataOffset, sizeof(DWORD), &NumberOfBytesRead, NULL) == 0)
    {
        Error = GetLastError();
        goto Exit;
    }

    if (SetFilePointer(FileHandle, 0xE, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
    {
        Error = GetLastError();
        goto Exit;
    }

    if (ReadFile(FileHandle, &GameBitmap->BitmapInfo.bmiHeader, sizeof(BITMAPINFOHEADER), &NumberOfBytesRead, NULL) == 0)
    {
        Error = GetLastError();
        goto Exit;
    }

    if ((GameBitmap->Memory = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, GameBitmap->BitmapInfo.bmiHeader.biSizeImage)) == NULL)
    {
        Error = ERROR_NOT_ENOUGH_MEMORY;
        goto Exit;
    }

    if (SetFilePointer(FileHandle, PixelDataOffset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
    {
        Error = GetLastError();
        goto Exit;
    }

    if (ReadFile(FileHandle, GameBitmap->Memory, GameBitmap->BitmapInfo.bmiHeader.biSizeImage, &NumberOfBytesRead, NULL) == 0)
    {
        Error = GetLastError();
        goto Exit;
    }

Exit:
    if (FileHandle && (FileHandle != INVALID_HANDLE_VALUE))
    {
        CloseHandle(FileHandle);
    }
    return(Error);
}

void BlitStringToBuffer(_In_ char* String, _In_ GAMEBITMAP* FontSheet, _In_ PIXEL32* Color, _In_ uint16_t x, _In_ uint16_t y)
{

    // Map any char value to an offset dictated by the g6x7Font ordering.
    static int FontCharacterPixelOffset[] = {
        //  .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. ..
            93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,
            //     !  "  #  $  %  &  '  (  )  *  +  ,  -  .  /  0  1  2  3  4  5  6  7  8  9  :  ;  <  =  >  ?
                94,64,87,66,67,68,70,85,72,73,71,77,88,74,91,92,52,53,54,55,56,57,58,59,60,61,86,84,89,75,90,93,
                //  @  A  B  C  D  E  F  G  H  I  J  K  L  M  N  O  P  Q  R  S  T  U  V  W  X  Y  Z  [  \  ]  ^  _
                    65,0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,80,78,81,69,76,
                    //  `  a  b  c  d  e  f  g  h  i  j  k  l  m  n  o  p  q  r  s  t  u  v  w  x  y  z  {  |  }  ~  ..
                        62,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,82,79,83,63,93,
                        //  .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. ..
                            93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,
                            //  .. .. .. .. .. .. .. .. .. .. .. �  .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. �  .. .. .. ..
                                93,93,93,93,93,93,93,93,93,93,93,96,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,95,93,93,93,93,
                                //  .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. ..
                                    93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,
                                    //  .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. F2 .. .. .. .. .. .. .. .. .. .. .. .. ..
                                        93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,97,93,93,93,93,93,93,93,93,93,93,93,93,93
    };


    uint16_t CharWidth = (uint16_t)FontSheet->BitmapInfo.bmiHeader.biWidth / FONT_SHEET_CHARACTERS_PER_ROW;
    uint16_t CharHeight = (uint16_t)FontSheet->BitmapInfo.bmiHeader.biHeight;      // only one row
    uint16_t BytesPerCharacter = (CharWidth * CharHeight * (FontSheet->BitmapInfo.bmiHeader.biBitCount / 8));
    uint16_t StringLength = (uint16_t)strlen(String);

    GAMEBITMAP StringBitmap = { 0 };

    StringBitmap.BitmapInfo.bmiHeader.biBitCount = GAME_BPP;
    StringBitmap.BitmapInfo.bmiHeader.biHeight = CharHeight;
    StringBitmap.BitmapInfo.bmiHeader.biWidth = CharWidth * StringLength;
    StringBitmap.BitmapInfo.bmiHeader.biPlanes = 1;
    StringBitmap.BitmapInfo.bmiHeader.biCompression = BI_RGB;
    StringBitmap.Memory = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ((size_t)BytesPerCharacter * (size_t)StringLength));

    for (int Character = 0; Character < StringLength; Character++)
    {
        int StartingFontSheetPixel = 0;
        int FontSheetOffset = 0;
        int StringBitmapOffset = 0;

        PIXEL32 FontSheetPixel = { 0 };

        StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * FontCharacterPixelOffset[(uint8_t)String[Character]]);

        for (int YPixel = 0; YPixel < CharHeight; YPixel++)
        {
            for (int XPixel = 0; XPixel < CharWidth; XPixel++)
            {
                FontSheetOffset = StartingFontSheetPixel + XPixel - (FontSheet->BitmapInfo.bmiHeader.biWidth * YPixel);

                StringBitmapOffset = (Character * CharWidth) + ((StringBitmap.BitmapInfo.bmiHeader.biWidth * StringBitmap.BitmapInfo.bmiHeader.biHeight) - StringBitmap.BitmapInfo.bmiHeader.biWidth) + XPixel - (StringBitmap.BitmapInfo.bmiHeader.biWidth * YPixel);

                memcpy_s(&FontSheetPixel, sizeof(PIXEL32), (PIXEL32*)FontSheet->Memory + FontSheetOffset, sizeof(PIXEL32));

                FontSheetPixel.Colors.Red = Color->Colors.Red;
                FontSheetPixel.Colors.Green = Color->Colors.Green;
                FontSheetPixel.Colors.Blue = Color->Colors.Blue;

                memcpy_s((PIXEL32*)StringBitmap.Memory + StringBitmapOffset, sizeof(PIXEL32*), &FontSheetPixel, sizeof(PIXEL32));
            }

        }

    }

    Blit32BppBitmapToBuffer(&StringBitmap, x, y, 0); //TODO: make brightness usefull here

    if (StringBitmap.Memory)        //remember to free memory
    {
        HeapFree(GetProcessHeap(), 0, StringBitmap.Memory);
    }
}

void RenderFrameGraphics(void)
{

#ifdef SIMD

    //fill the screen with blue
    __m128i QuadPixel = { 0x3f, 0x00, 0x00, 0xff, 0x3f, 0x00, 0x00, 0xff, 0x3f, 0x00, 0x00, 0xff, 0x3f, 0x00, 0x00, 0xff };     //load 4 pixels worth of info
    ClearScreenColor(&QuadPixel);

#else
    //PIXEL32 Pixel = { 0xff, 0x00, 0x00, 0xff };         //load 1 pixel
    //ClearScreenColor(&Pixel);
#endif


    //Blit32BppBitmapToBuffer(&gBackGroundGraphic, 0, 0, 0);         ////background grapic image
    

    switch (gCurrentGameState)
    {
        case GAMESTATE_SPLASHSCREEN:
        {
            DrawSplashScreen();
            break;
        }
        case GAMESTATE_MAINMENU:
        {
            DrawMainMenu();
            break;
        }
        case GAMESTATE_OPTIONS:
        {
            DrawOptionsScreen();
            break;
        }
        case GAMESTATE_SAVELOAD:
        {
            break;
        }
        case GAMESTATE_PLAY:
        {
            break;
        }
        default:
        {
            //ASSERT(FALSE, "gCurrentGameState was an unrecognized value in RenderFrameGraphics()");
            break;
        }
    }

    if (gGamePerformanceData.DisplayDebugInfo)
    {
        DrawDebugInfo();
    }

    if (gGamePerformanceData.DisplayControlsHelp)
    {

        DrawWindow(40, 165, 300, 51, &COLOR_NES_WHITE, &COLOR_BLACK, NULL, WINDOW_FLAG_BORDERED | WINDOW_FLAG_OPAQUE | WINDOW_FLAG_HORIZ_CENTERED);

        BlitStringToBuffer("Press F1 for debug, close this menu with H.", &g4x5Font, &COLOR_NES_PINK, 48, 170);
    }

    if (gRegistryParams.FullScreen)
    {
        HDC DeviceContext = GetDC(gGameWindow);
        StretchDIBits(DeviceContext,
            ((gGamePerformanceData.MonitorInfo.rcMonitor.right - gGamePerformanceData.MonitorInfo.rcMonitor.left) / 2) - (GAME_RES_WIDTH * gGamePerformanceData.CurrentScaleFactor / 2),
            ((gGamePerformanceData.MonitorInfo.rcMonitor.bottom - gGamePerformanceData.MonitorInfo.rcMonitor.top) / 2) - (GAME_RES_HEIGHT * gGamePerformanceData.CurrentScaleFactor / 2),
            GAME_RES_WIDTH * gGamePerformanceData.CurrentScaleFactor,
            GAME_RES_HEIGHT * gGamePerformanceData.CurrentScaleFactor,
            0,
            0,
            GAME_RES_WIDTH,
            GAME_RES_HEIGHT,
            gBackBuffer.Memory,
            &gBackBuffer.BitmapInfo,
            DIB_RGB_COLORS,
            SRCCOPY);

        ReleaseDC(gGameWindow, DeviceContext);
    }
    else
    {
        HDC DeviceContext = GetDC(gGameWindow);
        StretchDIBits(DeviceContext,
            0,
            0,
            GAME_RES_WIDTH * gGamePerformanceData.CurrentScaleFactor,
            GAME_RES_HEIGHT * gGamePerformanceData.CurrentScaleFactor,
            0,
            0,
            GAME_RES_WIDTH,
            GAME_RES_HEIGHT,
            gBackBuffer.Memory,
            &gBackBuffer.BitmapInfo,
            DIB_RGB_COLORS,
            SRCCOPY);

        ReleaseDC(gGameWindow, DeviceContext);
    }

    
    //this allows me to create a background image and stretch its bits to fit the changing screen size

    /*RECT WindowRect = { 0 };

    GetClientRect(gGameWindow, &WindowRect);
    MapWindowPoints(gGameWindow, GetParent(gGameWindow), (LPPOINT)&WindowRect, 2);

    HDC DeviceContext = GetDC(gGameWindow);
    StretchDIBits(DeviceContext,
        0,
        0,
        WindowRect.right - WindowRect.left,
        WindowRect.bottom - WindowRect.top,
        0,
        0,
        GAME_RES_WIDTH,
        GAME_RES_HEIGHT,
        gBackBuffer.Memory,
        &gBackBuffer.BitmapInfo,
        DIB_RGB_COLORS,
        SRCCOPY);

    ReleaseDC(gGameWindow, DeviceContext);*/

    ////
}

#ifdef SIMD
__forceinline void ClearScreenColor(_In_ __m128i* Color)
{
    for (int x = 0; x < GAME_RES_WIDTH * GAME_RES_HEIGHT; x += 4)      //paint the screen 4 pixels at a time
    {
        _mm_store_si128((PIXEL32*)gBackBuffer.Memory + x, *Color);
    }
}
#else
__forceinline void ClearScreenColor(_In_ PIXEL32* Pixel)
{
    for (int x = 0; x < GAME_RES_WIDTH * GAME_RES_HEIGHT; x++)      //paint the screen 1 pixel at a time
    {
        memcpy((PIXEL32*)gBackBuffer.Memory + x, Pixel, sizeof(PIXEL32));
    }
}
#endif


void Blit32BppBitmapToBuffer(_In_ GAMEBITMAP* GameBitmap, _In_ int16_t x, _In_ int16_t y, _In_ int16_t BrightnessAdjustment)
{
    int32_t StartingScreenPixel = ((GAME_RES_HEIGHT * GAME_RES_WIDTH) - GAME_RES_WIDTH) - (GAME_RES_WIDTH * y) + x;
    int32_t StartingBitmapPixel = ((GameBitmap->BitmapInfo.bmiHeader.biWidth * GameBitmap->BitmapInfo.bmiHeader.biHeight) - GameBitmap->BitmapInfo.bmiHeader.biWidth);
    int32_t MemoryOffset = 0;
    int32_t BitmapOffset = 0;
    PIXEL32 BitmapPixel = { 0 };

#ifdef AVX

    __m256i BitmapOctoPixel;

    for (int16_t YPixel = 0; YPixel < GameBitmap->BitmapInfo.bmiHeader.biHeight; YPixel++)
    {
        int16_t PixelsRemainingOnThisRow = GameBitmap->BitmapInfo.bmiHeader.biWidth;

        int16_t XPixel = 0;
        while (PixelsRemainingOnThisRow >= 8)
        {

            MemoryOffset = StartingScreenPixel + XPixel - (GAME_RES_WIDTH * YPixel);

            BitmapOffset = StartingBitmapPixel + XPixel - (GameBitmap->BitmapInfo.bmiHeader.biWidth * YPixel);


            BitmapOctoPixel = _mm256_load_si256((const __m256i*)((PIXEL32*)GameBitmap->Memory + BitmapOffset));


            __m256i Half1 = _mm256_cvtepu8_epi16(_mm256_extracti128_si256(BitmapOctoPixel, 0));


            // Add the brightness adjustment to each 16-bit element, except alpha.
            Half1 = _mm256_add_epi16(Half1, _mm256_set_epi16(
                0, BrightnessAdjustment, BrightnessAdjustment, BrightnessAdjustment,
                0, BrightnessAdjustment, BrightnessAdjustment, BrightnessAdjustment,
                0, BrightnessAdjustment, BrightnessAdjustment, BrightnessAdjustment,
                0, BrightnessAdjustment, BrightnessAdjustment, BrightnessAdjustment));

            __m256i Half2 = _mm256_cvtepu8_epi16(_mm256_extracti128_si256(BitmapOctoPixel, 1));

            Half2 = _mm256_add_epi16(Half2, _mm256_set_epi16(
                0, BrightnessAdjustment, BrightnessAdjustment, BrightnessAdjustment,
                0, BrightnessAdjustment, BrightnessAdjustment, BrightnessAdjustment,
                0, BrightnessAdjustment, BrightnessAdjustment, BrightnessAdjustment,
                0, BrightnessAdjustment, BrightnessAdjustment, BrightnessAdjustment));

            // Now we need to reassemble the two halves back into a single 256-bit group of 8 pixels.
            // _mm256_packus_epi16(a,b) takes the 16-bit signed integers in the 256-bit vectors a and b
            // and converts them to a 256-bit vector of 8-bit unsigned integers. The result contains the
            // first 8 integers from a, followed by the first 8 integers from b, followed by the last 8
            // integers from a, followed by the last 8 integers from b.
            // Values that are out of range are set to 0 or 255.
            __m256i Recombined = _mm256_packus_epi16(Half1, Half2);

            BitmapOctoPixel = _mm256_permute4x64_epi64(Recombined, _MM_SHUFFLE(3, 1, 2, 0));

            // Create a mask that selects only the pixels that have an Alpha == 255.
            __m256i Mask = _mm256_cmpeq_epi8(BitmapOctoPixel, _mm256_set1_epi8(-1));

            // Conditionally store the result to the global back buffer, based on the mask
            // we just created that selects only the pixels where Alpha == 255.
            _mm256_maskstore_epi32((int*)((PIXEL32*)gBackBuffer.Memory + MemoryOffset), Mask, BitmapOctoPixel);

            PixelsRemainingOnThisRow -= 8;

            XPixel += 8;
        }
        while (PixelsRemainingOnThisRow > 0)
        {
            MemoryOffset = StartingScreenPixel + XPixel - (GAME_RES_WIDTH * YPixel);

            BitmapOffset = StartingBitmapPixel + XPixel - (GameBitmap->BitmapInfo.bmiHeader.biWidth * YPixel);

            memcpy_s(&BitmapPixel, sizeof(PIXEL32), (PIXEL32*)GameBitmap->Memory + BitmapOffset, sizeof(PIXEL32));     //copy contents of bitmap pixel

            if (BitmapPixel.Colors.Alpha > 0)      ////not alpha blending, only drawing non 0 alpha pixels TOUSE: alphablending possibly?? 
            {
                BitmapPixel.Colors.Red = min(255, max((BitmapPixel.Colors.Red + BrightnessAdjustment), 0));
                BitmapPixel.Colors.Green = min(255, max((BitmapPixel.Colors.Green + BrightnessAdjustment), 0));
                BitmapPixel.Colors.Blue = min(255, max((BitmapPixel.Colors.Blue + BrightnessAdjustment), 0));

                memcpy_s((PIXEL32*)gBackBuffer.Memory + MemoryOffset, sizeof(PIXEL32), &BitmapPixel, sizeof(PIXEL32));     //place contents of bitmap pixel onto backbuffer
            }
            PixelsRemainingOnThisRow--;
            XPixel++;
        }
    }

#elif defined SSE2

    //TODO:

#else

    for (int16_t YPixel = 0; YPixel < GameBitmap->BitmapInfo.bmiHeader.biHeight; YPixel++)
    {
        for (int16_t XPixel = 0; XPixel < GameBitmap->BitmapInfo.bmiHeader.biWidth; XPixel++)
        {
            ////preventing pixels being drawn outside screen
            //if ((x < 1) || (x < GAME_RES_WIDTH - GameBitmap->BitmapInfo.bmiHeader.biWidth) || (y < 1) || (y < GAME_RES_HEIGHT - GameBitmap->BitmapInfo.bmiHeader.biHeight))         //////TODO: please optimize this way too many branches
            //{
            //    if (x < 1)
            //    {
            //        if (XPixel < -x)
            //        {
            //            break;
            //        }
            //    }
            //    else if (x > GAME_RES_WIDTH - GameBitmap->BitmapInfo.bmiHeader.biWidth)
            //    {
            //        if (XPixel > GAME_RES_WIDTH - x - 1)
            //        {
            //            break;
            //        }
            //    }

            //    if ( y < 1)
            //    {
            //        if (YPixel < -y)
            //        {
            //            break;
            //        }
            //    }
            //    else if (y > GAME_RES_HEIGHT - GameBitmap->BitmapInfo.bmiHeader.biHeight)
            //    {
            //        if (YPixel > GAME_RES_HEIGHT - y - 1)
            //        {
            //            break;
            //        }
            //    }
            //}
            MemoryOffset = StartingScreenPixel + XPixel - (GAME_RES_WIDTH * YPixel);

            BitmapOffset = StartingBitmapPixel + XPixel - (GameBitmap->BitmapInfo.bmiHeader.biWidth * YPixel);

            memcpy_s(&BitmapPixel, sizeof(PIXEL32), (PIXEL32*)GameBitmap->Memory + BitmapOffset, sizeof(PIXEL32));     //copy contents of bitmap pixel

            if (BitmapPixel.Alpha > 0)
            {
                BitmapPixel.Red = min(255, max((BitmapPixel.Red + BrightnessAdjustment), 0));
                BitmapPixel.Green = min(255, max((BitmapPixel.Green + BrightnessAdjustment), 0));
                BitmapPixel.Blue = min(255, max((BitmapPixel.Blue + BrightnessAdjustment), 0));

                memcpy_s((PIXEL32*)gBackBuffer.Memory + MemoryOffset, sizeof(PIXEL32), &BitmapPixel, sizeof(PIXEL32));     //place contents of bitmap pixel onto backbuffer
            }
        }
    }

#endif
}

void BlitBackgroundToBuffer(_In_ GAMEBITMAP* GameBitmap, _In_ int16_t BrightnessAdjustment)
{
    int32_t StartingScreenPixel = ((GAME_RES_HEIGHT * GAME_RES_WIDTH) - GAME_RES_WIDTH);
    int32_t StartingBitmapPixel = ((GameBitmap->BitmapInfo.bmiHeader.biWidth * GameBitmap->BitmapInfo.bmiHeader.biHeight) - GameBitmap->BitmapInfo.bmiHeader.biWidth) /*+ gCamera.x*/ - (GameBitmap->BitmapInfo.bmiHeader.biWidth /** gCamera.y*/);
    int32_t MemoryOffset = 0;
    int32_t BitmapOffset = 0;

#ifdef AVX
    __m256i BitmapOctoPixel;

    for (int16_t YPixel = 0; YPixel < GAME_RES_HEIGHT; YPixel++)
    {
        for (int16_t XPixel = 0; XPixel < GAME_RES_WIDTH; XPixel += 8)
        {
            MemoryOffset = StartingScreenPixel + XPixel - (GAME_RES_WIDTH * YPixel);

            BitmapOffset = StartingBitmapPixel + XPixel - (GameBitmap->BitmapInfo.bmiHeader.biWidth * YPixel);

            BitmapOctoPixel = _mm256_loadu_si256((const __m256i*)((PIXEL32*)GameBitmap->Memory + BitmapOffset));


            //        AARRGGBBAARRGGBB-AARRGGBBAARRGGBB-AARRGGBBAARRGGBB-AARRGGBBAARRGGBB
            // YMM0 = FF5B6EE1FF5B6EE1-FF5B6EE1FF5B6EE1-FF5B6EE1FF5B6EE1-FF5B6EE1FF5B6EE1

            __m256i Half1 = _mm256_cvtepu8_epi16(_mm256_extracti128_si256(BitmapOctoPixel, 0));

            //        AAAARRRRGGGGBBBB-AAAARRRRGGGGBBBB-AAAARRRRGGGGBBBB-AAAARRRRGGGGBBBB
            // YMM0 = 00FF005B006E00E1-00FF005B006E00E1-00FF005B006E00E1-00FF005B006E00E1

            Half1 = _mm256_add_epi16(Half1, _mm256_set1_epi16(BrightnessAdjustment));

            //        AAAARRRRGGGGBBBB-AAAARRRRGGGGBBBB-AAAARRRRGGGGBBBB-AAAARRRRGGGGBBBB        
            // YMM0 = 0000FF5Cff6FFFE2-0000FF5Cff6FFFE2-0000FF5Cff6FFFE2-0000FF5Cff6FFFE2

            //      take apart 256 bits into 2x 256 bits (half1&2), so we can do math with each pixel conataining 16bits of info (intead of 8bits) and brightness (which is 16bits)
            //

            __m256i Half2 = _mm256_cvtepu8_epi16(_mm256_extracti128_si256(BitmapOctoPixel, 1));

            //  
            //

            Half2 = _mm256_add_epi16(Half2, _mm256_set1_epi16(BrightnessAdjustment));

            //
            //

            __m256i Recombined = _mm256_packus_epi16(Half1, Half2);

            // packus doesnt retain order very well, so we need to shuffle
            // packus also clamps value between 255 and 0 so we dont have to

            BitmapOctoPixel = _mm256_permute4x64_epi64(Recombined, _MM_SHUFFLE(3, 1, 2, 0));

            _mm256_store_si256((__m256i*)((PIXEL32*)gBackBuffer.Memory + MemoryOffset), BitmapOctoPixel);
        }
    }

#elif defined SSE2    
    __m128i BitmapQuadPixel;

    for (int16_t YPixel = 0; YPixel < GAME_RES_HEIGHT; YPixel++)
    {
        for (int16_t XPixel = 0; XPixel < GAME_RES_WIDTH; XPixel += 4)
        {
            MemoryOffset = StartingScreenPixel + XPixel - (GAME_RES_WIDTH * YPixel);

            BitmapOffset = StartingBitmapPixel + XPixel - (GameBitmap->BitmapInfo.bmiHeader.biWidth * YPixel);

            BitmapQuadPixel = _mm_load_si128((PIXEL32*)GameBitmap->Memory + BitmapOffset);

            _mm_store_si128((PIXEL32*)gBackBuffer.Memory + MemoryOffset, BitmapQuadPixel);
        }
    }


#else
    PIXEL32 BitmapPixel = { 0 };
    for (int16_t YPixel = 0; YPixel < GAME_RES_HEIGHT; YPixel++)
    {
        for (int16_t XPixel = 0; XPixel < GAME_RES_WIDTH; XPixel++)
        {
            MemoryOffset = StartingScreenPixel + XPixel - (GAME_RES_WIDTH * YPixel);

            BitmapOffset = StartingBitmapPixel + XPixel - (GameBitmap->BitmapInfo.bmiHeader.biWidth * YPixel);

            memcpy_s(&BitmapPixel, sizeof(PIXEL32), (PIXEL32*)GameBitmap->Memory + BitmapOffset, sizeof(PIXEL32));     //copy contents of bitmap pixel

            BitmapPixel.Red = (uint8_t)min(255, max((BitmapPixel.Red + BrightnessAdjustment), 0));

            BitmapPixel.Blue = (uint8_t)min(255, max((BitmapPixel.Blue + BrightnessAdjustment), 0));

            BitmapPixel.Green = (uint8_t)min(255, max((BitmapPixel.Green + BrightnessAdjustment), 0));

            memcpy_s((PIXEL32*)gBackBuffer.Memory + MemoryOffset, sizeof(PIXEL32), &BitmapPixel, sizeof(PIXEL32));     //place contents of bitmap pixel onto backbuffer
        }
    }
#endif

}

DWORD LoadRegistryParameters(void)
{
    DWORD Result = ERROR_SUCCESS;

    HKEY RegKey = NULL;

    DWORD RegDisposition = 0;

    DWORD RegBytesRead = sizeof(DWORD);

    Result = RegCreateKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\" GAME_NAME, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &RegKey, &RegDisposition);

    if (Result != ERROR_SUCCESS)
    {
        LogMessageA(LL_ERROR, "[%s] RegCreateKey failed with error code 0x%08lx!", __FUNCTION__, Result);

        goto Exit;
    }

    if (RegDisposition == REG_CREATED_NEW_KEY)
    {
        LogMessageA(LL_INFO, "[%s] Registry key did not exist; created new key HKCU\\SOFTWARE\\%s.", __FUNCTION__, GAME_NAME);
    }
    else
    {
        LogMessageA(LL_INFO, "[%s] Opened existing registry key HCKU\\SOFTWARE\\%s", __FUNCTION__, GAME_NAME);
    }



    Result = RegGetValueA(RegKey, NULL, "LogLevel", RRF_RT_DWORD, NULL, (BYTE*)&gRegistryParams.LogLevel, &RegBytesRead);

    if (Result != ERROR_SUCCESS)
    {
        if (Result == ERROR_FILE_NOT_FOUND)
        {
            Result = ERROR_SUCCESS;
            LogMessageA(LL_INFO, "[%s] Registry value 'LogLevel' not found. Using default of 0. (LOG_LEVEL_NONE)", __FUNCTION__);
            gRegistryParams.LogLevel = LL_NONE;
        }
        else
        {
            LogMessageA(LL_ERROR, "[%s] Failed to read the 'LogLevel' registry value! Error 0x%08lx!", __FUNCTION__, Result);
            goto Exit;
        }
    }

    LogMessageA(LL_INFO, "[%s] LogLevel is %d", __FUNCTION__, gRegistryParams.LogLevel);



    Result = RegGetValueA(RegKey, NULL, "ScaleFactor", RRF_RT_DWORD, NULL, (BYTE*)&gRegistryParams.ScaleFactor, &RegBytesRead);

    if (Result != ERROR_SUCCESS)
    {
        if (Result == ERROR_FILE_NOT_FOUND)
        {
            Result = ERROR_SUCCESS;
            LogMessageA(LL_INFO, "[%s] Registry value 'ScaleFactor' not found. Using default of 0.", __FUNCTION__);
            gRegistryParams.ScaleFactor = 0;
        }
        else
        {
            LogMessageA(LL_ERROR, "[%s] Failed to read the 'ScaleFactor' registry value! Error 0x%08lx!", __FUNCTION__, Result);
            goto Exit;
        }
    }
    LogMessageA(LL_INFO, "[%s] ScaleFactor is %d", __FUNCTION__, gRegistryParams.ScaleFactor);



    Result = RegGetValueA(RegKey, NULL, "SFXVolume", RRF_RT_DWORD, NULL, &gRegistryParams.SFXVolume, &RegBytesRead);

    if (Result != ERROR_SUCCESS)
    {
        if (Result == ERROR_FILE_NOT_FOUND)
        {
            Result = ERROR_SUCCESS;
            LogMessageA(LL_INFO, "[%s] Registry value 'SFXVolume' not found. Using default of 0.5.", __FUNCTION__);
            gRegistryParams.SFXVolume = 50;
        }
        else
        {
            LogMessageA(LL_ERROR, "[%s] Failed to read the 'SFXVolume' registry value! Error 0x%08lx!", __FUNCTION__, Result);
            goto Exit;
        }
    }

    gSFXVolume = gRegistryParams.SFXVolume;
    LogMessageA(LL_INFO, "[%s] SFXVolume is %d", __FUNCTION__, gSFXVolume);



    Result = RegGetValueA(RegKey, NULL, "MusicVolume", RRF_RT_DWORD, NULL, &gRegistryParams.MusicVolume, &RegBytesRead);

    if (Result != ERROR_SUCCESS)
    {
        if (Result == ERROR_FILE_NOT_FOUND)
        {
            Result = ERROR_SUCCESS;
            LogMessageA(LL_INFO, "[%s] Registry value 'MusicVolume' not found. Using default of 0.5.", __FUNCTION__);
            gRegistryParams.MusicVolume = 50;
        }
        else
        {
            LogMessageA(LL_ERROR, "[%s] Failed to read the 'MusicVolume' registry value! Error 0x%08lx!", __FUNCTION__, Result);
            goto Exit;
        }
    }

    gMusicVolume = gRegistryParams.MusicVolume;
    LogMessageA(LL_INFO, "[%s] MusicVolume is %d", __FUNCTION__, gMusicVolume);



    Result = RegGetValueA(RegKey, NULL, "FullScreen", RRF_RT_DWORD, NULL, (BYTE*)&gRegistryParams.FullScreen, &RegBytesRead);

    if (Result != ERROR_SUCCESS)
    {
        if (Result == ERROR_FILE_NOT_FOUND)
        {
            Result = ERROR_SUCCESS;
            LogMessageA(LL_INFO, "[%s] Registry value 'FullScreen' not found. Using default of FALSE.", __FUNCTION__);
            gRegistryParams.FullScreen = 0;
        }
        else
        {
            LogMessageA(LL_ERROR, "[%s] Failed to read the 'FullScreen' registry value! Error 0x%08lx!", __FUNCTION__, Result);
            goto Exit;
        }
    }
    LogMessageA(LL_INFO, "[%s] FullScreen is %d", __FUNCTION__, gRegistryParams.FullScreen);



    Result = RegGetValueA(RegKey, NULL, "TextSpeed", RRF_RT_DWORD, NULL, (BYTE*)&gRegistryParams.TextSpeed, &RegBytesRead);

    if (Result != ERROR_SUCCESS)
    {
        if (Result == ERROR_FILE_NOT_FOUND)
        {
            Result = ERROR_SUCCESS;
            LogMessageA(LL_INFO, "[%s] Registry value 'TextSpeed' not found. Using default of 2 (Medium).", __FUNCTION__);
            gRegistryParams.TextSpeed = 2;
        }
        else
        {
            LogMessageA(LL_ERROR, "[%s] Failed to read the 'TextSpeed' registry value! Error 0x%08lx!", __FUNCTION__, Result);
            goto Exit;
        }
    }
    LogMessageA(LL_INFO, "[%s] TextSpeed is %d", __FUNCTION__, gRegistryParams.TextSpeed);


    //...

Exit:
    if (RegKey)
    {
        RegCloseKey(RegKey);
    }
    return(Result);
}


DWORD SaveRegistryParameters(void)
{

    DWORD Result = ERROR_SUCCESS;

    HKEY RegKey = NULL;

    DWORD RegDisposition = 0;

    Result = RegCreateKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\" GAME_NAME, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &RegKey, &RegDisposition);

    if (Result != ERROR_SUCCESS)
    {
        LogMessageA(LL_ERROR, "[%s] RegCreateKey failed with error code 0x%08lx!", __FUNCTION__, Result);
        goto Exit;
    }



    Result = RegSetValueExA(RegKey, "SFXVolume", 0, REG_DWORD, (const BYTE*)&gSFXVolume, sizeof(DWORD));

    if (Result != ERROR_SUCCESS)
    {
        LogMessageA(LL_ERROR, "[%s] Failed to set 'SFXVolume' in registry! Error 0x%08lx!", __FUNCTION__, Result);
        goto Exit;
    }
    LogMessageA(LL_INFO, "[%s] Saved 'SFXVolume' in registry: %d. ", __FUNCTION__, gSFXVolume);



    Result = RegSetValueExA(RegKey, "MusicVolume", 0, REG_DWORD, (const BYTE*)&gMusicVolume, sizeof(DWORD));

    if (Result != ERROR_SUCCESS)
    {
        LogMessageA(LL_ERROR, "[%s] Failed to set 'MusicVolume' in registry! Error 0x%08lx!", __FUNCTION__, Result);
        goto Exit;
    }
    LogMessageA(LL_INFO, "[%s] Saved 'MusicVolume' in registry: %d. ", __FUNCTION__, gMusicVolume);



    Result = RegSetValueExA(RegKey, "ScaleFactor", 0, REG_DWORD, (const BYTE*)&gGamePerformanceData.CurrentScaleFactor, sizeof(DWORD));

    if (Result != ERROR_SUCCESS)
    {
        LogMessageA(LL_ERROR, "[%s] Failed to set 'ScaleFactor' in registry! Error 0x%08lx!", __FUNCTION__, Result);
        goto Exit;
    }
    LogMessageA(LL_INFO, "[%s] Saved 'ScaleFactor' in registry: %d. ", __FUNCTION__, gGamePerformanceData.CurrentScaleFactor);



    Result = RegSetValueExA(RegKey, "FullScreen", 0, REG_DWORD, (const BYTE*)&gRegistryParams.FullScreen, sizeof(BOOL));

    if (Result != ERROR_SUCCESS)
    {
        LogMessageA(LL_ERROR, "[%s] Failed to set 'FullScreen' in registry! Error 0x%08lx!", __FUNCTION__, Result);
        goto Exit;
    }
    LogMessageA(LL_INFO, "[%s] Saved 'FullScreen' in registry: %d. ", __FUNCTION__, gRegistryParams.FullScreen);



    Result = RegSetValueExA(RegKey, "TextSpeed", 0, REG_DWORD, (const BYTE*)&gRegistryParams.TextSpeed, sizeof(DWORD));

    if (Result != ERROR_SUCCESS)
    {
        LogMessageA(LL_ERROR, "[%s] Failed to set 'TextSpeed' in registry! Error 0x%08lx!", __FUNCTION__, Result);
        goto Exit;
    }
    LogMessageA(LL_INFO, "[%s] Saved 'TextSpeed' in registry: %d. ", __FUNCTION__, gRegistryParams.TextSpeed);

Exit:
    if (RegKey)
    {
        RegCloseKey(RegKey);
    }
    return(Result);
}

void LogMessageA(_In_ DWORD LogLevel, _In_ char* Message, _In_ ...)
{
    size_t MessageLength = strlen(Message);

    SYSTEMTIME Time = { 0 };

    HANDLE LogFileHandle = INVALID_HANDLE_VALUE;

    DWORD EndOfFile = 0;

    DWORD NumberOfBytesWritten = 0;

    char DateTimeString[96] = { 0 };

    char SeverityString[8] = { 0 };

    char FormattedString[4096] = { 0 };

    int Error = 0;

    if (gRegistryParams.LogLevel < LogLevel)
    {
        return;
    }
    if (MessageLength < 1 || MessageLength > 4096)
    {
        return;
    }
    switch (LogLevel)
    {
        case LL_NONE:
        {
            return;
        }
        case LL_INFO:
        {
            strcpy_s(SeverityString, sizeof(SeverityString), "[INFO]");
            break;
        }
        case LL_WARNING:
        {
            strcpy_s(SeverityString, sizeof(SeverityString), "[WARN]");
            break;
        }
        case LL_ERROR:
        {
            strcpy_s(SeverityString, sizeof(SeverityString), "[ERROR]");
            break;
        }
        case LL_DEBUG:
        {
            strcpy_s(SeverityString, sizeof(SeverityString), "[DEBUG]");
            break;
        }
        default:
        {

            //assert?? pop-up error?
        }
    }
    GetLocalTime(&Time);

    va_list ArgPointer = NULL;
    va_start(ArgPointer, Message);

    _vsnprintf_s(FormattedString, sizeof(FormattedString), _TRUNCATE, Message, ArgPointer);

    va_end(ArgPointer);

    Error = _snprintf_s(DateTimeString, sizeof(DateTimeString), _TRUNCATE, "\r\n[%02u/%02u/%u %02u:%02u:%02u.%03u]", Time.wMonth, Time.wDay, Time.wYear, Time.wHour, Time.wMinute, Time.wSecond, Time.wMilliseconds);

    if ((LogFileHandle = CreateFileA(LOG_FILE_NAME, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
    {
        //assert? if log function fails?
        return;
    }

    EndOfFile = SetFilePointer(LogFileHandle, 0, NULL, FILE_END);

    WriteFile(LogFileHandle, DateTimeString, (DWORD)strlen(DateTimeString), &NumberOfBytesWritten, NULL);
    WriteFile(LogFileHandle, SeverityString, (DWORD)strlen(SeverityString), &NumberOfBytesWritten, NULL);
    WriteFile(LogFileHandle, FormattedString, (DWORD)strlen(FormattedString), &NumberOfBytesWritten, NULL);
    if (LogFileHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(LogFileHandle);
    }
}

void DrawDebugInfo(void)
{
    char DebugTextBuffer[64] = { 0 };
    PIXEL32 White = { 0xFF, 0xFF, 0xFF, 0xFF };
    PIXEL32 LimeGreen = { 0x00, 0xFF, 0x17, 0xFF };
    PIXEL32 SkyBlue = { 0xFF, 0x0F, 0x00, 0xFF };


    sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer), "FPS: %.01f (%.01f)", gGamePerformanceData.CookedFPSAverage, gGamePerformanceData.RawFPSAverage);
    BlitStringToBuffer(DebugTextBuffer, &g6x7Font, &LimeGreen, 0, 0);
    sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer), "Timer: %.02f/%.02f/%.02f", (gGamePerformanceData.CurrentTimerResolution / 10000.0f), (gGamePerformanceData.MinimumTimerResolution / 10000.0f), (gGamePerformanceData.MaximumTimerResolution / 10000.0f));
    BlitStringToBuffer(DebugTextBuffer, &g6x7Font, &LimeGreen, 0, 8);
    sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer), "Memory: %llu KB ", (gGamePerformanceData.MemInfo.PrivateUsage / 1024));
    BlitStringToBuffer(DebugTextBuffer, &g6x7Font, &LimeGreen, 0, 16);
    sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer), "CPU: %.03f%%", gGamePerformanceData.CPUPercent);
    BlitStringToBuffer(DebugTextBuffer, &g6x7Font, &LimeGreen, 0, 24);
    sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer), "Handles  :  %lu ", gGamePerformanceData.HandleCount);
    BlitStringToBuffer(DebugTextBuffer, &g6x7Font, &White, 0, 32);
    sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer), "Frames   :%llu", gGamePerformanceData.TotalFramesRendered);
    BlitStringToBuffer(DebugTextBuffer, &g6x7Font, &White, 0, 40);
}

HRESULT InitializeSoundEngine(void)
{
    HRESULT Result = S_OK;

    WAVEFORMATEX SfxWaveFormat = { 0 };
    WAVEFORMATEX MusicWaveFormat = { 0 };

    float SFXVolume = (DWORD)gSFXVolume / 100.0f;
    float MusicVolume = (DWORD)gMusicVolume / 100.0f;

    Result = CoInitializeEx(NULL, COINIT_MULTITHREADED);

    if (Result != S_OK)
    {
        LogMessageA(LL_ERROR, "[%s] CoInitializeEx failed with 0x%08lx!", __FUNCTION__, Result);
        goto Exit;
    }

    Result = XAudio2Create(&gXAudio, 0, XAUDIO2_ANY_PROCESSOR);

    if (FAILED(Result))
    {
        LogMessageA(LL_ERROR, "[%s] XAudio2Create failed with 0x%08lx!", __FUNCTION__, Result);
        goto Exit;
    }

    Result = gXAudio->lpVtbl->CreateMasteringVoice(gXAudio, &gXAudioMasteringVoice, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, 0, NULL, 0);

    if (FAILED(Result))
    {
        LogMessageA(LL_ERROR, "[%s] CreateMasteringVoice failed with 0x%08lx!", __FUNCTION__, Result);
        goto Exit;
    }

    SfxWaveFormat.wFormatTag = WAVE_FORMAT_PCM;

    SfxWaveFormat.nChannels = 1;    //mono

    SfxWaveFormat.nSamplesPerSec = 44100;   //hz of .wav file

    SfxWaveFormat.nAvgBytesPerSec = SfxWaveFormat.nSamplesPerSec * SfxWaveFormat.nChannels * 2;

    SfxWaveFormat.nBlockAlign = SfxWaveFormat.nChannels * 2;

    SfxWaveFormat.wBitsPerSample = 16;

    SfxWaveFormat.cbSize = 0x6164;

    for (uint8_t Counter = 0; Counter < NUMBER_OF_SFX_SOURCE_VOICES; Counter++)
    {
        Result = gXAudio->lpVtbl->CreateSourceVoice(gXAudio, &gXAudioSFXSourceVoice[Counter], &SfxWaveFormat, 0, XAUDIO2_DEFAULT_FREQ_RATIO, NULL, NULL, NULL);

        if (Result != S_OK)
        {
            LogMessageA(LL_ERROR, "[%s] CreateSourceVoice for SFX failed with 0x%08lx!", __FUNCTION__, Result);
            goto Exit;
        }

        gXAudioSFXSourceVoice[Counter]->lpVtbl->SetVolume(gXAudioSFXSourceVoice[Counter], SFXVolume, XAUDIO2_COMMIT_NOW);
    }

    MusicWaveFormat.wFormatTag = WAVE_FORMAT_PCM;

    MusicWaveFormat.nChannels = 2;      //stereo

    MusicWaveFormat.nSamplesPerSec = 44100;

    MusicWaveFormat.nAvgBytesPerSec = MusicWaveFormat.nSamplesPerSec * MusicWaveFormat.nChannels * 2;

    MusicWaveFormat.nBlockAlign = MusicWaveFormat.nChannels * 2;

    MusicWaveFormat.wBitsPerSample = 16;

    MusicWaveFormat.cbSize = 0;

    Result = gXAudio->lpVtbl->CreateSourceVoice(gXAudio, &gXAudioMusicSourceVoice, &MusicWaveFormat, 0, XAUDIO2_DEFAULT_FREQ_RATIO, NULL, NULL, NULL);

    if (Result != S_OK)
    {

        LogMessageA(LL_ERROR, "[%s] CreateSourceVoice for MUSIC failed with 0x%08lx!", __FUNCTION__, Result);
        goto Exit;
    }

    gXAudioMusicSourceVoice->lpVtbl->SetVolume(gXAudioMusicSourceVoice, MusicVolume, XAUDIO2_COMMIT_NOW);

Exit:
    return(Result);
}



DWORD LoadWaveFromMem(_In_ void* Buffer, _Inout_ GAMESOUND* GameSound)
{

    DWORD Error = ERROR_SUCCESS;

    //DWORD NumberOfBytesRead = 0;

    DWORD RIFF = 0;

    uint16_t DataChunkOffset = 0;

    DWORD DataChunkSearcher = 0;

    BOOL DataChunkFound = FALSE;

    DWORD DataChunkSize = 0;

    memcpy(&RIFF, Buffer, sizeof(DWORD));

    if (RIFF != 0x46464952)     //0x46464952 is "RIFF" backwards
    {
        Error = ERROR_FILE_INVALID;
        LogMessageA(LL_ERROR, "[%s] First four bytes of memory buffer are not 'RIFF'! Error 0x%08lx!", __FUNCTION__, Error);
        goto Exit;
    }

    memcpy(&GameSound->WaveFormat, (BYTE*)Buffer + 20, sizeof(WAVEFORMATEX));

    if (GameSound->WaveFormat.nBlockAlign != (GameSound->WaveFormat.nChannels * GameSound->WaveFormat.wBitsPerSample / 8) ||
        GameSound->WaveFormat.wFormatTag != WAVE_FORMAT_PCM ||
        GameSound->WaveFormat.wBitsPerSample != 16)
    {
        Error = ERROR_DATATYPE_MISMATCH;
        LogMessageA(LL_ERROR, "[%s] This wav data in the memory buffer did not meet format requirements! Only PCM format, 44.1kHz, 16bits per sample wav files are supported. Error 0x%08lx!", __FUNCTION__, Error);
        goto Exit;
    }

    while (DataChunkFound == FALSE)
    {

        memcpy(&DataChunkSearcher, (BYTE*)Buffer + DataChunkOffset, sizeof(DWORD));

        if (DataChunkSearcher == 0x61746164)    ////'data' backwords
        {
            DataChunkFound = TRUE;
            break;
        }
        else
        {
            DataChunkOffset += 4;
        }

        if (DataChunkOffset > 256)
        {
            Error = ERROR_DATATYPE_MISMATCH;
            LogMessageA(LL_ERROR, "[%s] Datachunk not found in first 256 bytes of the memory buffer! Error 0x%08lx!", __FUNCTION__, Error);
            goto Exit;
        }
    }

    memcpy(&DataChunkSize, (BYTE*)Buffer + DataChunkOffset + 4, sizeof(DWORD));

    GameSound->Buffer.Flags = XAUDIO2_END_OF_STREAM;
    GameSound->Buffer.AudioBytes = DataChunkSize;
    GameSound->Buffer.pAudioData = (BYTE*)Buffer + DataChunkOffset + 8;

Exit:

    if (Error == ERROR_SUCCESS)
    {
        LogMessageA(LL_INFO, "[%s] Successfully loaded wav from memory!", __FUNCTION__);
    }
    else
    {
        LogMessageA(LL_ERROR, "[%s] Failed to load wav from memory! Error 0x%08lx!", __FUNCTION__, Error);
    }

    return(Error);
}


DWORD LoadOggFromMem(_In_ void* Buffer, _In_ uint32_t BufferSize, _Inout_ GAMESOUND* GameSound)
{
    DWORD Error = ERROR_SUCCESS;

    int SamplesDecoded = 0;

    int Channels = 0;

    int SampleRate = 0;

    short* DecodedAudio = NULL;

    LogMessageA(LL_INFO, "[%s] Size of Ogg file: %lu.", __FUNCTION__, BufferSize);

    SamplesDecoded = stb_vorbis_decode_memory(Buffer, (int)BufferSize, &Channels, &SampleRate, &DecodedAudio);

    if (SamplesDecoded < 1)
    {
        Error = ERROR_BAD_COMPRESSION_BUFFER;
        LogMessageA(LL_ERROR, "[%s] stb_vorbis_decode_memory failed with 0x%08lx!", __FUNCTION__, Error);
        goto Exit;
    }

    GameSound->WaveFormat.wFormatTag = WAVE_FORMAT_PCM;

    GameSound->WaveFormat.nChannels = Channels;

    GameSound->WaveFormat.nSamplesPerSec = SampleRate;

    GameSound->WaveFormat.nAvgBytesPerSec = GameSound->WaveFormat.nSamplesPerSec * GameSound->WaveFormat.nChannels * 2;

    GameSound->WaveFormat.nBlockAlign = GameSound->WaveFormat.nChannels * 2;

    GameSound->WaveFormat.wBitsPerSample = 16;

    GameSound->Buffer.Flags = XAUDIO2_END_OF_STREAM;

    GameSound->Buffer.AudioBytes = SamplesDecoded * GameSound->WaveFormat.nChannels * 2;

    GameSound->Buffer.pAudioData = DecodedAudio;


Exit:

    return(Error);
}



DWORD LoadTileMapFromMem(_In_ void* Buffer, _In_ uint32_t BufferSize, _Inout_ TILEMAP* TileMap)
{
    DWORD Error = ERROR_SUCCESS;

    DWORD BytesRead = 0;

    char* Cursor = NULL;

    char TempBuffer[16] = { 0 };

    uint16_t Rows = 0;

    uint16_t Columns = 0;


    if (BufferSize < 300)
    {
        Error = ERROR_FILE_INVALID;

        LogMessageA(LL_ERROR, "[%s] Buffer is too small to be a valid tile map! 0x08%lx!", __FUNCTION__, Error);

        goto Exit;
    }



    ///////////width

    if ((Cursor = strstr(Buffer, "width=")) == NULL)
    {
        Error = ERROR_INVALID_DATA;
        LogMessageA(LL_ERROR, "[%s] Could not locate Width attribute! 0x%08lx!", __FUNCTION__, Error);
        goto Exit;
    }

    BytesRead = 0;      //reset

    for (;;)
    {
        if (BytesRead > 8)
        {
            ////should have found opening quotation mark ("width"=)
            Error = ERROR_INVALID_DATA;
            LogMessageA(LL_ERROR, "[%s] Could not locate opening quotation mark before Width attribute! 0x%08lx!", __FUNCTION__, Error);
            goto Exit;
        }
        if (*Cursor == '\"')
        {
            Cursor++;
            break;
        }
        else
        {
            Cursor++;
        }
        BytesRead++;
    }

    BytesRead = 0;      //reset

    for (uint8_t Counter = 0; Counter < 6; Counter++)
    {
        if (*Cursor == '\"')
        {
            Cursor++;
            break;
        }
        else
        {
            TempBuffer[Counter] = *Cursor;
            Cursor++;
        }
    }

    TileMap->Width = atoi(TempBuffer);
    if (TileMap->Width == 0)
    {
        Error = ERROR_INVALID_DATA;
        LogMessageA(LL_ERROR, "[%s] Width attribute was 0! 0x%08lx!", __FUNCTION__, Error);
        goto Exit;
    }

    memset(TempBuffer, 0, sizeof(TempBuffer));

    //////////height

    if ((Cursor = strstr(Buffer, "height=")) == NULL)
    {
        Error = ERROR_INVALID_DATA;
        LogMessageA(LL_ERROR, "[%s] Could not locate height attribute! 0x%08lx!", __FUNCTION__, Error);
        goto Exit;
    }

    BytesRead = 0;      //reset

    for (;;)
    {
        if (BytesRead > 8)
        {
            ////should have found opening quotation mark ("height"=)
            Error = ERROR_INVALID_DATA;
            LogMessageA(LL_ERROR, "[%s] Could not locate opening quotation mark before height attribute! 0x%08lx!", __FUNCTION__, Error);
            goto Exit;
        }
        if (*Cursor == '\"')
        {
            Cursor++;
            break;
        }
        else
        {
            Cursor++;
        }
        BytesRead++;
    }

    BytesRead = 0;      //reset

    for (uint8_t Counter = 0; Counter < 6; Counter++)
    {
        if (*Cursor == '\"')
        {
            Cursor++;
            break;
        }
        else
        {
            TempBuffer[Counter] = *Cursor;
            Cursor++;
        }
    }

    TileMap->Height = atoi(TempBuffer);
    if (TileMap->Height == 0)
    {
        Error = ERROR_INVALID_DATA;
        LogMessageA(LL_ERROR, "[%s] Height attribute was 0! 0x%08lx!", __FUNCTION__, Error);
        goto Exit;
    }

    LogMessageA(LL_INFO, "[%s] TileMap dimensions: %dx%d.", __FUNCTION__, TileMap->Width, TileMap->Height);

    Rows = TileMap->Height;

    Columns = TileMap->Width;

    TileMap->Map = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, Rows * sizeof(void*));

    if (TileMap->Map == NULL)
    {
        Error = ERROR_OUTOFMEMORY;
        LogMessageA(LL_ERROR, "[%s] HeapAlloc of height Failed with error 0x%08lx!", __FUNCTION__, Error);
        goto Exit;
    }

    for (uint16_t Counter = 0; Counter < TileMap->Height; Counter++)
    {
        TileMap->Map[Counter] = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, Columns * sizeof(void*));

        if (TileMap->Map[Counter] == NULL)
        {
            Error = ERROR_OUTOFMEMORY;
            LogMessageA(LL_ERROR, "[%s] HeapAlloc of width Failed with error 0x%08lx!", __FUNCTION__, Error);
            goto Exit;
        }
    }

    BytesRead = 0;

    memset(TempBuffer, 0, sizeof(TempBuffer));

    if ((Cursor = strstr(Buffer, ",")) == NULL)
    {
        Error = ERROR_INVALID_DATA;

        LogMessageA(LL_ERROR, "[%s] Could not find a comma character in the Buffer! 0x%08lx!", __FUNCTION__, Error);

        goto Exit;
    }

    while (*Cursor != '\r' && *Cursor != '\n')
    {
        if (BytesRead > 4)
        {
            Error = ERROR_INVALID_DATA;

            LogMessageA(LL_ERROR, "[%s] Could not find a new line character at the beginning of the tile map data in the Buffer! 0x%08lx!", __FUNCTION__, Error);

            goto Exit;
        }

        BytesRead++;

        Cursor--;
    }

    Cursor++;

    for (uint16_t Row = 0; Row < Rows; Row++)
    {
        for (uint16_t Column = 0; Column < Columns; Column++)
        {
            memset(TempBuffer, 0, sizeof(TempBuffer));

        Skip:

            if (*Cursor == '\r' || *Cursor == '\n')
            {
                Cursor++;

                goto Skip;
            }

            for (uint8_t Counter = 0; Counter <= 10; Counter++)
            {
                if (*Cursor == ',' || *Cursor == '<')
                {
                    if (((TileMap->Map[Row][Column]) = (uint8_t)atoi(TempBuffer)) == 0)
                    {
                        Error = ERROR_INVALID_DATA;

                        LogMessageA(LL_ERROR, "[%s] atoi failed while converting tile map data in the Buffer! 0x%08lx!", __FUNCTION__, Error);

                        goto Exit;
                    }

                    Cursor++;

                    break;
                }

                TempBuffer[Counter] = *Cursor;

                Cursor++;
            }
        }
    }



Exit:

    ////TODO: free Buffer somehow, using an mz_ function probably
/*if (Buffer)
{
    HeapFree(GetProcessHeap(), 0, Buffer);
}*/

    return(Error);
}



DWORD Load32BppBitmapFromMem(_In_ void* Buffer, _Inout_ GAMEBITMAP* GameBitmap)
{
    DWORD Error = ERROR_SUCCESS;

    WORD BitmapHeader = 0;

    DWORD PixelDataOffset = 0;

    //DWORD NumberOfBytesRead = 2;

    memcpy(&BitmapHeader, Buffer, 2);

    if (BitmapHeader != 0x4d42)     //0x4d42 is "BM" backwards
    {
        Error = ERROR_INVALID_DATA;
        LogMessageA(LL_ERROR, "[%s] First two bytes are not 'BM'! Error 0x%08lx!", __FUNCTION__, Error);
        goto Exit;
    }

    memcpy(&PixelDataOffset, (BYTE*)Buffer + 0xA, sizeof(DWORD));

    memcpy(&GameBitmap->BitmapInfo.bmiHeader, (BYTE*)Buffer + 0xE, sizeof(BITMAPINFOHEADER));

    GameBitmap->Memory = (BYTE*)Buffer + PixelDataOffset;



Exit:

    if (Error == ERROR_SUCCESS)
    {
        LogMessageA(LL_INFO, "[%s] Loading Successful", __FUNCTION__);
    }
    else
    {
        LogMessageA(LL_ERROR, "[%s] Loading failed! Error 0x%08lx!", __FUNCTION__, Error);
    }

    return(Error);
}

void DrawWindow(
    _In_opt_ uint16_t x,
    _In_opt_ uint16_t y,
    _In_ int16_t Width,
    _In_ int16_t Height,
    _In_opt_ PIXEL32* BorderColor,
    _In_opt_ PIXEL32* BackgroundColor,
    _In_opt_ PIXEL32* ShadowColor,
    _In_ DWORD Flags)
{
    if (Flags & WINDOW_FLAG_HORIZ_CENTERED)
    {
        x = (GAME_RES_WIDTH / 2) - (Width / 2);
    }

    if (Flags & WINDOW_FLAG_VERT_CENTERED)
    {
        y = (GAME_RES_HEIGHT / 2) - (Height / 2);
    }

    ASSERT((x + Width <= GAME_RES_WIDTH) && (y + Height <= GAME_RES_HEIGHT), "Window is off the screen!");

    ASSERT((Flags & WINDOW_FLAG_BORDERED) || (Flags & WINDOW_FLAG_OPAQUE), "Window must have either the BORDERED or the OPAQUE flags (or both) set!");

    int32_t StartingScreenPixel = ((GAME_RES_WIDTH * GAME_RES_HEIGHT) - GAME_RES_WIDTH) - (GAME_RES_WIDTH * y) + x;

    if (Flags & WINDOW_FLAG_OPAQUE)
    {
        if (BackgroundColor == NULL)
        {
            goto SkipOpaque;
        }

        //ASSERT(BackgroundColor != NULL, "WINDOW_FLAG_OPAQUE is set but BackgroundColor is NULL!");  ////I want to simply not draw when NULL, and sometimes fill that space with color

        for (int Row = 0; Row < Height; Row++)
        {
            int MemoryOffset = StartingScreenPixel - (GAME_RES_WIDTH * Row);

            // If the user wants rounded corners, don't draw the first and last pixels on the first and last rows.
            // Get a load of this sweet ternary action:
            for (int Pixel = ((Flags & WINDOW_FLAG_ROUNDED) && (Row == 0 || Row == Height - 1)) ? 1 : 0;
                Pixel < Width - ((Flags & WINDOW_FLAG_ROUNDED) && (Row == 0 || Row == Height - 1)) ? 1 : 0;
                Pixel++)
            {
                memcpy((PIXEL32*)gBackBuffer.Memory + MemoryOffset + Pixel, BackgroundColor, sizeof(PIXEL32));
            }
        }
    }

SkipOpaque: ////skipped when WINDOW_FLAG_OPAQUE is set but BackgroundColor is NULL

    if (Flags & WINDOW_FLAG_BORDERED)
    {
        ASSERT(BorderColor != NULL, "WINDOW_FLAG_BORDERED is set but BorderColor is NULL!");

        // Draw the top of the border.
        int MemoryOffset = StartingScreenPixel;

        for (int Pixel = ((Flags & WINDOW_FLAG_ROUNDED) ? 1 : 0);
            Pixel < Width - ((Flags & WINDOW_FLAG_ROUNDED) ? 1 : 0);
            Pixel++)
        {
            memcpy((PIXEL32*)gBackBuffer.Memory + MemoryOffset + Pixel, BorderColor, sizeof(PIXEL32));
        }

        // Draw the bottom of the border.
        MemoryOffset = StartingScreenPixel - (GAME_RES_WIDTH * (Height - 1));

        for (int Pixel = ((Flags & WINDOW_FLAG_ROUNDED) ? 1 : 0);
            Pixel < Width - ((Flags & WINDOW_FLAG_ROUNDED) ? 1 : 0);
            Pixel++)
        {
            memcpy((PIXEL32*)gBackBuffer.Memory + MemoryOffset + Pixel, BorderColor, sizeof(PIXEL32));
        }

        // Draw one pixel on the left side and the right for each row of the border, from the top down.
        for (int Row = 1; Row < Height - 1; Row++)
        {
            MemoryOffset = StartingScreenPixel - (GAME_RES_WIDTH * Row);

            memcpy((PIXEL32*)gBackBuffer.Memory + MemoryOffset, BorderColor, sizeof(PIXEL32));

            MemoryOffset = StartingScreenPixel - (GAME_RES_WIDTH * Row) + (Width - 1);

            memcpy((PIXEL32*)gBackBuffer.Memory + MemoryOffset, BorderColor, sizeof(PIXEL32));
        }

        // Recursion!
        // If the user wants a thick window, just draw a smaller concentric bordered window inside the existing window.
        if (Flags & WINDOW_FLAG_THICK)
        {
            DrawWindow(x + 1, y + 1, Width - 2, Height - 2, BorderColor, NULL, NULL, WINDOW_FLAG_BORDERED);
        }
    }

    // TODO: If a window was placed at the edge of the screen, the shadow effect might attempt
    // to draw off-screen and crash! i.e. make sure there's room to draw the shadow before attempting!
    if (Flags & WINDOW_FLAG_SHADOWED)
    {
        ASSERT(ShadowColor != NULL, "WINDOW_FLAG_SHADOW is set but ShadowColor is NULL!");

        // Draw the bottom of the shadow.
        int MemoryOffset = StartingScreenPixel - (GAME_RES_WIDTH * Height);

        for (int Pixel = 1;
            Pixel < Width + ((Flags & WINDOW_FLAG_ROUNDED) ? 0 : 1);
            Pixel++)
        {
            memcpy((PIXEL32*)gBackBuffer.Memory + MemoryOffset + Pixel, ShadowColor, sizeof(PIXEL32));
        }

        // Draw one pixel on the right side for each row of the border, from the top down.
        for (int Row = 1; Row < Height; Row++)
        {
            MemoryOffset = StartingScreenPixel - (GAME_RES_WIDTH * Row) + Width;

            memcpy((PIXEL32*)gBackBuffer.Memory + MemoryOffset, ShadowColor, sizeof(PIXEL32));
        }

        // Draw one shadow pixel in the bottom-right corner to compensate for rounded corner.
        if (Flags & WINDOW_FLAG_ROUNDED)
        {
            MemoryOffset = StartingScreenPixel - (GAME_RES_WIDTH * (Height - 1)) + (Width - 1);

            memcpy((PIXEL32*)gBackBuffer.Memory + MemoryOffset, ShadowColor, sizeof(PIXEL32));
        }
    }
}

void InitializeGlobals(void)
{
    gCurrentGameState = GAMESTATE_SPLASHSCREEN;

    gInputEnabled = TRUE;

    for (uint16_t i = 0, ptr = 0; i < 128; i++)
    {
        switch (ptr)
        {
            case 0:
            {
                gMenuItemPtr0[i % 16] = &gMenuItem[i];
                if (i % 16 == 15)
                {
                    ptr++;
                }
                break;
            }
            case 1:
            {
                gMenuItemPtr1[i % 16] = &gMenuItem[i];
                if (i % 16 == 15)
                {
                    ptr++;
                }
                break;
            }
            case 2:
            {
                gMenuItemPtr2[i % 16] = &gMenuItem[i];
                if (i % 16 == 15)
                {
                    ptr++;
                }
                break;
            }
            case 3:
            {
                gMenuItemPtr3[i % 16] = &gMenuItem[i];
                if (i % 16 == 15)
                {
                    ptr++;
                }
                break;
            }
            case 4:
            {
                gMenuItemPtr4[i % 16] = &gMenuItem[i];
                if (i % 16 == 15)
                {
                    ptr++;
                }
                break;
            }
            case 5:
            {
                gMenuItemPtr5[i % 16] = &gMenuItem[i];
                if (i % 16 == 15)
                {
                    ptr++;
                }
                break;
            }
            case 6:
            {
                gMenuItemPtr6[i % 16] = &gMenuItem[i];
                if (i % 16 == 15)
                {
                    ptr++;
                }
                break;
            }
            case 7:
            {
                gMenuItemPtr7[i % 16] = &gMenuItem[i];
                if (i % 16 == 15)
                {
                    ptr++;
                }
                break;
            }
            case 8:
            {
                gMenuItemPtr8[i % 16] = &gMenuItem[i];
                if (i % 16 == 15)
                {
                    ptr++;
                }
                break;
            }
            case 9:
            {
                gMenuItemPtr9[i % 16] = &gMenuItem[i];
                if (i % 16 == 15)
                {
                    ptr++;
                }
                break;
            }
            case 10:
            {
                gMenuItemPtr10[i % 16] = &gMenuItem[i];
                if (i % 16 == 15)
                {
                    ptr++;
                }
                break;
            }
            case 11:
            {
                gMenuItemPtr11[i % 16] = &gMenuItem[i];
                if (i % 16 == 15)
                {
                    ptr++;
                }
                break;
            }
            case 12:
            {
                gMenuItemPtr12[i % 16] = &gMenuItem[i];
                if (i % 16 == 15)
                {
                    ptr++;
                }
                break;
            }
            case 13:
            {
                gMenuItemPtr13[i % 16] = &gMenuItem[i];
                if (i % 16 == 15)
                {
                    ptr++;
                }
                break;
            }
            case 14:
            {
                gMenuItemPtr14[i % 16] = &gMenuItem[i];
                if (i % 16 == 15)
                {
                    ptr++;
                }
                break;
            }
            case 15:
            {
                gMenuItemPtr15[i % 16] = &gMenuItem[i];
                if (i % 16 == 15)
                {
                    ptr++;
                }
                break;
            }
        }
    }
}


void PlayGameSound(_In_ GAMESOUND* GameSound)
{
    gXAudioSFXSourceVoice[gSFXSourceVoiceSelector]->lpVtbl->SubmitSourceBuffer(gXAudioSFXSourceVoice[gSFXSourceVoiceSelector], &GameSound->Buffer, NULL);

    gXAudioSFXSourceVoice[gSFXSourceVoiceSelector]->lpVtbl->Start(gXAudioSFXSourceVoice[gSFXSourceVoiceSelector], 0, XAUDIO2_COMMIT_NOW);

    gSFXSourceVoiceSelector++;

    if (gSFXSourceVoiceSelector > (NUMBER_OF_SFX_SOURCE_VOICES - 1))
    {
        gSFXSourceVoiceSelector = 0;
    }
}

void PauseGameMusic(void)
{
    gXAudioMusicSourceVoice->lpVtbl->Stop(gXAudioMusicSourceVoice, 0, 0);
    gMusicPaused = TRUE;
}

void StopGameMusic(void)
{
    gXAudioMusicSourceVoice->lpVtbl->Stop(gXAudioMusicSourceVoice, 0, 0);

    gXAudioMusicSourceVoice->lpVtbl->FlushSourceBuffers(gXAudioMusicSourceVoice);

    gMusicPaused = FALSE;
}

void PlayGameMusic(_In_ GAMESOUND* GameSound, _In_ BOOL Looping, _In_ BOOL Immediate)
{
    if (gMusicPaused == FALSE)
    {
        if (Immediate)
        {
            gXAudioMusicSourceVoice->lpVtbl->Stop(gXAudioMusicSourceVoice, 0, 0);

            gXAudioMusicSourceVoice->lpVtbl->FlushSourceBuffers(gXAudioMusicSourceVoice);
        }

        if (Looping == TRUE)
        {
            GameSound->Buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
        }
        else
        {
            GameSound->Buffer.LoopCount = 0;
        }

        gXAudioMusicSourceVoice->lpVtbl->SubmitSourceBuffer(gXAudioMusicSourceVoice, &GameSound->Buffer, NULL);

        gXAudioMusicSourceVoice->lpVtbl->Start(gXAudioMusicSourceVoice, 0, XAUDIO2_COMMIT_NOW);

    }
    else
    {
        gXAudioMusicSourceVoice->lpVtbl->Start(gXAudioMusicSourceVoice, 0, XAUDIO2_COMMIT_NOW);
    }
    gMusicPaused = FALSE;
}

BOOL MusicIsPlaying(void)
{
    XAUDIO2_VOICE_STATE  State = { 0 };

    gXAudioMusicSourceVoice->lpVtbl->GetState(gXAudioMusicSourceVoice, &State, 0);

    if ((State.BuffersQueued > 0) && (gMusicPaused == FALSE))
    {
        return(TRUE);
    }
    else
    {
        return(FALSE);
    }
}

//MAX characters per row = 32, MAX rows = 7, only input needed is text
//Use character "\n" with no spaces behind for next row (spaces after will indent)
void DrawDialogueBox(_In_ char* Dialogue, _In_opt_ uint64_t Counter, _In_opt_ DWORD Flags)
{
    static uint8_t DialogueCharactersToShow;
    static uint8_t DialogueCharactersWritten;
    static uint8_t DialogueRowsToShow;
    char DialogueLineScratch[32] = { 0 };

    char InString[224] = { 0 };
    char* NextToken = NULL;
    char Separator[] = "\n";

    char* StrPtr[8];

    DrawWindow(1, 170, 192, 64, &COLOR_NES_WHITE, &COLOR_DARK_WHITE, &COLOR_DARK_GRAY, WINDOW_FLAG_HORIZ_CENTERED | WINDOW_FLAG_OPAQUE | WINDOW_FLAG_SHADOWED | WINDOW_FLAG_THICK | WINDOW_FLAG_BORDERED | WINDOW_FLAG_ROUNDED);
    if (strlen(Dialogue) <= 32 * MAX_DIALOGUE_ROWS && strlen(Dialogue) > 0)
    {
        strcpy_s(InString, 224, Dialogue);        ////need to define max msg length bc sizeof() and strlen() both result in errors

        for (uint8_t i = 0; i < MAX_DIALOGUE_ROWS; i++)       ////split string into pieces using \n as a separator
        {
            if (!i)
            {
                StrPtr[i] = strtok_s(InString, Separator, &NextToken);
            }
            else
            {
                StrPtr[i] = strtok_s(NULL, Separator, &NextToken);
            }
        }

        if (((Counter % (gRegistryParams.TextSpeed + 1) == 0) && (gRegistryParams.TextSpeed < 4)) && gFinishedDialogueTextAnimation == FALSE)
        {
            if (DialogueCharactersToShow <= strlen(StrPtr[DialogueRowsToShow + 1]))
            {
                DialogueCharactersToShow++;
                DialogueCharactersWritten++;
            }
            else if (DialogueCharactersToShow > strlen(StrPtr[DialogueRowsToShow + 1]) && DialogueCharactersWritten < strlen(Dialogue))   ////TODO FIX BUG: when using \n, gFinishedDialogueTextAnimation will be set at shortest line finishing, not once all text is finished
            {
                DialogueCharactersToShow = 0;
                DialogueRowsToShow++;
            }

            if (DialogueCharactersWritten > strlen(Dialogue))
            {
                DialogueCharactersWritten = 0;
                DialogueCharactersToShow = 0;
                DialogueRowsToShow = 0;
                goto StartBlit;
            }
        }
        else if (gRegistryParams.TextSpeed == 4 || gFinishedDialogueTextAnimation == TRUE)
        {

        StartBlit:

            for (uint8_t i = 0; i < MAX_DIALOGUE_ROWS; i++)
            {
                if (StrPtr[i] != NULL)
                {
                    BlitStringToBuffer(StrPtr[i], &g6x7Font, &COLOR_BLACK, 100, 174 + ((i) * 8));                 //////every time \n is called add a row to the dialogue box
                }
            }

            BlitStringToBuffer("�", &g6x7Font, &COLOR_BLACK, 276, 224);
            gFinishedDialogueTextAnimation = TRUE;
            gDialogueControls = TRUE;
            DialogueCharactersWritten = 0;
            DialogueCharactersToShow = 0;
            DialogueRowsToShow = 0;

        }

        if (!gFinishedDialogueTextAnimation)
        {
            for (uint8_t i = 0; i < MAX_DIALOGUE_ROWS; i++)
            {
                if (StrPtr[i] != NULL)
                {
                    if (DialogueRowsToShow == i)
                    {
                        snprintf(DialogueLineScratch, DialogueCharactersToShow, "%s", StrPtr[i]);
                        BlitStringToBuffer(DialogueLineScratch, &g6x7Font, &COLOR_BLACK, 100, 174 + ((i) * 8));
                        break;
                    }
                    BlitStringToBuffer(StrPtr[i], &g6x7Font, &COLOR_BLACK, 100, 174 + ((i) * 8));                 //////every time \n is called add a row to the dialogue box
                }
            }
        }
    }
    else
    {
        BlitStringToBuffer("MSG UNDEFINED CHECK LOG FILE", &g6x7Font, &COLOR_BLACK, 101, 174);
        LogMessageA(LL_ERROR, "[%s] ERROR: String '%d' was over 224 (32chars * 7rows) characters!", __FUNCTION__, Dialogue);
    }

    /*if (Flags && gFinishedDialogueTextAnimation)
    {

    }*/
}

DWORD LoadAssetFromArchive(_In_ char* ArchiveName, _In_ char* AssetFileName, _Inout_ void* Resource)
{
    DWORD Error = ERROR_SUCCESS;

    mz_zip_archive Archive = { 0 };

    BYTE* DecompressedBuffer = NULL;

    size_t DecompressedSize = 0;

    BOOL FileFoundInArchive = FALSE;

    char* FileExtension = NULL;

    if ((mz_zip_reader_init_file(&Archive, ArchiveName, 0)) == FALSE)
    {
        Error = mz_zip_get_last_error(&Archive);

        char* ErrorMessage = mz_zip_get_error_string(Error);

        LogMessageA(LL_ERROR, "[%s] mz_zip_reader_init_file failed with error 0x%08lx on archive file %s! Error: %s", __FUNCTION__, Error, ArchiveName, ErrorMessage);

        goto Exit;
    }

    //      iterate through each file until we find the correct one

    for (int FileIndex = 0; FileIndex < (int)mz_zip_reader_get_num_files(&Archive); FileIndex++)
    {
        mz_zip_archive_file_stat CompressedFileStatistics = { 0 };

        if (mz_zip_reader_file_stat(&Archive, FileIndex, &CompressedFileStatistics) == MZ_FALSE)
        {
            Error = mz_zip_get_last_error(&Archive);

            char* ErrorMessage = mz_zip_get_error_string(Error);

            LogMessageA(LL_ERROR, "[%s] mz_zip_reader_init_file failed with error 0x%08lx! Archive: %s File: %s Error: %s", __FUNCTION__, Error, ArchiveName, AssetFileName, ErrorMessage);

            goto Exit;

        }

        if (_stricmp(CompressedFileStatistics.m_filename, AssetFileName) == 0)      //changed FileIndex to AssetFileName and program works.
        {
            FileFoundInArchive = TRUE;

            if ((DecompressedBuffer = mz_zip_reader_extract_file_to_heap(&Archive, AssetFileName, &DecompressedSize, 0)) == NULL)   ////need both parentheticals around DecompressedBuffer otherwise it is NULL
            {
                Error = mz_zip_get_last_error(&Archive);

                char* ErrorMessage = mz_zip_get_error_string(Error);

                LogMessageA(LL_ERROR, "[%s] mz_zip_reader_extract_file_to_heap failed with error 0x%08lx! Archive: %s File: %s Error %s", __FUNCTION__, Error, ArchiveName, AssetFileName, ErrorMessage);

                goto Exit;
            }

            break;
        }
    }

    if (FileFoundInArchive == FALSE)
    {
        Error = ERROR_FILE_NOT_FOUND;

        LogMessageA(LL_ERROR, "[%s] File %s was not found in archive %s! 0x%08lx!", __FUNCTION__, AssetFileName, ArchiveName, Error);

        goto Exit;
    }

    for (int i = strlen(AssetFileName) - 1; i > 0; i--)     //look for period and point to file extension
    {
        FileExtension = &AssetFileName[i];
        if (FileExtension[0] == '.')
        {
            break;
        }
    }

    if (FileExtension && _stricmp(FileExtension, ".bmpx") == 0)
    {
        Error = Load32BppBitmapFromMem(DecompressedBuffer, Resource);
    }
    else if (FileExtension && _stricmp(FileExtension, ".ogg") == 0)
    {
        Error = LoadOggFromMem(DecompressedBuffer, (uint32_t)DecompressedSize, Resource);
    }
    else if (FileExtension && _stricmp(FileExtension, ".wav") == 0)
    {
        Error = LoadWaveFromMem(DecompressedBuffer, Resource);
    }
    else if (FileExtension && _stricmp(FileExtension, ".tmx") == 0)
    {
        Error = LoadTileMapFromMem(DecompressedBuffer, (uint32_t)DecompressedSize, Resource);
    }
    else
    {
        ASSERT(FALSE, "Unknown resource type in LoadAssetFromArchive!");
    }


Exit:

    mz_zip_reader_end(&Archive);

    return(Error);
}

DWORD AssetLoadingThreadProc(_In_ LPVOID lpParam)
{
    UNREFERENCED_PARAMETER(lpParam);

    DWORD Error = ERROR_SUCCESS;

    typedef struct ASSET
    {
        char* Name;

        void* Destination;

    } ASSET;

    ASSET Assets[] = {
        {   "PixelFont(6x7).bmpx", &g6x7Font },
        {   "PixelFont(4x5CAPS).bmpx", &g4x5CAPSFont },
        {   "PixelFont(4x5).bmpx", &g4x5Font },
        {   "SplashNoise.wav", &gSoundSplashScreen },           // last essential asset before main menu
        {   "menu.wav", &gSoundMenuNavigate },
        {   "item.wav", &gSoundMenuChoose },
    };

    int FinalEssentialAssetIndex = 1;

    LogMessageA(LL_INFO, "[%s] Asset loading has begun.", __FUNCTION__);

    for (int i = 0; i < _countof(Assets); i++)
    {
        if ((Error = LoadAssetFromArchive(ASSET_FILE, Assets[i].Name, Assets[i].Destination)) != ERROR_SUCCESS)
        {
            MessageBoxA(NULL, "LoadAssetFromArchive failed! Check log file!", "Error!", MB_ICONERROR | MB_OK);
            LogMessageA(LL_ERROR, "[%s] Loading %s failed with error 0x%08lx!", __FUNCTION__, Assets[i].Name, Error);
            goto Exit;
        }

        if (i == FinalEssentialAssetIndex)
        {
            //end of assets needed for splash screen and titlescreen
            SetEvent(gEssentialAssetsLoadedEvent);
        }
    }



Exit:

    if (Error == ERROR_SUCCESS)
    {
        LogMessageA(LL_INFO, "[%s] Asset loading ended successfully.", __FUNCTION__);
    }
    else
    {
        LogMessageA(LL_ERROR, "[%s] Asset loading failed with result 0x%08lx!", __FUNCTION__, Error);
    }

    return(Error);
}

void ApplyFadeIn(_In_ uint64_t FrameCounter, _In_ PIXEL32 DefaultTextColor, _Inout_ PIXEL32* TextColor, _Inout_opt_ int16_t* BrightnessAdjustment)
{
    ASSERT(_countof(gFadeBrightnessGradient) == FADE_DURATION_FRAMES, "gFadeBrightnessGradient has too few elements!");

    int16_t LocalBrightnessAdjustment;

    if (FrameCounter > FADE_DURATION_FRAMES)
    {
        return;
    }

    if (FrameCounter == FADE_DURATION_FRAMES)
    {
        gInputEnabled = TRUE;
        LocalBrightnessAdjustment = 0;
    }
    else
    {
        gInputEnabled = FALSE;
        LocalBrightnessAdjustment = gFadeBrightnessGradient[FrameCounter];
    }

    if (BrightnessAdjustment != NULL)
    {
        *BrightnessAdjustment = LocalBrightnessAdjustment;
    }

    TextColor->Colors.Red = (uint8_t)(min(255, max(0, DefaultTextColor.Colors.Red + LocalBrightnessAdjustment)));
    TextColor->Colors.Blue = (uint8_t)(min(255, max(0, DefaultTextColor.Colors.Blue + LocalBrightnessAdjustment)));
    TextColor->Colors.Green = (uint8_t)(min(255, max(0, DefaultTextColor.Colors.Green + LocalBrightnessAdjustment)));
}

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
    )
{
    MENU Menu = ClearMenu();
    uint8_t RowItems = 0;
    uint8_t ColumnItems = 0;

    if (menuX)
    {
        Menu.x = menuX;
    }

    if (menuY)
    {
        Menu.y = menuY;
    }

    if (fontsheet)
    {
        Menu.FontSheet = fontsheet;
    }
    else
    {
        Menu.FontSheet = &g6x7Font;
    }

    if (&pallet.border1 || &pallet.border2)
    {
        Menu.Pallet = pallet;
    }
    else
    {
        MENUPALLET color = { &COLOR_NES_GRAY, &COLOR_LIGHT_GRAY, &COLOR_DARK_GRAY, NULL, &COLOR_BLACK, NULL, &COLOR_NES_MAGENTA, &COLOR_NES_ORANGE };
        Menu.Pallet = color;
    }

    uint16_t MenuWidthX = widthX;
    uint16_t MenuWidthY = widthY;
    uint16_t ItemWidthX = itemWidthX;
    uint16_t ItemWidthY = itemWidthY;

    uint16_t xlength = 0;
    uint16_t ylength = 0;

    uint8_t dummyx = 0;
    uint8_t dummyy = 0;

    switch (flags)
    {
        case 0:
            //Assume NULL or 0 is a 2x2box
        case MENU_BOX2x2:
        {
            RowItems = 2;
            ColumnItems = 2;

            if (!widthX)
            {
                //default value;
                MenuWidthX = 128;
            }
            if (!widthY)
            {
                //default value;
                MenuWidthY = 128;
            }

            if (!itemWidthX)
            {
                //default value;
                ItemWidthX = 12;
            }
            if (!itemWidthY)
            {
                //default value;
                ItemWidthY = 12;
            }

            break;
        }
        case MENU_BOX3x3:
        {
            RowItems = 3;
            ColumnItems = 3;

            if (!widthX)
            {
                //default value;
                MenuWidthX = 192;
            }
            if (!widthY)
            {
                //default value;
                MenuWidthY = 192;
            }

            if (!itemWidthX)
            {
                //default value;
                ItemWidthX = 12;
            }
            if (!itemWidthY)
            {
                //default value;
                ItemWidthY = 12;
            }
            break;
        }
        case MENU_BOX4x4:
        {
            RowItems = 4;
            ColumnItems = 4;

            if (!widthX)
            {
                //default value;
                MenuWidthX = 216;
            }
            if (!widthY)
            {
                //default value;
                MenuWidthY = 216;   //240 is max height
            }

            if (!itemWidthX)
            {
                //default value;
                ItemWidthX = 12;
            }
            if (!itemWidthY)
            {
                //default value;
                ItemWidthY = 12;
            }
            break;
        }
        case MENU_BOX2x3:
        {
            RowItems = 2;
            ColumnItems = 3;

            if (!widthX)
            {
                //default value;
                MenuWidthX = 128;
            }
            if (!widthY)
            {
                //default value;
                MenuWidthY = 192;
            }

            if (!itemWidthX)
            {
                //default value;
                ItemWidthX = 12;
            }
            if (!itemWidthY)
            {
                //default value;
                ItemWidthY = 12;
            }
            break;
        }
        case MENU_BOX2x4:
        {
            RowItems = 2;
            ColumnItems = 4;

            if (!widthX)
            {
                //default value;
                MenuWidthX = 128;
            }
            if (!widthY)
            {
                //default value;
                MenuWidthY = 256;
            }

            if (!itemWidthX)
            {
                //default value;
                ItemWidthX = 12;
            }
            if (!itemWidthY)
            {
                //default value;
                ItemWidthY = 12;
            }
            break;
        }
        case MENU_BOX1x2:
        {
            RowItems = 1;
            ColumnItems = 2;

            if (!widthX)
            {
                //default value;
                MenuWidthX = 64;
            }
            if (!widthY)
            {
                //default value;
                MenuWidthY = 128;
            }

            if (!itemWidthX)
            {
                //default value;
                ItemWidthX = 12;
            }
            if (!itemWidthY)
            {
                //default value;
                ItemWidthY = 12;
            }
            break;
        }
        case MENU_BOX1x3:
        {
            RowItems = 1;
            ColumnItems = 3;

            if (!widthX)
            {
                //default value;
                MenuWidthX = 64;
            }
            if (!widthY)
            {
                //default value;
                MenuWidthY = 192;
            }

            if (!itemWidthX)
            {
                //default value;
                ItemWidthX = 12;
            }
            if (!itemWidthY)
            {
                //default value;
                ItemWidthY = 12;
            }
            break;
        }
        case MENU_BOX1x4:
        {
            RowItems = 1;
            ColumnItems = 4;

            if (!widthX)
            {
                //default value;
                MenuWidthX = 64;
            }
            if (!widthY)
            {
                //default value;
                MenuWidthY = 256;
            }

            if (!itemWidthX)
            {
                //default value;
                ItemWidthX = 12;
            }
            if (!itemWidthY)
            {
                //default value;
                ItemWidthY = 12;
            }
            break;
        }
        case MENU_BOX4x1:
        {
            RowItems = 4;
            ColumnItems = 1;

            if (!widthX)
            {
                //default value;
                MenuWidthX = 256;
            }
            if (!widthY)
            {
                //default value;
                MenuWidthY = 64;
            }

            if (!itemWidthX)
            {
                //default value;
                ItemWidthX = 12;
            }
            if (!itemWidthY)
            {
                //default value;
                ItemWidthY = 12;
            }
            break;
        }
        case MENU_BOX8x1:
        {
            RowItems = 8;
            ColumnItems = 1;

            if (!widthX)
            {
                //default value;
                MenuWidthX = 256;
            }
            if (!widthY)
            {
                //default value;
                MenuWidthY = 32;
            }

            if (!itemWidthX)
            {
                //default value;
                ItemWidthX = 12;
            }
            if (!itemWidthY)
            {
                //default value;
                ItemWidthY = 12;
            }
            break;
        }

        default:
        {
            ASSERT(FALSE, "Unknown flag in CreateMenuObj()");
            break;
        }
    }

    Menu.Rows = RowItems;
    Menu.Columns = ColumnItems;

    //solve for x,y of a tiny box centered in a partition of menu box for each partition

    xlength = MenuWidthX / RowItems;
    xlength -= ItemWidthX;
    xlength /= 2;

    ylength = MenuWidthY / ColumnItems;
    ylength -= ItemWidthY;
    ylength /= 2;

    if (xlength > GAME_RES_WIDTH || ylength > GAME_RES_HEIGHT)
    {
        ASSERT(FALSE, "An item width is too large for the menu width to be spit!");
    }

    //TOFIX:
    //only call this here so it will only increase gActiveMenus
    Menu.Items = AllocatePtrToMenuObj(Menu);

    for (uint8_t item = 0; item < Menu.Rows * Menu.Columns; item++)
    {
        if (((item) % RowItems == 0) && (item))
        {
            dummyx = 0;
            dummyy++;
        }


        Menu.Items[item]->x = menuX + xlength + (dummyx * (MenuWidthX / RowItems));
        Menu.Items[item]->y = menuY + ylength + (dummyy * (MenuWidthY / ColumnItems));
        Menu.Items[item]->width = ItemWidthX;
        Menu.Items[item]->height = ItemWidthY;
        Menu.Items[item]->Enabled = FALSE;


        dummyx++;
    }

    if ((Menu.x + MenuWidthX) > GAME_RES_WIDTH || Menu.y + MenuWidthY > GAME_RES_HEIGHT)
    {
        LogMessageA(LL_WARNING, "[%s] Error! CreateMenuObj created a menu offscreen with the right bottom coordinates of %d,%d!", __FUNCTION__, (Menu.x + MenuWidthX), (Menu.y + MenuWidthY));
        ASSERT(FALSE, "Created a menu object off screen in CreateMenuObj()");
    }

    Menu.width = MenuWidthX;
    Menu.height = MenuWidthY;

    Menu.Active = TRUE;

    return(Menu);
}

//returns false when no room to store a new menu object; index between 1 - 16, NULL = default
BOOL StoreMenuObj(_In_ MENU menu, _In_opt_ uint8_t index)
{
    BOOL retvalue = FALSE;
    if (gActiveMenus == MAX_MENUS)
    {
        goto Exit;
    }
    if (!menu.Active)
    {
        goto Exit;
    }

    if (index && !gMenuBuffer[index - 1].Active)
    {
        gMenuBuffer[index - 1] = menu;
        gActiveMenus++;
        retvalue = TRUE;
        goto Exit;
    }
    else    //Index not set
    {
        for (uint8_t M = 0; M < MAX_MENUS; M++)
        {
            if (!gMenuBuffer[M].Active)
            {
                gMenuBuffer[M] = menu;
                gActiveMenus++;
                retvalue = TRUE;
                goto Exit;
            }
        }
    }

    Exit:
    return(retvalue);
}

//menu will be NULL if no objects are stored, index between 1 - 16
MENU ReturnStoredMenuObj(_In_opt_ uint8_t index)
{
    MENU menu = { NULL };

    if (index)
    {
        if (gMenuBuffer[index - 1].Active)
        {
            menu = gMenuBuffer[index - 1];
            gMenuBuffer[index - 1] = ClearMenu();
            gActiveMenus--;
            goto Exit;
        }
        goto Exit;
    }

    //TOFIX: set M = MAX_MENUS and count down?? to keep priority of what menu was most recently added???
    for (uint8_t M = 0; M < MAX_MENUS; M++)
    {
        if (gMenuBuffer[M].Active)
        {
            menu = gMenuBuffer[M];
            gMenuBuffer[M] = ClearMenu();
            gActiveMenus--;
            break;
        }
    }


    Exit:
    return(menu);
}

MENU PlayerInputToMenuObj(_Inout_ MENU menu, INPUT_KEYS input)
{
    if (!input)
    {
        return(menu);
    }


    if (input & INPUT_WUP && menu.Columns > 0)
    {
        if (menu.SelectedItem <= menu.Rows - 1)     //top row
        {
            menu.SelectedItem += (menu.Rows * (menu.Columns - 1));
            PlayGameSound(&gSoundMenuNavigate);
        }
        else
        {
            menu.SelectedItem -= menu.Rows;
            PlayGameSound(&gSoundMenuNavigate);
        }
    }

    if (input & INPUT_ALEFT && menu.Rows > 0)
    {
        if ((menu.SelectedItem + menu.Rows) % menu.Rows == 0)   //left side
        {
            menu.SelectedItem += menu.Rows - 1;
            if (menu.Rows > 1)
            {
                PlayGameSound(&gSoundMenuNavigate);
            }
        }
        else
        {
            menu.SelectedItem--;
            PlayGameSound(&gSoundMenuNavigate);
        }
    }

    if (input & INPUT_SDOWN && menu.Columns > 0)
    {
        if (menu.SelectedItem >= (menu.Rows * (menu.Columns - 1)))  //bottom row
        {
            menu.SelectedItem -= (menu.Rows * (menu.Columns - 1));
            PlayGameSound(&gSoundMenuNavigate);
        }
        else
        {
            menu.SelectedItem += menu.Rows;
            PlayGameSound(&gSoundMenuNavigate);
        }
    }

    if (input & INPUT_DRIGHT && menu.Rows > 0)
    {
        if ((menu.SelectedItem + menu.Rows) % menu.Rows == menu.Rows - 1)   //right side
        {
            menu.SelectedItem -= menu.Rows - 1;
            if (menu.Rows > 1)
            {
                PlayGameSound(&gSoundMenuNavigate);
            }
        }
        else 
        {
            menu.SelectedItem++;
            PlayGameSound(&gSoundMenuNavigate);
        }
    }

    return(menu);
}

MENU ClearMenu(void)
{
    MENU menu = { NULL };
    return(menu);
}

//returns false if provided menu does not match one found in gMenuBuffer, index 1 - 16
BOOL DeleteGameMenu(uint8_t index)
{
    MENU tempmenu[16] = {NULL};
    BOOL retvalue = FALSE;

    if (index)
    {
        if (!gMenuBuffer[index - 1].Active)
        {
            LogMessageA(LL_WARNING, "[%s] Error! DeleteGameMenu(), tried to clear menu data that was already cleared!", __FUNCTION__);
        }
        gMenuBuffer[index - 1] = ClearMenu();
        gActiveMenus--;
        retvalue = TRUE;
        goto Exit;
    }
    else //when no index provided look at top most menu item
    {
        for (uint8_t current = 0, next = 0; next < MAX_MENUS; next++)
        {
            if (current == next)
            {
                tempmenu[next] = ReturnStoredMenuObj(next + 1);
                next++;
                tempmenu[next] = ReturnStoredMenuObj(next + 1);
            }
            else
            {
                tempmenu[next] = ReturnStoredMenuObj(next + 1);
            }

            if (tempmenu[current].Active && !tempmenu[next].Active)
            {
                //UnpopulateMenuItemPtr(gMenuBuffer[current].Items, gMenuBuffer[current]);
                gMenuBuffer[current] = ClearMenu();

                if (current > 0)    //store the objects that were returned but not deleted
                {
                    while (current != 0)
                    {
                        StoreMenuObj(tempmenu[current - 1], current);
                        current--;
                    }
                }

                retvalue = TRUE;
                goto Exit;
            }
            current++;
        }
    }

    Exit:
    return(retvalue);
}


MENU ModifyMenuObjData(MENU menu, DWORD flag, _In_opt_ uint16_t inputvalue)
{

    switch (flag)
    {
        case MODIFYMENU_NAME:
        {
            for (uint8_t character = 0; character < NAMELENGTH_MAX; character++)
            {
                menu.Name[character] = gMenuStrBuffer[character];
            }
            break;
        }
        case MODIFYMENU_X:
        {
            if (!inputvalue)
            {
                break;
            }
            menu.x = inputvalue;
            break;
        }
        case MODIFYMENU_Y:
        {
            if (!inputvalue)
            {
                break;
            }
            menu.y = inputvalue;
            break;
        }
        case MODIFYMENU_WIDTH:
        {
            if (!inputvalue)
            {
                break;
            }
            menu.width = inputvalue;
            break;
        }
        case MODIFYMENU_HEIGHT:
        {
            if (!inputvalue)
            {
                break;
            }
            menu.height = inputvalue;
            break;
        }
        case MODIFYMENU_ACTIVATE:
        {
            menu.Active = TRUE;
            break;
        }
        case MODIFYMENU_DEACTIVATE:
        {
            menu.Active = FALSE;
            break;
        }
        case MODIFYMENU_SELECTED:
        {
            if (!inputvalue)
            {
                break;
            }
            menu.SelectedItem = inputvalue;
            break;
        }
        default:
        {
            ASSERT(FALSE, "Unknown flag inside of ModifyMenuObjData!");
            break;
        }
    }

    return(menu);
}


MENU ModifyMenuItemData(MENU menu, uint8_t item16, DWORD flag, _In_opt_ uint16_t inputvalue)
{

    switch (flag)
    {
        case MODIFYITEM_NAME:
        {
            for (uint8_t character = 0; character < NAMELENGTH_MAX; character++)
            {
                menu.Items[item16]->Name[character] = gMenuStrBuffer[character];
            }
            break;
        }
        case MODIFYITEM_ENABLE:
        {
            menu.Items[item16]->Enabled = TRUE;
            break;
        }
        case MODIFYITEM_DISABLE:
        {
            menu.Items[item16]->Enabled = FALSE;
            break;
        }
        case MODIFYITEM_X:
        {
            if (!inputvalue)
            {
                break;
            }
            menu.Items[item16]->x = inputvalue;
            break;
        }
        case MODIFYITEM_Y:
        {
            if (!inputvalue)
            {
                break;
            }
            menu.Items[item16]->y = inputvalue;
            break;
        }
        case MODIFYITEM_WIDTH:
        {
            if (!inputvalue)
            {
                break;
            }
            menu.Items[item16]->width = inputvalue;
            break;
        }
        case MODIFYITEM_HEIGHT:
        {
            if (!inputvalue)
            {
                break;
            }
            menu.Items[item16]->height = inputvalue;
            break;
        }
        case MODIFYITEM_ACTION:
        {
            //TODO: change the action of the menuitem
            switch (inputvalue)
            {
                case MENUFUNC_QUIT: 
                {
                    menu.Items[item16]->Action = QuitGame;
                    break;
                }
                case MENUFUNC_GOTO_GS:
                {
                    menu.Items[item16]->Action = GoToDestGamestate;
                    break;
                }
                case MENUFUNC_PREV_GS:
                {
                    menu.Items[item16]->Action = GoToPrevGamestate;
                    break;
                }
                case MENUFUNC_SFX_VOL:
                {
                    menu.Items[item16]->Action = IncreaseSFXVolume;
                    break;
                }
                case MENUFUNC_MUSIC_VOL:
                {
                    menu.Items[item16]->Action = IncreaseMusicVolume;
                    break;
                }
                case MENUFUNC_SCREENSCALE:
                {
                    menu.Items[item16]->Action = IncreaseScaleFactor;
                    break;
                }
                case MENUFUNC_ALLOWACTION:
                {
                    menu.Items[item16]->Action = IncreaseScaleFactor;
                    break;
                }
            }
            break;
        }
        default:
        {
            ASSERT(FALSE, "Unknown flag inside of ModifyMenuItemData!");
            break;
        }
    }
    
    return(menu);
}

//must call this function before calling ModifyMenuItemData with a 'string' flag
BOOL SetMenuStringBuffer(char* string)
{
    BOOL retvalue = FALSE;
    size_t strsize = strlen(string);

    if ( strsize > MENUSTRLENGTH_MAX)
    {
        goto Exit;
    }
    else
    {
        strcpy_s(gMenuStrBuffer, sizeof(gMenuStrBuffer), string);
        retvalue = TRUE;
    }

    Exit:
    return(retvalue);
}

void DrawMenu( _In_ MENU menu )
{
    if (menu.Pallet.background1.Bytes == NULL)
    {
        DrawWindow(menu.x, menu.y, menu.width, menu.height, &menu.Pallet.border1, NULL, NULL, WINDOW_FLAG_OPAQUE | WINDOW_FLAG_BORDERED);
    }
    else
    {
        DrawWindow(menu.x, menu.y, menu.width, menu.height, &menu.Pallet.border1, &menu.Pallet.background1, NULL, WINDOW_FLAG_OPAQUE | WINDOW_FLAG_BORDERED);
    }

    if (menu.Pallet.background2.Bytes == NULL)
    {
        DrawWindow(menu.x + 1, menu.y + 1, menu.width - 2, menu.height - 2, &menu.Pallet.border2, NULL, NULL, WINDOW_FLAG_BORDERED | WINDOW_FLAG_OPAQUE);
    }
    else
    {
        DrawWindow(menu.x + 1, menu.y + 1, menu.width - 2, menu.height - 2, &menu.Pallet.border2, &menu.Pallet.background2, NULL, WINDOW_FLAG_BORDERED | WINDOW_FLAG_OPAQUE);
    }
    //TODO: dividing lines between options, options for bordered buttons, etc
    
    for (uint8_t items = 0; items < menu.Rows * menu.Columns; items++)
    {
        //TOREMOVE: temp for seeing scales and sizes, REUSE to be button borders??
        if (menu.Pallet.itembackground.Bytes == NULL)
        {
            DrawWindow(menu.Items[items]->x, menu.Items[items]->y, menu.Items[items]->width, menu.Items[items]->height, &menu.Pallet.itemborder, NULL, NULL, WINDOW_FLAG_BORDERED | WINDOW_FLAG_OPAQUE);
        }
        else
        {
            DrawWindow(menu.Items[items]->x, menu.Items[items]->y, menu.Items[items]->width, menu.Items[items]->height, &menu.Pallet.itemborder, &menu.Pallet.itembackground, NULL, WINDOW_FLAG_BORDERED | WINDOW_FLAG_OPAQUE);
        }
        //

        if (menu.Items[items]->Enabled == TRUE)
        {
            BlitStringToBuffer(menu.Items[items]->Name, menu.FontSheet, &menu.Pallet.text, menu.Items[items]->x + 2, menu.Items[items]->y + 2);
        }
    }

    BlitStringToBuffer("�", menu.FontSheet, &COLOR_BLACK, menu.Items[menu.SelectedItem]->x - 4, menu.Items[menu.SelectedItem]->y + 2);
    BlitStringToBuffer("�", menu.FontSheet, &menu.Pallet.cursor, menu.Items[menu.SelectedItem]->x - 5, menu.Items[menu.SelectedItem]->y + 2);
    BlitStringToBuffer("�", menu.FontSheet, &COLOR_BLACK, menu.Items[menu.SelectedItem]->x - 6, menu.Items[menu.SelectedItem]->y + 2);
}

void ProcessGameTickCalculation(void)
{
    
    //TODO: do calculations here




}

///returns negative if isactive is FALSE
//TODO: this is full of ifs, remove a few??
int16_t WASDMenuNavigation(BOOL isactive)
{
    int16_t retValue = 0;

    if (!isactive)
    {
        return(-1);
    }

    if (PlayerInputWUp())
    {
        if (!retValue)
        {
            retValue = INPUT_WUP;
        }
        else
        {
            retValue |= INPUT_WUP;
        }
    }
    if (PlayerInputALeft())
    {
        if (!retValue)
        {
            retValue = INPUT_ALEFT;
        }
        else
        {
            retValue |= INPUT_ALEFT;
        }
    }
    if (PlayerInputSDown())
    {
        if (!retValue)
        {
            retValue = INPUT_SDOWN;
        }
        else
        {
            retValue |= INPUT_SDOWN;
        }
    }
    if (PlayerInputDRight())
    {
        if (!retValue)
        {
            retValue = INPUT_DRIGHT;
        }
        else
        {
            retValue |= INPUT_DRIGHT;
        }
    }
        return(retValue);
}

    BOOL PlayerInputWUp(void)
{
    if (gGameInput.WUpKeyPressed && !gGameInput.WUpKeyAlreadyPressed)
    {
        return(TRUE);
    }
    else
    {
        return (FALSE);
    }
}

BOOL PlayerInputALeft(void)
{
    if (gGameInput.ALeftKeyPressed && !gGameInput.ALeftKeyAlreadyPressed)
    {
        return(TRUE);
    }
    else
    {
        return (FALSE);
    }
}

BOOL PlayerInputSDown(void)
{
    if (gGameInput.SDownKeyPressed && !gGameInput.SDownKeyAlreadyPressed)
    {
        return(TRUE);
    }
    else
    {
        return (FALSE);
    }
}

BOOL PlayerInputDRight(void)
{
    if (gGameInput.DRightKeyPressed && !gGameInput.DRightKeyAlreadyPressed)
    {
        return(TRUE);
    }
    else
    {
        return (FALSE);
    }
}

BOOL PlayerInputEKey(void)
{
    if (gGameInput.EKeyPressed && !gGameInput.EKeyAlreadyPressed)
    {
        return(TRUE);
    }
    else
    {
        return (FALSE);
    }
}

BOOL PlayerInputEscape(void)
{
    if (gGameInput.EscapeKeyPressed && !gGameInput.EscapeKeyAlreadyPressed)
    {
        return(TRUE);
    }
    else
    {
        return (FALSE);
    }
}


void GoToDestGamestate(GAMESTATE destination)
{
    //store destination
    gDestinationGameState = destination;

    //store where we just were
    gPreviousGameState = gCurrentGameState;

    //transition
    gCurrentGameState = gDestinationGameState;
}

void GoToPrevGamestate(void)
{
    //store destination
    gDestinationGameState = gPreviousGameState;

    //store where we just were
    gPreviousGameState = gCurrentGameState;

    //transition
    gCurrentGameState = gDestinationGameState;
}

MENUITEM* AllocatePtrToMenuObj(_In_ MENU menu)
{
    switch (gActiveMenus)
    {
        case 0:
        {
            return(gMenuItemPtr0);
        }
        case 1:
        {
            return(gMenuItemPtr1);
        }
        case 2:
        {
            return(gMenuItemPtr2);
        }
        case 3:
        {
            return(gMenuItemPtr3);
        }
        case 4:
        {
            return(gMenuItemPtr4);
        }
        case 5:
        {
            return(gMenuItemPtr5);
        }
        case 6:
        {
            return(gMenuItemPtr6);
        }
        case 7:
        {
            return(gMenuItemPtr7);
        }
        case 8:
        {
            return(gMenuItemPtr8);
        }
        case 9:
        {
            return(gMenuItemPtr9);
        }
        case 10:
        {
            return(gMenuItemPtr10);
        }
        case 11:
        {
            return(gMenuItemPtr11);
        }
        case 12:
        {
            return(gMenuItemPtr12);
        }
        case 13:
        {
            return(gMenuItemPtr13);
        }
        case 14:
        {
            return(gMenuItemPtr14);
        }
        case 15:
        {
            return(gMenuItemPtr15);
        }
    }
}

uint8_t FindCurrentMenu(void)
{
    MENU menu[MAX_MENUS];

    for (uint8_t currentmenu = 0, next = 0; next < MAX_MENUS; next++)
    {
        if (next == currentmenu)
        {
            next++;
            menu[currentmenu] = ReturnStoredMenuObj(currentmenu + 1);
            menu[next] = ReturnStoredMenuObj(next + 1);
        }
        else
        {
            menu[next] = ReturnStoredMenuObj(next + 1);
        }

        if (!menu[next].Active && menu[currentmenu].Active)
        {
            for (next = currentmenu; next < MAX_MENUS; next--)      //using integeroverflow to know when to stop storing objects, 2.. 1.. 0.. 255..
            {
                StoreMenuObj(menu[next], next + 1);
            }
            return(currentmenu);
        }
        currentmenu++;
    }
}

void QuitGame(void)
{
    SendMessageA(gGameWindow, WM_CLOSE, 0, 0);
}

void CreateYesNoMenu(void)
{
    MENUPALLET Colorpallet = { .border1 = COLOR_LIGHT_GRAY, .background1 = COLOR_NES_GRAY, .border2 = COLOR_DARK_GRAY, .background2 = NULL, .itemborder = COLOR_DARK_RED, .itembackground = NULL, .text = COLOR_NES_RED, .cursor = COLOR_FORREST_GREEN };
    MENU menu = CreateMenuObj((GAME_RES_WIDTH / 2) - 60, (GAME_RES_HEIGHT / 2) - 32, 80, 64, 64, 28, Colorpallet, &g6x7Font, MENU_BOX1x2);

    SetMenuStringBuffer("No");
    ModifyMenuItemData(menu, 0, MODIFYITEM_NAME, NULL);
    ModifyMenuItemData(menu, 0, MODIFYITEM_ENABLE, NULL);

    SetMenuStringBuffer("Yes");
    ModifyMenuItemData(menu, 1, MODIFYITEM_NAME, NULL);
    ModifyMenuItemData(menu, 1, MODIFYITEM_ENABLE, NULL);
    ModifyMenuItemData(menu, 1, MODIFYITEM_ACTION, MENUFUNC_PREV_GS);

    StoreMenuObj(menu, NULL);

    gYesNoScreen = TRUE;
}

void DrawYesNoMenu(void)
{
    if (!gYesNoScreen)
    {
        MENU menu = ReturnStoredMenuObj(NULL);

        DrawMenu(menu/*, &COLOR_LIGHT_GRAY, &COLOR_NES_GRAY, &COLOR_DARK_GRAY, NULL, &COLOR_DARK_RED, NULL, &COLOR_NES_RED, &COLOR_FORREST_GREEN*/);

        StoreMenuObj(menu, NULL);
    }
}

void PPI_YesNoMenu(void)
{
    BOOL success;
    MENU menu = { NULL };
    int16_t player_input = WASDMenuNavigation(TRUE);
    uint8_t currentmenu = FindCurrentMenu();

    if (player_input < 0)
    {
        return;
    }

    if (gActiveMenus)
    {
        menu = ReturnStoredMenuObj(currentmenu + 1);

        if (PlayerInputEKey())
        {
            switch (menu.SelectedItem)
            {
                case 0:
                {
                    //no
                    break;
                }
                case 1:
                {
                    //yes
                    SelectMenuItemFromStoredMenu();
                    break;
                }
            }
            PlayGameSound(&gSoundMenuChoose);
            menu = ClearMenu();    //pop menu for any election
            gYesNoScreen = FALSE;
        }
        else if (PlayerInputEscape())
        {
            if (menu.SelectedItem == menu.Rows * menu.Columns - 1)
            {
                SelectMenuItemFromStoredMenu();
                PlayGameSound(&gSoundMenuChoose);
                menu = ClearMenu();    //pop menu
                gYesNoScreen = FALSE;
            }
            else
            {
                menu.SelectedItem = menu.Rows * menu.Columns - 1;     //yes button
                PlayGameSound(&gSoundMenuNavigate);
            }
        }

        if (menu.Active)
        {
            success = StoreMenuObj(PlayerInputToMenuObj(menu, player_input), currentmenu + 1);     //update menu based on player input
            ASSERT(success, "Too many menus in gMenuBuffer[]!");
        }
    }
}

void StoreSelectedMenuItem(uint8_t selecteditem, uint8_t menuindex, BOOL popmenu, _In_opt_ DWORD actionparam)
{
    gSelectedMenuItem = selecteditem;
    gMenuIndexBuffer = menuindex;
    gMenuItemActionParam = actionparam;
    gPopMenu = popmenu;
}

void SelectMenuItemFromStoredMenu(void)
{
    MENU menu = ReturnStoredMenuObj(gMenuIndexBuffer + 1);

    if (gMenuItemActionParam)
    {
        menu.Items[gSelectedMenuItem]->Action(gMenuItemActionParam);
    }
    else
    {
        menu.Items[gSelectedMenuItem]->Action();
    }

    if (!gPopMenu)
    {
        StoreMenuObj(menu, gMenuIndexBuffer + 1);
    }
}


