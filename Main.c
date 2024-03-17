#pragma warning(push, 3)
#include <stdio.h>
#include <windows.h>
#include <psapi.h>
#include <emmintrin.h>
#include <xaudio2.h>
#include <stdint.h>
//#include "stb_vorbis.h"
#include <string.h>
#pragma warning(pop)

#include "Main.h"

HWND gGameWindow;

BOOL gGameIsRunning;

GAMEBITMAP gBackBuffer;

GAMEBITMAP g6x7Font;
GAMEBITMAP gBackGroundGraphic;
//GAMEBITMAP gIcon;

GAME_PERFORMANCE_DATA gGamePerformanceData;

BOOL gWindowHasFocus;

IXAudio2* gXAudio;

IXAudio2MasteringVoice* gXAudioMasteringVoice;

uint8_t gSFXSourceVoiceSelector;

REGISTRYPARAMS gRegistryParams;

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

    if (LoadRegistryParameters() != ERROR_SUCCESS)
    {
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
    GetSystemTimeAsFileTime((FILETIME*)&gGamePerformanceData.PreviousSystemTime);



    if (GameIsAlreadyRunning() == TRUE)
    {
        LogMessageA(LL_WARNING, "[%s] Another instance is already running!", __FUNCTION__);
        MessageBoxA(NULL, "Another instance of this program is already running!", "Error!", MB_ICONERROR | MB_OK);
        goto Exit;
    }

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

    if ((Load32BppBitmapFromFile("Assets\\PixelFont(6x7).bmpx", &g6x7Font)) != ERROR_SUCCESS)
    {
        LogMessageA(LL_ERROR, "[%s] Loading PixelFont(6x7).bmpx failed!", __FUNCTION__);
        MessageBoxA(NULL, "Load32BppBitmapFromFile failed!", "Error!", MB_ICONERROR | MB_OK);
        goto Exit;
    }

    switch (gRegistryParams.Graphic)
    {
        case 0:
        {
            if ((Load32BppBitmapFromFile("Assets\\rescue_ace.bmpx", &gBackGroundGraphic)) != ERROR_SUCCESS)
            {
                LogMessageA(LL_ERROR, "[%s] Loading rescue_ace.bmpx failed!", __FUNCTION__);
                MessageBoxA(NULL, "Load32BppBitmapFromFile failed!", "Error!", MB_ICONERROR | MB_OK);
                goto Exit;
            }
            break;
        }
        case 1:
        {
            if ((Load32BppBitmapFromFile("Assets\\marine.bmpx", &gBackGroundGraphic)) != ERROR_SUCCESS)
            {
                LogMessageA(LL_ERROR, "[%s] Loading marine.bmpx failed!", __FUNCTION__);
                MessageBoxA(NULL, "Load32BppBitmapFromFile failed!", "Error!", MB_ICONERROR | MB_OK);
                goto Exit;
            }
            break;
        }
        case 2:
        {
            if ((Load32BppBitmapFromFile("Assets\\labyrinth.bmpx", &gBackGroundGraphic)) != ERROR_SUCCESS)
            {
                LogMessageA(LL_ERROR, "[%s] Loading labyrinth.bmpx failed!", __FUNCTION__);
                MessageBoxA(NULL, "Load32BppBitmapFromFile failed!", "Error!", MB_ICONERROR | MB_OK);
                goto Exit;
            }
            break;
        }
        case 3:
        {
            if ((Load32BppBitmapFromFile("Assets\\swordsoul.bmpx", &gBackGroundGraphic)) != ERROR_SUCCESS)
            {
                LogMessageA(LL_ERROR, "[%s] Loading swordsoul.bmpx failed!", __FUNCTION__);
                MessageBoxA(NULL, "Load32BppBitmapFromFile failed!", "Error!", MB_ICONERROR | MB_OK);
                goto Exit;
            }
            break;
        }
        case 4:
        {
            if ((Load32BppBitmapFromFile("Assets\\unchained.bmpx", &gBackGroundGraphic)) != ERROR_SUCCESS)
            {
                LogMessageA(LL_ERROR, "[%s] Loading unchained.bmpx failed!", __FUNCTION__);
                MessageBoxA(NULL, "Load32BppBitmapFromFile failed!", "Error!", MB_ICONERROR | MB_OK);
                goto Exit;
            }
            break;
        }
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

    memset(gBackBuffer.Memory, 0x00, GAME_DRAWING_AREA_MEMORY_SIZE);

    gGameIsRunning = TRUE;

    while (gGameIsRunning == TRUE) //basic message loop
    {
        QueryPerformanceCounter((LARGE_INTEGER*)&FrameStart);

        while (PeekMessageA(&Message, gGameWindow, 0, 0, PM_REMOVE))
        {
            DispatchMessageA(&Message);
        }
        ProcessPlayerInput();
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
        {   gGameIsRunning = FALSE;
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
    WindowClass.style = CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = MainWindowProc;
    WindowClass.cbClsExtra = 0;
    WindowClass.cbWndExtra = 0;
    WindowClass.hInstance = GetModuleHandleA(NULL);
    WindowClass.hIcon = (HICON)LoadImage( // returns a HANDLE so we have to cast to HICON
        NULL,             // hInstance must be NULL when loading from a file
        "Assets\\yugioh_icon.ico",   // the icon file name
        IMAGE_ICON,       // specifies that the file is an icon
        0,                // width of the image (we'll specify default later on)
        0,                // height of the image
        LR_LOADFROMFILE |  // we want to load a file (as opposed to a resource)
        LR_DEFAULTSIZE |   // default metrics based on the type (IMAGE_ICON, 32x32)
        LR_LOADTRANSPARENT
    );
    WindowClass.hIconSm = (HICON)LoadImage( // returns a HANDLE so we have to cast to HICON
        NULL,             // hInstance must be NULL when loading from a file
        "Assets\\yugioh_icon.ico",   // the icon file name
        IMAGE_ICON,       // specifies that the file is an icon
        0,                // width of the image (we'll specify default later on)
        0,                // height of the image
        LR_LOADFROMFILE |  // we want to load a file (as opposed to a resource)
        LR_DEFAULTSIZE |   // default metrics based on the type (IMAGE_ICON, 32x32)
        LR_LOADTRANSPARENT
    );
    WindowClass.hCursor = LoadCursorA(NULL, IDC_ARROW);
    WindowClass.hbrBackground = CreateSolidBrush(RGB(255, 255, 0));
    WindowClass.lpszMenuName = NULL;
    WindowClass.lpszClassName = GAME_NAME "_WINDOWCLASS";

    if (RegisterClassExA(&WindowClass) == 0)
    {
        Result = GetLastError();
        LogMessageA(LL_ERROR, "[%s] Window Registration Failed! RegisterClassExa Failed! Error 0x%08lx!", __FUNCTION__, Result);
        MessageBoxA(NULL, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    gGameWindow = CreateWindowExA(0, WindowClass.lpszClassName, "YGO LP Counter", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, NULL, NULL, GetModuleHandleA(NULL), NULL);

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

    gGamePerformanceData.MonitorWidth = gGamePerformanceData.MonitorInfo.rcMonitor.right - gGamePerformanceData.MonitorInfo.rcMonitor.left;
    gGamePerformanceData.MonitorHeight = gGamePerformanceData.MonitorInfo.rcMonitor.bottom - gGamePerformanceData.MonitorInfo.rcMonitor.top;

    if (SetWindowLongPtrA(gGameWindow, GWL_STYLE, (WS_OVERLAPPEDWINDOW | WS_VISIBLE) /*& ~WS_OVERLAPPEDWINDOW*/) == 0)
    {
        Result = GetLastError();
        LogMessageA(LL_ERROR, "[%s] SetWindowLongPtrA failed! Error 0x%08lx!", __FUNCTION__, Result);
        goto Exit;
    }

    if (SetWindowPos(gGameWindow,
        HWND_TOP,
        ((gGamePerformanceData.MonitorInfo.rcMonitor.right - gGamePerformanceData.MonitorInfo.rcMonitor.left) / 2) - (640 / 2),
        ((gGamePerformanceData.MonitorInfo.rcMonitor.bottom - gGamePerformanceData.MonitorInfo.rcMonitor.top) / 2) - (480 / 2),
        //gGamePerformanceData.MonitorInfo.rcMonitor.left,
        //gGamePerformanceData.MonitorInfo.rcMonitor.top,
        640,
        480,
        SWP_NOOWNERZORDER | SWP_FRAMECHANGED) == 0)
    {
        Result = GetLastError();
        LogMessageA(LL_ERROR, "[%s] SetWindowPos failed! Error 0x%08lx!", __FUNCTION__, Result);
        goto Exit;
    }

Exit:
    return(Result);
}

BOOL GameIsAlreadyRunning(void)
{
    HANDLE Mutex = NULL;

    Mutex = CreateMutexA(NULL, FALSE, "YGO_LP_Mutex");

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
    if (gWindowHasFocus == FALSE)
    {
        return;
    }

    int16_t EscapeKeyPressed = GetAsyncKeyState(VK_ESCAPE);
    int16_t DebugKeyPressed = GetAsyncKeyState(VK_F1);
    static int16_t DebugKeyAlreadyPressed;
    int16_t XKeyPressed = GetAsyncKeyState('X');
    static int16_t XKeyAlreadyPressed;
    int16_t HKeyPressed = GetAsyncKeyState('H');
    static int16_t HKeyAlreadyPressed;
    int16_t TabKeyPressed = GetAsyncKeyState(VK_TAB);
    static int16_t TabKeyAlreadyPressed;


    int16_t SDownKeyPressed = GetAsyncKeyState('S') | GetAsyncKeyState(VK_DOWN);
    static int16_t SDownKeyAlreadyPressed;
    int16_t ALeftKeyPressed = GetAsyncKeyState('A') | GetAsyncKeyState(VK_LEFT);       // WASD and ArrowKey movement
    static int16_t ALeftKeyAlreadyPressed;
    int16_t DRightKeyPressed = GetAsyncKeyState('D') | GetAsyncKeyState(VK_RIGHT);
    static int16_t DRightKeyAlreadyPressed;
    int16_t WUpKeyPressed = GetAsyncKeyState('W') | GetAsyncKeyState(VK_UP);
    static int16_t WUpKeyAlreadyPressed;
    int16_t DeleteKeyPressed = GetAsyncKeyState(VK_BACK) | GetAsyncKeyState(VK_DELETE);
    static int16_t DeleteKeyAlreadyPressed;
    int16_t ChooseKeyPressed = GetAsyncKeyState('E') | GetAsyncKeyState(VK_RETURN);
    static int16_t ChooseKeyAlreadyPressed;
    int16_t PlusMinusKeyPressed = GetAsyncKeyState(VK_OEM_PLUS) | GetAsyncKeyState(VK_OEM_MINUS) | GetAsyncKeyState(VK_ADD) | GetAsyncKeyState(VK_SUBTRACT);
    static int16_t PlusMinusKeyAlreadyPressed;


    int16_t OneKeyPressed = GetAsyncKeyState('1') | GetAsyncKeyState(VK_NUMPAD1);
    static int16_t OneKeyAlreadyPressed;
    int16_t TwoKeyPressed = GetAsyncKeyState('2') | GetAsyncKeyState(VK_NUMPAD2);
    static int16_t TwoKeyAlreadyPressed;
    int16_t ThreeKeyPressed = GetAsyncKeyState('3') | GetAsyncKeyState(VK_NUMPAD3);
    static int16_t ThreeKeyAlreadyPressed;
    int16_t FourKeyPressed = GetAsyncKeyState('4') | GetAsyncKeyState(VK_NUMPAD4);
    static int16_t FourKeyAlreadyPressed;
    int16_t FiveKeyPressed = GetAsyncKeyState('5') | GetAsyncKeyState(VK_NUMPAD5);
    static int16_t FiveKeyAlreadyPressed;
    int16_t SixKeyPressed = GetAsyncKeyState('6') | GetAsyncKeyState(VK_NUMPAD6);
    static int16_t SixKeyAlreadyPressed;
    int16_t SevenKeyPressed = GetAsyncKeyState('7') | GetAsyncKeyState(VK_NUMPAD7);
    static int16_t SevenKeyAlreadyPressed;
    int16_t EightKeyPressed = GetAsyncKeyState('8') | GetAsyncKeyState(VK_NUMPAD8);
    static int16_t EightKeyAlreadyPressed;
    int16_t NineKeyPressed = GetAsyncKeyState('9') | GetAsyncKeyState(VK_NUMPAD9);
    static int16_t NineKeyAlreadyPressed;
    int16_t ZeroKeyPressed = GetAsyncKeyState('0') | GetAsyncKeyState(VK_NUMPAD0);
    static int16_t ZeroKeyAlreadyPressed;

    if (EscapeKeyPressed)
    {
        SendMessageA(gGameWindow, WM_CLOSE, 0, 0);
    }
    if (DebugKeyPressed && !DebugKeyAlreadyPressed)
    {
        gGamePerformanceData.DisplayDebugInfo = !gGamePerformanceData.DisplayDebugInfo;
    }
    if (HKeyPressed && !HKeyAlreadyPressed)
    {
        gGamePerformanceData.DisplayControls = !gGamePerformanceData.DisplayControls;
    }

    


    DebugKeyAlreadyPressed = DebugKeyPressed;
    ALeftKeyAlreadyPressed = ALeftKeyPressed;
    DRightKeyAlreadyPressed = DRightKeyPressed;
    WUpKeyAlreadyPressed = WUpKeyPressed;
    SDownKeyAlreadyPressed = SDownKeyPressed;
    OneKeyAlreadyPressed = OneKeyPressed;
    TwoKeyAlreadyPressed = TwoKeyPressed;
    ThreeKeyAlreadyPressed = ThreeKeyPressed;
    FourKeyAlreadyPressed = FourKeyPressed;
    FiveKeyAlreadyPressed = FiveKeyPressed;
    SixKeyAlreadyPressed = SixKeyPressed;
    SevenKeyAlreadyPressed = SevenKeyPressed;
    EightKeyAlreadyPressed = EightKeyPressed;
    NineKeyAlreadyPressed = NineKeyPressed;
    ZeroKeyAlreadyPressed = ZeroKeyPressed;
    DeleteKeyAlreadyPressed = DeleteKeyPressed;
    ChooseKeyAlreadyPressed = ChooseKeyPressed;
    PlusMinusKeyAlreadyPressed = PlusMinusKeyPressed;
    XKeyAlreadyPressed = XKeyPressed;
    TabKeyAlreadyPressed = TabKeyPressed;
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
                            //  .. .. .. .. .. .. .. .. .. .. .. «  .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. »  .. .. .. ..
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
    //__m128i QuadPixel = { 0x3f, 0x00, 0x00, 0xff, 0x3f, 0x00, 0x00, 0xff, 0x3f, 0x00, 0x00, 0xff, 0x3f, 0x00, 0x00, 0xff };     //load 4 pixels worth of info
    //ClearScreenColor(&QuadPixel);

#else
    //PIXEL32 Pixel = { 0xff, 0x00, 0x00, 0xff };         //load 1 pixel
    //ClearScreenColor(&Pixel);
#endif

    Blit32BppBitmapToBuffer(&gBackGroundGraphic, 0, 0, 0);         ////main background grapic image

   

    if (gGamePerformanceData.DisplayDebugInfo == TRUE)
    {
        DrawDebugInfo();
    }

    if (gGamePerformanceData.DisplayControls)
    {

        DrawWindow(40, 165, 300, 51, &COLOR_NES_WHITE, &COLOR_BLACK, NULL, WINDOW_FLAG_BORDERED | WINDOW_FLAG_OPAQUE | WINDOW_FLAG_HORIZ_CENTERED);

        

    }
    else
    {

    }




    RECT WindowRect = { 0 };

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

    ReleaseDC(gGameWindow, DeviceContext);
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
        LogMessageA(LL_INFO, "[%s] Opened existig registry key HCKU\\SOFTWARE\\%s", __FUNCTION__, GAME_NAME);
    }

    /////////////////////////////

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

    ///////////////////////////

    Result = RegGetValueA(RegKey, NULL, "Graphic", RRF_RT_DWORD, NULL, (BYTE*)&gRegistryParams.Graphic, &RegBytesRead);

    if (Result != ERROR_SUCCESS)
    {
        if (Result == ERROR_FILE_NOT_FOUND)
        {
            Result = ERROR_SUCCESS;
            LogMessageA(LL_INFO, "[%s] Registry value 'Graphic' not found. Using default of 0. (RESCUE_ACE)", __FUNCTION__);
            gRegistryParams.Graphic = 0;
        }
        else
        {
            LogMessageA(LL_ERROR, "[%s] Failed to read the 'Graphic' registry value! Error 0x%08lx!", __FUNCTION__, Result);
            goto Exit;
        }
    }

    LogMessageA(LL_INFO, "[%s] Graphic is %d", __FUNCTION__, gRegistryParams.Graphic);

    ///////////////////////////



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

    //DWORD SFXVolume = (DWORD)gSFXVolume * 100.0f;

    //DWORD MusicVolume = (DWORD)gMusicVolume * 100.0f;

    Result = RegCreateKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\" GAME_NAME, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &RegKey, &RegDisposition);

    if (Result != ERROR_SUCCESS)
    {
        //LogMessageA(LL_ERROR, "[%s] RegCreateKey failed with error code 0x%08lx!", __FUNCTION__, Result);

        goto Exit;
    }

    Result = RegSetValueExA(RegKey, "Graphic", 0, REG_DWORD, (const BYTE*)&gRegistryParams.Graphic, sizeof(DWORD));

    if (Result != ERROR_SUCCESS)
    {
        LogMessageA(LL_ERROR, "[%s] Failed to set 'Graphic' in registry! Error 0x%08lx!", __FUNCTION__, Result);
        goto Exit;
    }
    LogMessageA(LL_INFO, "[%s] Saved 'Graphic' in registry: %d. ", __FUNCTION__, gRegistryParams.Graphic);

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
        ASSERT(BackgroundColor != NULL, "WINDOW_FLAG_OPAQUE is set but BackgroundColor is NULL!");

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

/*void ChangeBackgroundGraphic(void)
{
    if (gRegistryParams.Graphic < NUM_GRAPHIC_ART - 1)
    {
        gRegistryParams.Graphic++;
    }
    else
    {
        gRegistryParams.Graphic = 0;
    }
    switch (gRegistryParams.Graphic)
    {
        case 0:
        {
            if ((Load32BppBitmapFromFile("Assets\\rescue_ace.bmpx", &gBackGroundGraphic)) != ERROR_SUCCESS)
            {
                LogMessageA(LL_ERROR, "[%s] Loading rescue_ace.bmpx failed!", __FUNCTION__);
                MessageBoxA(NULL, "Load32BppBitmapFromFile failed!", "Error!", MB_ICONERROR | MB_OK);
                goto Exit;
            }
            break;
        }
        case 1:
        {
            if ((Load32BppBitmapFromFile("Assets\\marine.bmpx", &gBackGroundGraphic)) != ERROR_SUCCESS)
            {
                LogMessageA(LL_ERROR, "[%s] Loading marine.bmpx failed!", __FUNCTION__);
                MessageBoxA(NULL, "Load32BppBitmapFromFile failed!", "Error!", MB_ICONERROR | MB_OK);
                goto Exit;
            }
            break;
        }
        case 2:
        {
            if ((Load32BppBitmapFromFile("Assets\\labyrinth.bmpx", &gBackGroundGraphic)) != ERROR_SUCCESS)
            {
                LogMessageA(LL_ERROR, "[%s] Loading labyrinth.bmpx failed!", __FUNCTION__);
                MessageBoxA(NULL, "Load32BppBitmapFromFile failed!", "Error!", MB_ICONERROR | MB_OK);
                goto Exit;
            }
            break;
        }
        case 3:
        {
            if ((Load32BppBitmapFromFile("Assets\\swordsoul.bmpx", &gBackGroundGraphic)) != ERROR_SUCCESS)
            {
                LogMessageA(LL_ERROR, "[%s] Loading swordsoul.bmpx failed!", __FUNCTION__);
                MessageBoxA(NULL, "Load32BppBitmapFromFile failed!", "Error!", MB_ICONERROR | MB_OK);
                goto Exit;
            }
            break;
        }
        case 4:
        {
            if ((Load32BppBitmapFromFile("Assets\\unchained.bmpx", &gBackGroundGraphic)) != ERROR_SUCCESS)
            {
                LogMessageA(LL_ERROR, "[%s] Loading unchained.bmpx failed!", __FUNCTION__);
                MessageBoxA(NULL, "Load32BppBitmapFromFile failed!", "Error!", MB_ICONERROR | MB_OK);
                goto Exit;
            }
            break;
        }
    }

    SaveRegistryParameters();

Exit:

    return(0);
}*/