#ifndef HHUOS_FONTS_H
#define HHUOS_FONTS_H

#include "TerminalFont.h"
#include "TerminalFontSmall.h"
#include "Font.h"
#include "BdfFonts/SpleenFont12x24.h"
#include "BdfFonts/BdfFont.h"

namespace Util::Graphic::Fonts {

static Font TERMINAL_FONT = Font(8, 16, TERMINAL_FONT_DATA);
static Font TERMINAL_FONT_SMALL = Font(8, 8, TERMINAL_FONT_SMALL_DATA);
static BdfFont SPLEEN_FONT_12 = BdfFont(spleen_12.Width, spleen_12.Height, spleen_12.Bitmap, spleen_12.Index);

}

#endif
