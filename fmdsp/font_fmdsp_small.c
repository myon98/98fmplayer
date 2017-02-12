#include "font.h"

#include "font_fmdsp_small_data.h"

static const void *fmdsp_font_get(const struct fmdsp_font *font,
                                  uint16_t addr, enum fmdsp_font_type type) {
  if (type != FMDSP_FONT_ANK) return 0;
  if (addr >> 8) return 0;
  return &fontdat[addr*6];
}

const struct fmdsp_font font_fmdsp_small = {
  fmdsp_font_get, 0, 5, 6
};
