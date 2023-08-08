#include <coreinit/cache.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <coreinit/title.h>
#include <gx2/event.h>

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
static uint8_t stack[0x30000];

static int cl_wups_main(int argc, const char **argv)
{
  while (true)
  {
    //if (wups_settings.sync_method == CL_WUPS_SYNC_METHOD_TICKS)
      OSSleepTicks(OSNanosecondsToTicks(16666667));
    //else
    //  GX2WaitForVsync();

    cl_run();

#if CL_WUPS_DEBUG
    /* Display a notification whenever a rich value changes */
    char msg[1024];

    for (unsigned int i = 0; i < memory.note_count; i++)
    {
      cl_memnote_t *note = &memory.notes[i];

      if (!note->flags)
        continue;

      if (note->current.intval != note->previous.intval)
      {
        snprintf(msg, sizeof(msg), "Note %04X {%u} %u %u: %llu",
          note->address,
          note->key,
          note->type,
          note->pointer_passes,
          note->current.intval);
        NotificationModule_AddInfoNotification(msg);
      }
    }
#endif
  }

  return 0;
}

INITIALIZE_PLUGIN()
{
  NotificationModule_InitLibrary();
  curl_global_init(CURL_GLOBAL_ALL);
}

DEINITIALIZE_PLUGIN()
{
  NotificationModule_DeInitLibrary();
  curl_global_cleanup();
}

ON_APPLICATION_START()
{
  auto title_id = OSGetTitleID();
  auto title_type = title_id & 0xFFFFFFFF00000000;
  auto title_system = title_get_system(title_id);

  /* Ignore everything that's not a disc, eShop title, or demo */
  if ((title_type == 0x0005000000000000 || title_type == 0x0005000200000000) &&
      (title_system == CL_WUPS_TITLE_WII_U || title_system == CL_WUPS_TITLE_N64))
  {
    if (!cl_init(&title_id, sizeof(title_id), ""))
      cl_fe_display_message(CL_MSG_ERROR, "cl_init error");

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

ON_APPLICATION_REQUESTS_EXIT()
{
  if (session.ready)
    cl_network_post("close", NULL, NULL);
  //OSJoinThread(&thread, nullptr);
}
