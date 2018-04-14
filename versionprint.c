#!/usr/bin/tcc -run
#include "version.h"
#include <stdio.h>

int main(void) {
  puts(FMPLAYER_VERSION_STR);
  return 0;
}

