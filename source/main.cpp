#include <coreinit/cache.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <coreinit/title.h>
#include <gx2/event.h> // for v-sync
#include <proc_ui/procui.h> // for detecting home menu

#include <curl/curl.h>
#include <notifications/notifications.h>
#include <wups.h>

extern "C"
{
  #include <classicslive-integration/cl_common.h>
  #include <classicslive-integration/cl_frontend.h>
  #include <classicslive-integration/cl_main.h>
  #include <classicslive-integration/cl_memory.h>
  #include <classicslive-integration/cl_network.h>
  #include <classicslive-integration/cl_script.h>
};

#include "config.h"
#include "title.h"

WUPS_PLUGIN_NAME("Classics Live");
WUPS_PLUGIN_DESCRIPTION("Interfaces with Classics Live directly from the Wii U");
WUPS_PLUGIN_VERSION(GIT_VERSION);
WUPS_PLUGIN_AUTHOR("Keith Bourdon");
WUPS_PLUGIN_LICENSE("MIT");

static OSThread thread;
static bool paused = false;
static unsigned pause_frames = 0;
static uint8_t stack[0x30000];
static uint64_t title_id = 0;
static uint64_t title_type = 0;
static uint64_t title_system = 0;
static int error = 0;

static int cl_wups_main(int argc, const char **argv)
{
  /**
   * Wait for an arbitrary amount for the game to fully start.
   * This amount of time was tested for Nintendo 64 games to begin emulation.
   */
  OSSleepTicks(OSSecondsToTicks(10));

  if (title_system == CL_WUPS_TITLE_N64)
  {
    for (auto i = (uint32_t*)0x14000000; i < (uint32_t*)0x20000000; i++)
    {
      /* Magic used at the beginning of the N64 ROM header */
      if (*i == 0x80371240)
      {
        /* The ROM size is in memory 0x10 bytes behind the ROM */
        unsigned size = *(i - 4);

        if (!size)
          continue;
        else if (!cl_init(i, size, "Wii U"))
          cl_message(CL_MSG_ERROR, "cl_init error");

        break;
      }
    }
  }
  else if (title_system == CL_WUPS_TITLE_WII_U)
  {
    if (!cl_init(&title_id, sizeof(title_id), "Wii U"))
      cl_message(CL_MSG_ERROR, "cl_init error");
  }

  while (true)
  {
    /** @todo V-sync mode temporarily disabled */
    // if (wups_settings.sync_method == CL_WUPS_SYNC_METHOD_TICKS)
      OSSleepTicks(OSNanosecondsToTicks(16666667));
    // else
    //   GX2WaitForVsync();

    if (paused || error)
      continue;

    /** @todo HACK! Implement new endianness type for the N64 situation */
    if (title_system == CL_WUPS_TITLE_N64 && memory.region_count)
      memory.regions[0].endianness = CL_ENDIAN_BIG;

    if (pause_frames)
    {
      cl_update_memory();
      pause_frames--;
      continue;
    }
    
    cl_run();

#if CL_WUPS_DEBUG
    /* Display a notification whenever a rich value changes */
    for (unsigned int i = 0; i < memory.note_count; i++)
    {
      cl_memnote_t *note = &memory.notes[i];

      if (!note->flags)
        continue;

      if (note->current.intval.i64 != note->previous.intval.i64)
      {
        cl_message(CL_MSG_INFO, "Note %04X {%u} %u %u: %llu",
          note->address,
          note->key,
          note->type,
          note->pointer_passes,
          note->current.intval.i64);
      }
    }
#endif
  }

  return 0;
}

/**
 * Resume processing when the HOME Menu is closed.
 */
ON_ACQUIRED_FOREGROUND()
{
  paused = false;
}

/**
 * Pause processing when the HOME Menu is opened.
 */
ON_RELEASE_FOREGROUND()
{
  if (session.ready)
    paused = true;
}

/**
 * The following strings are printed to console when the Nintendo 64
 * emulator toggles the Virtual Console Menu, which we can monitor to pause
 * CL processing when needed.
 */
DECL_FUNCTION(void, OSReport, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  char buffer[512];
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  if (session.ready && title_type == CL_WUPS_TITLE_N64)
  {
    if (strstr(buffer, "trlEmuShellMenuOpen"))
    {
      cl_message(CL_MSG_DEBUG, "VC Menu opened. Pausing.");
      paused = true;
    }
    else if (strstr(buffer, "trlEmuShellMenuClose"))
    {
      cl_message(CL_MSG_DEBUG, "VC Menu closed. Unpausing.");
      paused = false;
      pause_frames = 15;
    }
  }

  real_OSReport(buffer);
}

WUPS_MUST_REPLACE(OSReport, WUPS_LOADER_LIBRARY_COREINIT, OSReport);

INITIALIZE_PLUGIN()
{
  NotificationModule_InitLibrary();
  if (curl_global_init(CURL_GLOBAL_ALL))
  {
    cl_fe_display_message(CL_MSG_ERROR, "Could not initialize network. "
                                        "Is CURLWrapperModule installed?");
    error = 1;
  }
  InitConfig();
}

DEINITIALIZE_PLUGIN()
{
  NotificationModule_DeInitLibrary();
  curl_global_cleanup();
}

ON_APPLICATION_START()
{
  if (!wups_settings.enabled || error)
    return;

  memset(&memory, 0, sizeof(memory));
  memset(&session, 0, sizeof(session));
  paused = false;

  title_id = OSGetTitleID();
  title_type = title_id & 0xFFFFFFFF00000000;

  /**
   * Ignore everything that's not a disc or eShop title.
   * If we want to support demos at some point, we can enable them here.
   */
  if (!(title_type == 0x0005000000000000)) /* || title_type == 0x0005000200000000)) */
    return;

  title_system = title_get_system(title_id);
  
  if (title_system == CL_WUPS_TITLE_WII_U || title_system == CL_WUPS_TITLE_N64)
  {
    OSMemoryBarrier();
    if (!OSCreateThread(&thread,
                        cl_wups_main,
                        0,
                        nullptr,
                        stack + sizeof(stack),
                        sizeof(stack),
                        31,
                        OS_THREAD_ATTRIB_AFFINITY_ANY))
      cl_fe_display_message(CL_MSG_ERROR, "Main thread error");
    else
    {
      OSSetThreadName(&thread, "Classics Live client");
      OSResumeThread(&thread);
    }
    OSMemoryBarrier();
  }
#if CL_WUPS_DEBUG
  else
    cl_fe_display_message(CL_MSG_ERROR, "Only Wii U or Nintendo 64 titles "
                                        "are supported.");
#endif
}

ON_APPLICATION_ENDS()
{
  if (session.ready)
    cl_network_post("close", NULL, NULL);
}
