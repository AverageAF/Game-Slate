
#include "Main.h"

#include "OptionsScreen.h"

void DrawOptionsScreen(void)
{
    static uint64_t LocalFrameCounter;

    static uint64_t LastFrameSeen;

    static PIXEL32 Red = { 0x00, 0x00, 0xFF, 0xFF };
    static PIXEL32 Gray = { 0x50, 0x50, 0x50, 0x50 };
    static PIXEL32 TextColor = { 0xFF, 0xFF, 0xFF, 0xFF };

    if (gGamePerformanceData.TotalFramesRendered > LastFrameSeen + 1)
    {
        LocalFrameCounter = 0;
        MENU mainmenu = CreateMenuObj(16, 12, GAME_RES_WIDTH - 64, GAME_RES_HEIGHT - 24, 128, 48, &g6x7Font, MENU_BOX1x4);

        SetMenuStringBuffer("Screen");
        ModifyMenuItemData(mainmenu, 0, MODIFYITEM_NAME, NULL);
        ModifyMenuItemData(mainmenu, 0, MODIFYITEM_ENABLE, NULL);
        ModifyMenuItemData(mainmenu, 0, MODIFYITEM_ACTION, MENUFUNC_SCREENSCALE);

        SetMenuStringBuffer("Sound");
        ModifyMenuItemData(mainmenu, 1, MODIFYITEM_NAME, NULL);
        ModifyMenuItemData(mainmenu, 1, MODIFYITEM_ENABLE, NULL);
        ModifyMenuItemData(mainmenu, 1, MODIFYITEM_ACTION, MENUFUNC_SFX_VOL);

        SetMenuStringBuffer("Music");
        ModifyMenuItemData(mainmenu, 2, MODIFYITEM_NAME, NULL);
        ModifyMenuItemData(mainmenu, 2, MODIFYITEM_ENABLE, NULL);
        ModifyMenuItemData(mainmenu, 2, MODIFYITEM_ACTION, MENUFUNC_MUSIC_VOL);

        SetMenuStringBuffer("Back");
        ModifyMenuItemData(mainmenu, 3, MODIFYITEM_NAME, NULL);
        ModifyMenuItemData(mainmenu, 3, MODIFYITEM_ENABLE, NULL);
        ModifyMenuItemData(mainmenu, 3, MODIFYITEM_ACTION, MENUFUNC_PREV_GS);

        StoreMenuObj(mainmenu, NULL);

        gInputEnabled = FALSE;
    }

    ApplyFadeIn(LocalFrameCounter, COLOR_NES_WHITE, &TextColor, NULL);

    ApplyFadeIn(LocalFrameCounter, COLOR_DARK_GRAY, &Gray, NULL);

    __stosd(gBackBuffer.Memory, 0xFF000000, GAME_DRAWING_AREA_MEMORY_SIZE / sizeof(DWORD));

    MENU MenuToDraw[MAX_MENUS] = { NULL };

    for (uint8_t currentmenu = 0; currentmenu < MAX_MENUS; currentmenu++)
    {
        MenuToDraw[currentmenu] = ReturnStoredMenuObj(currentmenu + 1);
        if (MenuToDraw[currentmenu].Active == TRUE)
        {
            DrawMenu(MenuToDraw[currentmenu], &COLOR_NES_GRAY, &COLOR_LIGHT_GRAY, &COLOR_DARK_GRAY, NULL, &COLOR_BLACK, NULL, &COLOR_NES_MAGENTA, &COLOR_NES_ORANGE);
            StoreMenuObj(MenuToDraw[currentmenu], currentmenu + 1);
        }
    }

    for (uint8_t volume = 0, tenpercent = 0; volume <= 100; volume += 10)
    {
        if (tenpercent < 10)
        {
            if (volume < gSFXVolume)
            {
                BlitStringToBuffer("!", &g6x7Font, &COLOR_DARK_RED, 144 + (6 * tenpercent), 96);
            }
            else
            {
                BlitStringToBuffer(".", &g6x7Font, &COLOR_DARK_RED, 144 + (6 * tenpercent), 96);
            }
            if (volume < gMusicVolume)
            {
                BlitStringToBuffer("!", &g6x7Font, &COLOR_DARK_BLUE, 144 + (6 * tenpercent), 144);
            }
            else
            {
                BlitStringToBuffer(".", &g6x7Font, &COLOR_DARK_BLUE, 144 + (6 * tenpercent), 144);
            }
        }
        tenpercent++;
    }

    LocalFrameCounter++;
    LastFrameSeen = gGamePerformanceData.TotalFramesRendered;
}

