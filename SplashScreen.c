
#include "Main.h"

#include "SplashScreen.h"

void DrawSplashScreen(void)
{
    static uint64_t LocalFrameCounter;

    static uint64_t LastFrameSeen;

    static PIXEL32 TextColor = { 0xFF, 0xFF, 0xFF, 0xFF };

    static BOOL Blink;

    if (WaitForSingleObject(gEssentialAssetsLoadedEvent, 0) != WAIT_OBJECT_0)
    {
        return;
    }

    if (gGamePerformanceData.TotalFramesRendered > LastFrameSeen + 1)
    {
        LocalFrameCounter = 0;
        gInputEnabled = FALSE;
    }

    if (gGamePerformanceData.TotalFramesRendered % 15 == 0)
    {
        Blink = !Blink;
    }

    __stosd(gBackBuffer.Memory, 0xFF000000, GAME_DRAWING_AREA_MEMORY_SIZE / sizeof(DWORD));

    if (Blink && (WaitForSingleObject(gAssetLoadingThreadHandle, 0) != WAIT_OBJECT_0))
    {
        //TODO:Draw better "Loading..." text
        BlitStringToBuffer("Loading...", &g6x7Font, &COLOR_DARK_GRAY, GAME_RES_WIDTH - (6 * 11), GAME_RES_HEIGHT - 8);
    }
    else
    {
        gInputEnabled = TRUE;
    }


    if (LocalFrameCounter >= 60)
    {
        if (LocalFrameCounter == 60)
        {
            TextColor.Colors.Red = 255;
            TextColor.Colors.Blue = 255;
            TextColor.Colors.Green = 255;
            PlayGameSound(&gSoundSplashScreen);
        }

        if ((LocalFrameCounter >= 120 && LocalFrameCounter < 150) && (LocalFrameCounter % 10 == 0))
        {
            TextColor.Colors.Red -= 63;
            TextColor.Colors.Blue -= 63;
            TextColor.Colors.Green -= 63;
        }

        if (LocalFrameCounter == 150)
        {
            TextColor.Colors.Red = 0;
            TextColor.Colors.Blue = 0;
            TextColor.Colors.Green = 0;
        }

        if (LocalFrameCounter > 160)
        {
            if (WaitForSingleObject(gAssetLoadingThreadHandle, 0) == WAIT_OBJECT_0)
            {
                DWORD ThreadExitCode = ERROR_SUCCESS;
                GetExitCodeThread(gAssetLoadingThreadHandle, &ThreadExitCode);

                if (ThreadExitCode != ERROR_SUCCESS)
                {
                    LogMessageA(LL_ERROR, "[%s] Asset loading thread failed with 0x%08lx!", __FUNCTION__, ThreadExitCode);

                    gGameIsRunning = FALSE;

                    MessageBoxA(gGameWindow, "Asset Loading Failed Check Log!", "Error", MB_OK | MB_ICONERROR);
                }
                GoToDestGamestate(GAMESTATE_MAINMENU);
            }
        }

        BlitStringToBuffer("A game by me :)", &g6x7Font, &TextColor, (GAME_RES_WIDTH / 2) - (6 * 16 / 2), 112);

        BlitStringToBuffer("Matthew Warriner", &g6x7Font, &TextColor, (GAME_RES_WIDTH / 2) - (6 * 17 / 2), 128);
    }



    LocalFrameCounter++;

    LastFrameSeen = gGamePerformanceData.TotalFramesRendered;
}


void PPI_SplashScreen(void)      //skip splash screen
{
    if (gGameInput.EscapeKeyPressed && !gGameInput.EscapeKeyAlreadyPressed ||
        gGameInput.EKeyPressed && !gGameInput.EKeyAlreadyPressed ||
        gGameInput.WUpKeyPressed && !gGameInput.WUpKeyAlreadyPressed ||
        gGameInput.DRightKeyPressed && !gGameInput.DRightKeyAlreadyPressed ||
        gGameInput.ALeftKeyPressed && !gGameInput.ALeftKeyAlreadyPressed ||
        gGameInput.SDownKeyPressed && !gGameInput.SDownKeyAlreadyPressed)
    {
        if (WaitForSingleObject(gAssetLoadingThreadHandle, 0) == WAIT_OBJECT_0)
        {
            DWORD ThreadExitCode = ERROR_SUCCESS;
            GetExitCodeThread(gAssetLoadingThreadHandle, &ThreadExitCode);

            if (ThreadExitCode != ERROR_SUCCESS)
            {
                GoToDestGamestate(GAMESTATE_MAINMENU);
            }
        }
    }
}