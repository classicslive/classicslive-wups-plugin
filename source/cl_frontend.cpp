#include <cstring>

#include <coreinit/cache.h>
#include <coreinit/memory.h>
#include <coreinit/memorymap.h>
#include <coreinit/thread.h>

#include <curl/curl.h>
#include <notifications/notifications.h>
#include <wups.h>

#include "config.h"
#include "title.h"

extern "C"
{
  #include <classicslive-integration/cl_common.h>
  #include <classicslive-integration/cl_frontend.h>
  #include <classicslive-integration/cl_memory.h>
  #include <classicslive-integration/cl_network.h>
};

void cl_fe_display_message(unsigned level, const char *msg)
{
  switch (level)
  {
#if CL_WUPS_DEBUG
  case CL_MSG_DEBUG:
#endif
  case CL_MSG_INFO:
    //snprintf(new_msg, sizeof(new_msg), "\u2282\u221f %s", msg); //\ue02b
    NotificationModule_AddInfoNotification(msg);
    break;
  case CL_MSG_WARN:
  case CL_MSG_ERROR:
    //snprintf(new_msg, sizeof(new_msg), "\u2282\u221f %s", msg);
    NotificationModule_AddErrorNotification(msg);
  }
}

/**
 * @todo This will need to be more involved for proper Wii U Virtual Console
 * support. N64 and NDS are easy as long as there are no pointers, other
 * consoles will be more complicated. The simple implementation is perfect for
 * native Wii U games, though.
 */
bool cl_fe_install_membanks(void)
{
  cl_memory_region_t *region = nullptr;
  unsigned int data, size;

  if (memory.region_count)
    return true;
  else if (title_is_n64())
  {
    data = CL_WUPS_N64_RAMPTR;
    memory.regions = (cl_memory_region_t*)malloc(sizeof(cl_memory_region_t));
    region = &memory.regions[0];
    region->base_host = (void*)data;
    region->base_guest = 0x80000000;
    region->size = 8 * 1024 * 1024;
    snprintf(region->title, sizeof(region->title), "%s",
      "Vessel RDRAM");
    memory.region_count = 1;
  }
#if 0
  else if (title_is_nds())
  {
    data = *((uint32_t*)(0xf56139d8)) + 0x02000000; // values at beginning of addrspace should be filled with 0xdeadbeef
    memory.regions = (cl_memory_region_t*)malloc(sizeof(cl_memory_region_t));
    region = &memory.regions[0];
    region->base_host = (void*)data;
    region->base_guest = 0x02000000;
    region->size = 4 * 1024 * 1024;
    snprintf(region->title, sizeof(region->title), "%s",
      "Hachihachi Main RAM");
    memory.region_count = 1;
  }
#endif
  else if (OSGetForegroundBucketFreeArea(&data, &size) && data && size)
  {
    data = OSEffectiveToPhysical(data);
    memory.regions = (cl_memory_region_t*)malloc(sizeof(cl_memory_region_t));
    region = &memory.regions[0];
    region->base_host = (void*)data;
    region->base_guest = data;
    region->size = size;
    snprintf(region->title, sizeof(region->title), "%s",
      "CafeOS Foreground Bucket");
    memory.region_count = 1;
  }
  else
    return false;

  cl_message(CL_MSG_DEBUG, "%s : %04X at %p is %u MB",
    region->title,
    region->base_guest,
    region->base_host,
    region->size >> 20);

  return true;
}

const char* cl_fe_library_name(void)
{
  return "CafeOS";
}

typedef struct
{
  char data[2048];
  size_t size;
} cl_wups_network_chunk_t;

static size_t cl_wups_network_cb(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  cl_wups_network_chunk_t *chunk = (cl_wups_network_chunk_t*)userp;
  char *current_pos = &chunk->data[chunk->size];

  memcpy(current_pos, contents, realsize);
  chunk->size += realsize;
  chunk->data[chunk->size] = '\0';

  return realsize;
}

void cl_fe_network_post(const char *url, char *data,
  void(*callback)(cl_network_response_t))
{
  CURL *curl_handle;
  CURLcode response_code;
  cl_wups_network_chunk_t chunk;
  cl_network_response_t response;
  char user_agent[256];
  auto curl_version = curl_version_info(CURLVERSION_NOW);

  snprintf(user_agent, sizeof(user_agent), "cl_wups " GIT_VERSION " using curl/%s", curl_version->version);
  chunk.size = 0;

  curl_handle = curl_easy_init();
  curl_easy_setopt(curl_handle, CURLOPT_URL, CL_REQUEST_URL);
  curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, data);
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, cl_wups_network_cb); 
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void*)&chunk);
  curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, user_agent);
#if !CL_HAVE_SSL
  curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0L);
#endif
  response_code = curl_easy_perform(curl_handle);
  curl_easy_cleanup(curl_handle);

  if (response_code == CURLE_OK)
  {
    response.data = chunk.data;
    response.error_code = 0;
    response.error_msg = "";

    if (wups_settings.network_notifications)
      cl_fe_display_message(CL_MSG_INFO, response.data);
  }
  else
  {
    char msg[16];

    snprintf(msg, 16, "Error %03u", response_code);
    cl_fe_display_message(CL_MSG_ERROR, msg);

    response.data = NULL;
    response.error_code = response_code;
    response.error_msg = "Network error";
  }
  if (callback)
    callback(response);
}

typedef struct
{
  OSThread os_thread;
  uint8_t stack[0x20000];
  cl_task_t *task;
} cl_wups_thread_t;

static int cl_wups_thread_handler(int argc, const char **argv)
{
  auto *thread = ((cl_wups_thread_t*)argv);

  thread->task->handler(thread->task);
  thread->task->callback(thread->task);

  return thread->task->error ? 1 : 0;
}

void cl_fe_thread(cl_task_t *cl_task)
{
  auto *thread = (cl_wups_thread_t*)calloc(1, sizeof(cl_wups_thread_t));

  thread->task = cl_task;
  OSMemoryBarrier();
  if (!OSCreateThread(&thread->os_thread,
                      cl_wups_thread_handler,
                      1,
                      ((char*)thread),
                      thread->stack + sizeof(thread->stack),
                      sizeof(thread->stack),
                      31,
                      OS_THREAD_ATTRIB_AFFINITY_ANY))
    cl_fe_display_message(CL_MSG_ERROR, "Thread error");
  else
    OSResumeThread(&thread->os_thread);
  OSMemoryBarrier();
}

bool cl_fe_user_data(cl_user_t *user, unsigned index)
{
  CL_UNUSED(index);

  /* Login should be retrieved from storage before this is called */
  memcpy(user, &wups_settings.user, sizeof(cl_user_t));

  return true;
}

/* STUBBED -- WILL NOT IMPLEMENT */
void cl_fe_pause(void)
{
}

void cl_fe_unpause(void)
{
}
