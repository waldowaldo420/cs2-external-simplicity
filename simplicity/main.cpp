#include <Windows.h>
#include <iostream>
#include "game/game.h"
#include "render/overlay.h"
#include "menu/console.h"

int main()
{
    ::SetConsoleTitleA("simplicity");

    console::init();

    if (!game::g_game.initialize())
        return 1;

    render::g_overlay.run();

    return 0;
}