void PPI_OptionsScreen(void)
{
    BOOL success;
    MENU menu[MAX_MENUS] = { NULL };
    int16_t player_input = WASDMenuNavigation(TRUE);
    uint8_t currentmenu = FindCurrentMenu();

    if (player_input < 0)
    {
        return;
    }

    if (gActiveMenus)
    {
        menu[currentmenu] = ReturnStoredMenuObj(currentmenu + 1);

        if (PlayerInputEKey())
        {
            switch (menu[currentmenu].SelectedItem)
            {
                case 0:
                {
                    menu[currentmenu].Items[menu[currentmenu].SelectedItem]->Action();
                    break;
                }
                case 1:
                {
                    menu[currentmenu].Items[menu[currentmenu].SelectedItem]->Action(10);
                    break;
                }
                case 2:
                {
                    menu[currentmenu].Items[menu[currentmenu].SelectedItem]->Action(10);
                    break;
                }
                case 3:
                {
                    menu[currentmenu].Items[menu[currentmenu].SelectedItem]->Action();
                    menu[currentmenu] = ClearMenu();    //pop menu
                    break;
                }
            }
            PlayGameSound(&gSoundMenuChoose);
        }

        if (PlayerInputEscape())
        {
            if (menu[currentmenu].SelectedItem == menu[currentmenu].Rows * menu[currentmenu].Columns - 1)
            {
                menu[currentmenu].Items[menu[currentmenu].SelectedItem]->Action();
                menu[currentmenu] = ClearMenu();    //pop menu when changing states
                PlayGameSound(&gSoundMenuChoose);
            }
            else
            {
                menu[currentmenu].SelectedItem = menu[currentmenu].Rows * menu[currentmenu].Columns - 1;     //back button
                PlayGameSound(&gSoundMenuNavigate);
            }
        }

        if (menu[currentmenu].Active)
        {
            success = StoreMenuObj(PlayerInputToMenuObj(menu[currentmenu], player_input), currentmenu + 1);
            ASSERT(success, "Too many menus in gMenuBuffer[]!");
        }
    }
}

void IncreaseSFXVolume(uint8_t percent)
{
    uint8_t scratch = 0;
    if (percent > 100)
    {
        percent = 10;
    }

    if (100 < gSFXVolume + percent)
    {
        gSFXVolume = 0;
    }
    else
    {
        gSFXVolume += percent;
    }

    float SFXVolume = gSFXVolume / 100.0f;

    for (uint8_t Counter = 0; Counter < NUMBER_OF_SFX_SOURCE_VOICES; Counter++)
    {
        gXAudioSFXSourceVoice[Counter]->lpVtbl->SetVolume(gXAudioSFXSourceVoice[Counter], SFXVolume, XAUDIO2_COMMIT_NOW);
    }

}

void DecreaseSFXVolume(uint8_t percent)
{
    int8_t scratch = 0;
    if (percent > 100)
    {
        percent = 10;
    }

    scratch = gSFXVolume - percent;
    if (0 > scratch)
    {
        gSFXVolume = 100 - percent;
    }
    else
    {
        gSFXVolume -= percent;
    }

    float SFXVolume = gSFXVolume / 100.0f;

    for (uint8_t Counter = 0; Counter < NUMBER_OF_SFX_SOURCE_VOICES; Counter++)
    {
        gXAudioSFXSourceVoice[Counter]->lpVtbl->SetVolume(gXAudioSFXSourceVoice[Counter], SFXVolume, XAUDIO2_COMMIT_NOW);
    }

}

void IncreaseMusicVolume(uint8_t percent)
{
    uint8_t scratch = 0;
    if (percent > 100)
    {
        percent = 10;
    }

    if (100 < gMusicVolume + percent)
    {
        gMusicVolume = 0;
    }
    else
    {
        gMusicVolume += percent;
    }

    float MusicVolume = gMusicVolume / 100.0f;

    gXAudioMusicSourceVoice->lpVtbl->SetVolume(gXAudioMusicSourceVoice, MusicVolume, XAUDIO2_COMMIT_NOW);
}

void DecreaseMusicVolume(uint8_t percent)
{
    int8_t scratch = 0;
    if (percent > 100)
    {
        percent = 10;
    }

    scratch = gMusicVolume - percent;
    if (0 > scratch)
    {
        gMusicVolume = 100 - percent;
    }
    else
    {
        gMusicVolume -= percent;
    }

    float MusicVolume = gMusicVolume / 100.0f;

    gXAudioMusicSourceVoice->lpVtbl->SetVolume(gXAudioMusicSourceVoice, MusicVolume, XAUDIO2_COMMIT_NOW);
}

void DecreaseScaleFactor(void)
{
    if (gGamePerformanceData.CurrentScaleFactor > 1)
    {
        gGamePerformanceData.CurrentScaleFactor--;
    }
    else
    {
        gGamePerformanceData.CurrentScaleFactor = gGamePerformanceData.MaxScaleFactor;
    }
    InvalidateRect(gGameWindow, NULL, TRUE);
}

void IncreaseScaleFactor(void)
{
    if (gGamePerformanceData.CurrentScaleFactor < gGamePerformanceData.MaxScaleFactor)
    {
        gGamePerformanceData.CurrentScaleFactor++;
    }
    else
    {
        gGamePerformanceData.CurrentScaleFactor = 1;
    }
    InvalidateRect(gGameWindow, NULL, TRUE);
}