#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdint.h>

typedef struct
{
  uint8_t kb[256];
  uint8_t kb_mod[8];
  uint8_t mouse[8];
  uint8_t mouse_sensitivity_x;
  uint8_t mouse_sensitivity_y;
} bindings_t;

#endif