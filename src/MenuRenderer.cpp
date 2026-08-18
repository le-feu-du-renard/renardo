#include "MenuRenderer.h"
#include "UiTheme.h"
#include "fonts/MonoFonts.h"

namespace MenuRenderer
{

namespace
{

// The hint bar is the answer to a mismatch between the design mock-up and this
// machine. The mock-up navigates with five keys — MENU, up, down, OK, RETOUR —
// and spells them out along the bottom of every menu screen. The dryer has one
// rotary encoder with a click, so the band says what the knob does instead.
const char *HintFor(const MenuItem &item, bool editing)
{
  if (editing)
  {
    return "TOURNER regler - CLIC valider";
  }
  if (item.kind == MenuItemKind::kValue)
  {
    return "TOURNER naviguer - CLIC modifier";
  }
  return "TOURNER naviguer - CLIC ouvrir";
}

} // namespace

void Render(TftDisplay &display, MenuSystem &menu)
{
  if (!menu.IsOpen() || !menu.ConsumeDirty())
  {
    return;
  }

  const MenuPage *page = menu.GetCurrentPage();
  if (page == nullptr)
  {
    return;
  }

  TFT_eSPI &tft = display.GetTft();
  const int16_t width   = TftDisplay::kWidth;
  const int16_t header  = 24;
  const int16_t hint_y  = TftDisplay::kHintY;
  const int16_t hint_h  = TftDisplay::kHintH;
  const uint8_t rows    = MenuSystem::VisibleRows();
  const int16_t row_h   = (hint_y - header) / rows;
  const uint8_t scroll  = menu.GetScroll();
  const uint8_t cursor  = menu.GetCursor();
  const bool    editing = menu.IsEditing();

  // Drawn as a header strip plus one sprite per row rather than a single
  // full-screen canvas: 320x240 at 16 bits is 150 KB of heap for a page that is
  // mostly empty, and asking for that much in one block invites an allocation
  // failure once the heap has been in use for a while. Peak here is one row.

  {
    TFT_eSprite canvas(&tft);
    if (canvas.createSprite(width, header) != nullptr)
    {
      canvas.fillSprite(UiTheme::kPanelSunken);
      canvas.drawFastHLine(0, header - 1, width, UiTheme::kBorder);

      canvas.setFreeFont(&Mono14B);
      canvas.setTextColor(UiTheme::kAccent);
      canvas.setTextDatum(TL_DATUM);
      canvas.drawString(page->title, 8,
                        UiLayout::CapTop((header - 1) / 2, kMono14BBaseline,
                                         kMono14BCapHeight));

      if (editing)
      {
        canvas.setFreeFont(&Mono12B);
        canvas.setTextColor(UiTheme::kWarn);
        canvas.setTextDatum(TR_DATUM);
        canvas.drawString("EDITION", width - 8,
                          UiLayout::CapTop((header - 1) / 2, kMono12BBaseline,
                                           kMono12BCapHeight));
      }

      canvas.pushSprite(0, 0);
      canvas.deleteSprite();
    }
  }

  char value_text[24];
  for (uint8_t row = 0; row < rows; row++)
  {
    uint8_t index = static_cast<uint8_t>(scroll + row);
    int16_t y = header + row * row_h;

    TFT_eSprite canvas(&tft);
    if (canvas.createSprite(width, row_h) == nullptr)
    {
      continue;
    }
    canvas.fillSprite(UiTheme::kBackground);

    if (index >= page->count)
    {
      // Blank the leftover rows of a short page, otherwise the previous page
      // shows through underneath.
      canvas.pushSprite(0, y);
      canvas.deleteSprite();
      continue;
    }

    const MenuItem &item = page->items[index];
    bool selected  = (index == cursor);
    bool available = menu.IsItemSelectable(item);

    // The cursor is a filled, outlined pill rather than a solid bar across the
    // screen: it is the same shape as the cards on the dashboard, so the two
    // screens read as one interface.
    if (selected)
    {
      canvas.fillRoundRect(4, 1, width - 8, row_h - 2, 4, UiTheme::kAccentDim);
      canvas.drawRoundRect(4, 1, width - 8, row_h - 2, 4, UiTheme::kAccent);
    }

    uint16_t text_color = !available ? UiTheme::kMuted
                          : (selected ? UiTheme::kText : UiTheme::kMuted);

    // The selected row is set in the bold cut, so it stands out even for a
    // reader who cannot pick the highlight out at a distance. The two cuts were
    // generated at the same size and so share a cell, which is what lets one
    // baseline calculation serve both and keeps the rows from shifting by a
    // pixel as the cursor passes over them.
    canvas.setFreeFont(selected ? &Mono14B : &Mono14);
    canvas.setTextColor(text_color);
    canvas.setTextDatum(TL_DATUM);
    canvas.drawString(item.label, 12,
                      UiLayout::CapTop(row_h / 2, kMono14Baseline,
                                       kMono14CapHeight));

    menu.FormatItemValue(item, value_text, sizeof(value_text));
    if (value_text[0] != '\0')
    {
      // Amber while editing, so the row being changed is unmistakable; the
      // accent otherwise, matching the live figures on the dashboard.
      uint16_t value_color = !available ? UiTheme::kMuted
                             : (selected && editing) ? UiTheme::kWarn
                                                     : UiTheme::kAccent;

      canvas.setFreeFont(&Mono14B);
      canvas.setTextColor(value_color);
      canvas.setTextDatum(TR_DATUM);
      canvas.drawString(value_text, width - 14,
                        UiLayout::CapTop(row_h / 2, kMono14BBaseline,
                                         kMono14BCapHeight));
    }

    // Scroll indicator, clipped into whichever row it crosses.
    if (page->count > rows)
    {
      int16_t track_h = hint_y - header;
      int16_t thumb_h = (track_h * rows) / page->count;
      int16_t thumb_y = header + (track_h * scroll) / page->count;
      int16_t local_y = thumb_y - y;
      if (local_y < row_h && local_y + thumb_h > 0)
      {
        int16_t top    = local_y < 0 ? 0 : local_y;
        int16_t bottom = local_y + thumb_h;
        if (bottom > row_h)
        {
          bottom = row_h;
        }
        canvas.fillRect(width - 3, top, 3, bottom - top, UiTheme::kBorder);
      }
    }

    canvas.pushSprite(0, y);
    canvas.deleteSprite();
  }

  // The band between the last row and the hint bar, left over when the rows do
  // not divide the height exactly. Cleared so a taller previous page cannot
  // leave a stripe behind.
  {
    int16_t used = header + rows * row_h;
    if (used < hint_y)
    {
      tft.fillRect(0, used, width, hint_y - used, UiTheme::kBackground);
    }
  }

  {
    TFT_eSprite canvas(&tft);
    if (canvas.createSprite(width, hint_h) != nullptr)
    {
      canvas.fillSprite(UiTheme::kPanelSunken);
      canvas.drawFastHLine(0, 0, width, UiTheme::kBorder);

      const MenuItem &item = page->items[cursor < page->count ? cursor : 0];
      canvas.setFreeFont(&Mono12B);
      canvas.setTextColor(UiTheme::kMuted);
      canvas.setTextDatum(TC_DATUM);
      canvas.drawString(HintFor(item, editing), width / 2,
                        UiLayout::CapTop(hint_h / 2, kMono12BBaseline,
                                         kMono12BCapHeight));

      canvas.pushSprite(0, hint_y);
      canvas.deleteSprite();
    }
  }
}

} // namespace MenuRenderer
