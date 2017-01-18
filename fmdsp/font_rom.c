#include "font.h"

static const void *font_rom_get(const struct fmdsp_font *font,
                                uint16_t jis, enum fmdsp_font_type type) {
  const uint8_t *fontdata = font->data;
  const uint8_t *fontptr;
  if (type == FMDSP_FONT_ANK) {
    fontptr =  fontdata + 0x800 + (jis<<4);
  } else {
    uint8_t row = jis >> 8;
    uint8_t cell = jis;
    fontptr = fontdata + (0x800 + (0x60*16*2*(row-0x20)) + (cell<<5));
    if (type == FMDSP_FONT_JIS_RIGHT) fontptr += 16;
  }
  if ((fontptr + (16*16/8)) > (fontdata + FONT_ROM_FILESIZE)) fontptr = 0;
  return fontptr;
}

void fmdsp_font_from_font_rom(struct fmdsp_font *font, const void *data) {
  font->data = data;
  font->width_half = 8;
  font->height = 16;
  font->get = font_rom_get;
}
