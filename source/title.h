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

  /* Native Wii U disc or digital download software */
  CL_WUPS_TITLE_WII_U,

  /* Virtual Console: Nintendo Entertainment System, Famicom */
  CL_WUPS_TITLE_NES,

  /* Virtual Console: Super Nintendo Entertainment System, Super Famicom */
  CL_WUPS_TITLE_SNES,

  /* Virtual Console: Nintendo 64 */
  CL_WUPS_TITLE_N64,

  /* Virtual Console: Wii */
  CL_WUPS_TITLE_WII,

  /* Virtual Console: Game Boy Advance */
  CL_WUPS_TITLE_GBA,

  /* Virtual Console: Nintendo DS */
  CL_WUPS_TITLE_NDS,

  /* Virtual Console: TurboGrafx-16, PC Engine */
  CL_WUPS_TITLE_TG16,

  /* Virtual Console: MSX */
  CL_WUPS_TITLE_MSX,

  /* Wii U system applications and applets. CL should not run here. */
  CL_WUPS_TITLE_SYSTEM,

  /* Wii U demos and downloadable trailers */
  CL_WUPS_TITLE_DEMO,

  CL_WUPS_TITLE_SIZE
};

/**
 * Returns whether or not the active foreground process is a Nintendo 64
 * Virtual Console game.
 */
bool title_is_n64(void);

unsigned title_get_system(uint64_t title_id);

#endif
