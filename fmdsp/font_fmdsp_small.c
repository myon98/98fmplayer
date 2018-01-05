#include "font.h"

#include "font_fmdsp_small_data.h"
#include "font_fmdsp_medium_data.h"

static const void *fmdsp_font_get(const struct fmdsp_font *font,
                                  uint16_t addr, enum fmdsp_font_type type) {
  (void)font;
  if (type != FMDSP_FONT_ANK) return 0;
  if (addr >> 8) return 0;
  return &fontdat[addr*6];
}

const struct fmdsp_font font_fmdsp_small = {
  fmdsp_font_get, 0, 5, 6
};

static const void *fmdsp_font_medium_get(const struct fmdsp_font *font,
                                  uint16_t addr, enum fmdsp_font_type type) {
  (void)font;
  if (type != FMDSP_FONT_ANK) return 0;
  if (addr >> 8) return 0;
  return &fmdsp_medium_dat[addr*8];
}

const struct fmdsp_font font_fmdsp_medium = {
  fmdsp_font_medium_get, 0, 6, 8
};
