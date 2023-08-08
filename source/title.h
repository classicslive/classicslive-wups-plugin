#ifndef CL_WUPS_TITLE_H
#define CL_WUPS_TITLE_H

#include <cstdint>

#define CL_WUPS_N64_RAMPTR 0xF547F014

typedef struct
{
  uint64_t id;
  unsigned type;
} cl_wups_title_t;

enum
{
  CL_WUPS_TITLE_UNKNOWN = 0,

  CL_WUPS_TITLE_WII_U,
  CL_WUPS_TITLE_NES,
  CL_WUPS_TITLE_SNES,
  CL_WUPS_TITLE_N64,
  CL_WUPS_TITLE_WII,
  CL_WUPS_TITLE_GBA,
  CL_WUPS_TITLE_NDS,
  CL_WUPS_TITLE_TG16,
  CL_WUPS_TITLE_MSX
};

/**
 * Returns whether or not the active foreground process is a Nintendo 64
 * Virtual Console game.
 */
bool title_is_n64(void);

unsigned title_get_system(uint64_t title_id);

#endif
