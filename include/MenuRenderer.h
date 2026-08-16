#ifndef MENU_RENDERER_H
#define MENU_RENDERER_H

#include "MenuSystem.h"
#include "TftDisplay.h"

// Draws a MenuSystem. Split out so the menu logic stays free of TFT_eSPI and
// remains testable on the host, the same way DisplayModel separates the main
// screen's contents from its rendering.
namespace MenuRenderer
{

// Redraws the page if anything moved since the last call.
void Render(TftDisplay &display, MenuSystem &menu);

} // namespace MenuRenderer

#endif // MENU_RENDERER_H
