
#include "Main.h"

#include "MainMenu.h"



void DrawMainMenu(void)
{
    static uint64_t LocalFrameCounter;

    static uint64_t LastFrameSeen;

    //TODO: Fadein
    //static PIXEL32 Border1 = COLOR_NES_GRAY;
    //static PIXEL32 Border2 = COLOR_LIGHT_GRAY;
    static PIXEL32 TextColor = { 0xFF, 0xFF, 0xFF, 0xFF };
    static int16_t BrightnessAdjustment = -255;

    if ( gFade || gGamePerformanceData.TotalFramesRendered > LastFrameSeen + 1)
    {
        gFade = FALSE;
        LocalFrameCounter = 0;
        BrightnessAdjustment = -255;
        gInputEnabled = FALSE;
        MENU mainmenu = CreateMenuObj(16, 12, GAME_RES_WIDTH - 64, GAME_RES_HEIGHT - 24, 128, 48, &g6x7Font, MENU_BOX1x4);

        SetMenuStringBuffer("Play");
        ModifyMenuItemData(mainmenu, 0, MODIFYITEM_NAME, NULL);
        ModifyMenuItemData(mainmenu, 0, MODIFYITEM_ENABLE, NULL);
        ModifyMenuItemData(mainmenu, 0, MODIFYITEM_ACTION, MENUFUNC_GOTO_GS);

        SetMenuStringBuffer("Options");
        ModifyMenuItemData(mainmenu, 1, MODIFYITEM_NAME, NULL);
        ModifyMenuItemData(mainmenu, 1, MODIFYITEM_ENABLE, NULL);
        ModifyMenuItemData(mainmenu, 1, MODIFYITEM_ACTION, MENUFUNC_GOTO_GS);

        SetMenuStringBuffer("Save/Load");
        ModifyMenuItemData(mainmenu, 2, MODIFYITEM_NAME, NULL);
        ModifyMenuItemData(mainmenu, 2, MODIFYITEM_ENABLE, NULL);
        ModifyMenuItemData(mainmenu, 2, MODIFYITEM_ACTION, MENUFUNC_GOTO_GS);

        SetMenuStringBuffer("Quit");
        ModifyMenuItemData(mainmenu, 3, MODIFYITEM_NAME, NULL);
        ModifyMenuItemData(mainmenu, 3, MODIFYITEM_ENABLE, NULL);
        ModifyMenuItemData(mainmenu, 3, MODIFYITEM_ACTION, MENUFUNC_QUIT);

        StoreMenuObj(mainmenu, NULL);

        gInputEnabled = FALSE;
    }

    //TODO: fadein
    //ApplyFadeIn(LocalFrameCounter, COLOR_NES_WHITE, &TextColor, &BrightnessAdjustment);

    //ApplyFadeIn(LocalFrameCounter, COLOR_DARK_GRAY, &Border1, &BrightnessAdjustment);

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

    LocalFrameCounter++;
    LastFrameSeen = gGamePerformanceData.TotalFramesRendered;
}

void PPI_MainMenu(void)
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
                    menu[currentmenu].Items[menu[currentmenu].SelectedItem]->Action(GAMESTATE_PLAY);
                    break;
                }
                case 1:
                {
                    menu[currentmenu].Items[menu[currentmenu].SelectedItem]->Action(GAMESTATE_OPTIONS);
                    break;
                }
                case 2:
                {
                    menu[currentmenu].Items[menu[currentmenu].SelectedItem]->Action(GAMESTATE_SAVELOAD);
                    break;
                }
                case 3:
                {
                    menu[currentmenu].Items[menu[currentmenu].SelectedItem]->Action();
                    break;
                }
            }
            PlayGameSound(&gSoundMenuChoose);
            menu[currentmenu] = ClearMenu();    //pop menu for any statechange
        }

        if (PlayerInputEscape())
        {
            if (menu[currentmenu].SelectedItem == menu[currentmenu].Rows * menu[currentmenu].Columns - 1)
            {
                menu[currentmenu].Items[menu[currentmenu].SelectedItem]->Action();
                PlayGameSound(&gSoundMenuChoose);
            }
            else
            {
                menu[currentmenu].SelectedItem = menu[currentmenu].Rows * menu[currentmenu].Columns - 1;     //quit button
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

