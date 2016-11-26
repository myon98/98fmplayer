#ifndef MYON_FMDRIVER_H_INCLUDED
#define MYON_FMDRIVER_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>
#include "ppz8.h"

struct fmdriver_work {
  // set by driver, called by opna
  void (*driver_opna_interrupt)(struct fmdriver_work *work);
  void (*driver_deinit)(struct fmdriver_work *work);
  // driver internal
  void *driver;


  // set by opna, called by driver in the interrupt functions
  unsigned (*opna_readreg)(struct fmdriver_work *work, unsigned addr);
  void (*opna_writereg)(struct fmdriver_work *work, unsigned addr, unsigned data);
  uint8_t (*opna_status)(struct fmdriver_work *work, bool a1);
  void *opna;

  const struct ppz8_functbl *ppz8_functbl;
  struct ppz8 *ppz8;

  const char *title;
  // driver status
  // fm3ex part map
};

#endif // MYON_FMDRIVER_H_INCLUDED
