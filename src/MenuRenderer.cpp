#include "MenuRenderer.h"
#include "UiTheme.h"

namespace MenuRenderer
{

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
  const int16_t height  = TftDisplay::kHeight;
  const int16_t header  = 30;
  const uint8_t rows    = MenuSystem::VisibleRows();
  const int16_t row_h   = (height - header) / rows;
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
      canvas.fillSprite(UiTheme::kPanel);
      canvas.setTextDatum(ML_DATUM);
      canvas.setTextColor(UiTheme::kSetpoint, UiTheme::kPanel);
      canvas.drawString(page->title, 8, header / 2, 4);

      if (editing)
      {
        canvas.setTextDatum(MR_DATUM);
        canvas.setTextColor(UiTheme::kWarning, UiTheme::kPanel);
        canvas.drawString("EDITION", width - 8, header / 2, 2);
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

    if (index >= page->count)
    {
      // Blank the leftover rows of a short page, otherwise the previous page
      // shows through underneath.
      canvas.fillSprite(UiTheme::kBackground);
      canvas.pushSprite(0, y);
      canvas.deleteSprite();
      continue;
    }

    const MenuItem &item = page->items[index];
    bool selected  = (index == cursor);
    bool available = menu.IsItemSelectable(item);

    // While editing, the selected row keeps a quieter background so the
    // highlighted value is what stands out, not the row itself.
    uint16_t background = UiTheme::kBackground;
    if (selected)
    {
      background = editing ? UiTheme::kPanel : UiTheme::kBorder;
    }
    canvas.fillSprite(background);

    uint16_t text_color = !available ? UiTheme::kInactive
                          : (selected ? UiTheme::kValue : UiTheme::kLabel);

    canvas.setTextDatum(ML_DATUM);
    canvas.setTextColor(text_color, background);
    canvas.drawString(item.label, 10, row_h / 2, 4);

    menu.FormatItemValue(item, value_text, sizeof(value_text));
    if (value_text[0] != '\0')
    {
      uint16_t value_color = !available ? UiTheme::kInactive
                             : (selected && editing) ? UiTheme::kWarning
                                                     : UiTheme::kSetpoint;
      canvas.setTextDatum(MR_DATUM);
      canvas.setTextColor(value_color, background);
      canvas.drawString(value_text, width - 12, row_h / 2, 4);
    }

    // Scroll indicator, clipped into whichever row it crosses.
    if (page->count > rows)
    {
      int16_t track_h = height - header;
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
}

} // namespace MenuRenderer
