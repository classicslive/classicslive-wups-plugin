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
  unsigned int data, size;

  if (memory.bank_count)
    return true;

  if (title_is_n64())
  {
    cl_membank_t* bank;

    memory.banks = (cl_membank_t*)malloc(sizeof(cl_membank_t));
    bank = &memory.banks[0];
    bank->data = (uint8_t*)CL_WUPS_N64_RAMPTR;
    bank->start = 0x80000000;
    bank->size = 8 * 1024 * 1024;
    snprintf(bank->title, sizeof(bank->title), "%s",
             "CafeOS Vessel RDRAM");
    memory.bank_count = 1;

#if CL_WUPS_DEBUG
    cl_message(CL_MSG_INFO, "%s %04X %04X", bank->title, bank->start, bank->size);
#endif

    return true;
  }
  else if (OSGetForegroundBucketFreeArea(&data, &size) && data && size)
  {
    cl_membank_t* bank;

    data = OSEffectiveToPhysical(data);
    memory.banks = (cl_membank_t*)malloc(sizeof(cl_membank_t));
    bank = &memory.banks[0];
    memcpy(&bank->data, &data, sizeof(data));
    bank->start = data;
    bank->size = size;
    snprintf(bank->title, sizeof(bank->title), "%s",
             "CafeOS Foreground Bucket");
    memory.bank_count = 1;

#if CL_WUPS_DEBUG
    cl_message(CL_MSG_INFO, "%s %04X %04X", bank->title, bank->start, bank->size);
#endif

    return true;
  }
  else
    return false;
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

  memcpy(chunk->data, contents, realsize);
  chunk->data[realsize] = '\0';
  chunk->size = realsize;

  return realsize;
}

void cl_fe_network_post(const char *url, char *data,
  void(*callback)(cl_network_response_t))
{
  CURL *curl_handle;
  CURLcode response_code;
  cl_wups_network_chunk_t chunk;
  cl_network_response_t response;

  curl_handle = curl_easy_init();
  curl_easy_setopt(curl_handle, CURLOPT_URL, CL_REQUEST_URL);
  curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, data);
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, cl_wups_network_cb); 
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void*)&chunk);
  curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "cl_wups " GIT_VERSION " using curl/1.0");
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

  WUPS_OpenStorage();

  if (WUPS_GetString(nullptr, "language", user->language, sizeof(user->language)))
  {
    WUPS_StoreString(nullptr, "language", "en_US");
    snprintf(user->language, sizeof(user->language), "%s", "en_US");
  }
  if (WUPS_GetString(nullptr, "username", user->username, sizeof(user->username)) ||
      WUPS_GetString(nullptr, "password", user->password, sizeof(user->password)))
  {
    cl_message(CL_MSG_ERROR, "Please provide your Classics Live account credentials in "
                             "plugins/config/classicslive.json");
    return false;
  }

  WUPS_CloseStorage();

  return true;
}

/* STUBBED -- WILL NOT IMPLEMENT */
void cl_fe_pause(void)
{
}

void cl_fe_unpause(void)
{
}
