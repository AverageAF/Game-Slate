
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
        //gInputEnabled = TRUE;

        MENUPALLET Colorpallet = { .border1 = COLOR_NES_GRAY, .background1 = COLOR_LIGHT_GRAY, .border2 = COLOR_DARK_GRAY, .background2 = NULL, .itemborder = COLOR_BLACK, .itembackground = NULL, .text = COLOR_NES_MAGENTA, .cursor = COLOR_NES_ORANGE };
        MENU mainmenu = CreateMenuObj(16, 12, GAME_RES_WIDTH - 64, GAME_RES_HEIGHT - 24, 128, 48, Colorpallet,&g6x7Font, MENU_BOX1x4);

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
            DrawMenu(MenuToDraw[currentmenu]/*, &COLOR_NES_GRAY, &COLOR_LIGHT_GRAY, &COLOR_DARK_GRAY, NULL, &COLOR_BLACK, NULL, &COLOR_NES_MAGENTA, &COLOR_NES_ORANGE*/);
            StoreMenuObj(MenuToDraw[currentmenu], currentmenu + 1);
        }
    }

    LocalFrameCounter++;
    LastFrameSeen = gGamePerformanceData.TotalFramesRendered;
}

void PPI_MainMenu(void)
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
        if (gYesNoScreen)
        {
            PPI_YesNoMenu();
            goto SkipMenuPPI;
        }

        menu = ReturnStoredMenuObj(currentmenu + 1);

        if (PlayerInputEKey())
        {
            switch (menu.SelectedItem)
            {
                case 0:
                {
                    menu.Items[menu.SelectedItem]->Action(GAMESTATE_PLAY);
                    menu = ClearMenu();    //pop menu for any statechange
                    break;
                }
                case 1:
                {
                    menu.Items[menu.SelectedItem]->Action(GAMESTATE_OPTIONS);
                    menu = ClearMenu();    //pop menu for any statechange
                    break;
                }
                case 2:
                {
                    menu.Items[menu.SelectedItem]->Action(GAMESTATE_SAVELOAD);
                    menu = ClearMenu();    //pop menu for any statechange
                    break;
                }
                case 3:
                {
                    StoreMenuObj(menu, currentmenu + 1);
                    StoreSelectedMenuItem(menu.SelectedItem, currentmenu, TRUE, NULL);
                    menu = ClearMenu();
                    CreateYesNoMenu();
                    break;
                }
            }
            PlayGameSound(&gSoundMenuChoose);
        }
        else if (PlayerInputEscape())
        {
            if (menu.SelectedItem == menu.Rows * menu.Columns - 1)
            {
                StoreMenuObj(menu, currentmenu + 1);
                StoreSelectedMenuItem(menu.SelectedItem, currentmenu, TRUE, NULL);
                menu = ClearMenu();
                CreateYesNoMenu();
                PlayGameSound(&gSoundMenuChoose);
            }
            else
            {
                menu.SelectedItem = menu.Rows * menu.Columns - 1;     //quit button
                PlayGameSound(&gSoundMenuNavigate);
            }
        }

        if (menu.Active)
        {
            success = StoreMenuObj(PlayerInputToMenuObj(menu, player_input), currentmenu + 1);     //update menu based on player input
            ASSERT(success, "Too many menus in gMenuBuffer[]!");
        }
    }

SkipMenuPPI:    //used when YES/NO screen pops up

    return;
}

