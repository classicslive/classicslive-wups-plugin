#ifndef CL_WUPS_MAIN_H
#define CL_WUPS_MAIN_H

#include <stdint.h>

typedef struct cl_wups_state_t
{ 
  uint64_t title_id = 0;
  uint64_t title_type = 0;
  uint64_t title_system = 0;
  uint32_t title_version = 0;
  char title_name[128] = { 0 };
  void *rom_data = nullptr;
  unsigned rom_size = 0;
} cl_wups_state_t;

extern cl_wups_state_t wups_state;

#endif
