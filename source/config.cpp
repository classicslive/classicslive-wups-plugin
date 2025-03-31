#include <string_view>

#include <coreinit/title.h>
#include <wups.h>
#include <wups/config/WUPSConfigItemBoolean.h>
#include <wups/config/WUPSConfigItemIntegerRange.h>
#include <wups/config/WUPSConfigItemMultipleValues.h>
#include <wups/config/WUPSConfigItemStub.h>
#include <wups/storage.h>

#include <stdarg.h>

extern "C"
{
  #include <classicslive-integration/cl_common.h>
  #include <classicslive-integration/cl_frontend.h>
  #include <classicslive-integration/cl_main.h>
  #include <classicslive-integration/cl_memory.h>
};

#define DEBUG_FUNCTION_LINE_ERR(fmt, ...) OSReport("Error: %s:%d: " fmt "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)

#include "config.h"

cl_wups_settings_t wups_settings = { true, true, CL_WUPS_SYNC_METHOD_TICKS };

WUPS_USE_STORAGE("classicslive");

void bool_cb(ConfigItemBoolean *item, bool value)
{
  if (item && item->identifier)
  {
    bool *target = nullptr;
    WUPSStorageError res;

    if (std::string_view(item->identifier) == CL_WUPS_CONFIG_ENABLED)
    {
      target = &wups_settings.enabled;
      cl_fe_display_message(CL_MSG_INFO, value ?
        "The plugin will be enabled next time you load a game." :
        "The plugin will be disabled next time you load a game. Exit the game to close this session.");
    }
    else if (std::string_view(item->identifier) == CL_WUPS_CONFIG_NETWORK_NOTIFICATIONS)
      target = &wups_settings.network_notifications;
    else
      return;
    
    *target = value;
    if ((res = WUPSStorageAPI::Store(item->identifier, value)) != WUPS_STORAGE_ERROR_SUCCESS)
      DEBUG_FUNCTION_LINE_ERR("Failed to save storage %s (%d)", WUPSStorageAPI::GetStatusStr(res).data(), res);
  }
}

void multiple_values_cb(ConfigItemMultipleValues *item, unsigned value)
{
  if (item && item->identifier)
  {
    int *target = nullptr;
    WUPSStorageError res;

    if (std::string_view(item->identifier) == CL_WUPS_CONFIG_SYNC_METHOD)
      target = &wups_settings.sync_method;
    else
      return;

    *target = value;
    if ((res = WUPSStorageAPI::Store(item->identifier, value)) != WUPS_STORAGE_ERROR_SUCCESS)
      DEBUG_FUNCTION_LINE_ERR("Failed to save storage %s (%d)", WUPSStorageAPI::GetStatusStr(res).data(), res);
  }
}

WUPSConfigAPICallbackStatus ConfigMenuOpenedCallback(WUPSConfigCategoryHandle rootHandle);

void ConfigMenuClosedCallback(void)
{
  WUPSStorageError storageRes;

  if ((storageRes = WUPSStorageAPI::SaveStorage()) != WUPS_STORAGE_ERROR_SUCCESS)
    DEBUG_FUNCTION_LINE_ERR("Failed to close storage %s (%d)", WUPSStorageAPI::GetStatusStr(storageRes).data(), storageRes);
}

void InitConfig(void)
{
  WUPSConfigAPIOptionsV1 configOptions = { .name = "Classics Live" };
  WUPSStorageError storageRes;

  if (WUPSConfigAPI_Init(configOptions, ConfigMenuOpenedCallback, ConfigMenuClosedCallback) != WUPSCONFIG_API_RESULT_SUCCESS)
    DEBUG_FUNCTION_LINE_ERR("Failed to init config api");

  if (((storageRes = WUPSStorageAPI::GetOrStoreDefault(CL_WUPS_CONFIG_ENABLED, wups_settings.enabled, true)) != WUPS_STORAGE_ERROR_SUCCESS) ||
      ((storageRes = WUPSStorageAPI::GetOrStoreDefault(CL_WUPS_CONFIG_SYNC_METHOD, wups_settings.sync_method, CL_WUPS_SYNC_METHOD_TICKS)) != WUPS_STORAGE_ERROR_SUCCESS) ||
      ((storageRes = WUPSStorageAPI::GetOrStoreDefault(CL_WUPS_CONFIG_NETWORK_NOTIFICATIONS, wups_settings.network_notifications, true)) != WUPS_STORAGE_ERROR_SUCCESS) ||
      ((storageRes = WUPSStorageAPI_GetString(nullptr, CL_WUPS_CONFIG_USERNAME, wups_settings.user.username, sizeof(wups_settings.user.username), nullptr)) != WUPS_STORAGE_ERROR_SUCCESS) ||
      ((storageRes = WUPSStorageAPI_GetString(nullptr, CL_WUPS_CONFIG_PASSWORD, wups_settings.user.password, sizeof(wups_settings.user.password), nullptr)) != WUPS_STORAGE_ERROR_SUCCESS) ||
      ((storageRes = WUPSStorageAPI_GetString(nullptr, CL_WUPS_CONFIG_TOKEN, wups_settings.user.token, sizeof(wups_settings.user.token), nullptr)) != WUPS_STORAGE_ERROR_SUCCESS) ||
      ((storageRes = WUPSStorageAPI_GetString(nullptr, CL_WUPS_CONFIG_LANGUAGE, wups_settings.user.language, sizeof(wups_settings.user.language), nullptr)) != WUPS_STORAGE_ERROR_SUCCESS))
    DEBUG_FUNCTION_LINE_ERR("Failed to get or store defaults");

  if ((storageRes = WUPSStorageAPI::SaveStorage()) != WUPS_STORAGE_ERROR_SUCCESS)
    DEBUG_FUNCTION_LINE_ERR("Failed to save storage %s (%d)", WUPSStorageAPI::GetStatusStr(storageRes).data(), storageRes);
}

/**
 * Add to a config category a field meant for information only.
 */
static WUPSConfigAPIStatus cl_add_readonly(WUPSConfigCategoryHandle cat, const char *name, const char *value, ...)
{
  static unsigned dummy_iter = 0;
  char dummy_key[8];
  va_list args;
  char message[256];

  dummy_iter++;
  snprintf(dummy_key, sizeof(dummy_key), "dm%u", dummy_iter);

  va_start(args, value);
  vsnprintf(message, sizeof(message), value, args);
  va_end(args);

  ConfigItemMultipleValuesPair message_values[] = { { 0, message } };

  return WUPSConfigItemMultipleValues_AddToCategory(cat, dummy_key, name, 0, 0, message_values, 1, NULL);
}

WUPSConfigAPICallbackStatus ConfigMenuOpenedCallback(WUPSConfigCategoryHandle root)
{
  try
  {
    /**
     * ========================================================================
     * Classics Live settings submenu -- Settings that change how the plugin functions
     * ========================================================================
     */
    WUPSConfigCategoryHandle cat_settings;
    WUPSConfigAPICreateCategoryOptionsV1 cat_settings_options = { .name = "Settings" };
    WUPSConfigAPI_Category_Create(cat_settings_options, &cat_settings);

    /* Enable Classics Live */
    WUPSConfigItemBoolean_AddToCategory(cat_settings,
      CL_WUPS_CONFIG_ENABLED,
      "Enable Classics Live",
      true,
      wups_settings.enabled,
      &bool_cb);

    /* Show network notifications */
    WUPSConfigItemBoolean_AddToCategory(cat_settings,
      CL_WUPS_CONFIG_NETWORK_NOTIFICATIONS,
      "Show network notifications",
      true,
      wups_settings.network_notifications,
      &bool_cb);

    /* Sync method */
    ConfigItemMultipleValuesPair syncs[] =
    {
      { CL_WUPS_SYNC_METHOD_TICKS, "milliseconds" },
      { CL_WUPS_SYNC_METHOD_VSYNC, "v-sync" },
    };
    WUPSConfigItemMultipleValues_AddToCategory(cat_settings,
      CL_WUPS_CONFIG_SYNC_METHOD,
      "Sync method",
      CL_WUPS_SYNC_METHOD_TICKS,
      wups_settings.sync_method,
      syncs,
      2,
      &multiple_values_cb);

    WUPSConfigAPI_Category_AddCategory(root, cat_settings);

    /**
     * ========================================================================
     * Classics Live login information submenu -- Read-only information from the config
     * ========================================================================
     */
    WUPSConfigCategoryHandle cat_login;
    WUPSConfigAPICreateCategoryOptionsV1 cat_login_options = { .name = "Login information" };
    WUPSConfigAPI_Category_Create(cat_login_options, &cat_login);

    WUPSConfigItemStub_AddToCategory(cat_login, "Login info is stored in the 'classicslive.json' file on your SD Card.");
    WUPSConfigItemStub_AddToCategory(cat_login, "Please edit it in a text editor to change these values.");
    WUPSConfigItemStub_AddToCategory(cat_login, "For more information, visit:");
    WUPSConfigItemStub_AddToCategory(cat_login, "https://www.github.com/classicslive/classicslive-wups-plugin");

    /* Username */
    cl_add_readonly(cat_login, "Username", wups_settings.user.username);

    /* Password */
    char hidden_pw[sizeof(wups_settings.user.password)];
    for (unsigned i = 0; i < sizeof(hidden_pw); i++)
      hidden_pw[i] = wups_settings.user.password[i] ? '*' : '\0';
    cl_add_readonly(cat_login, "Password", hidden_pw);

    /* Language */
    cl_add_readonly(cat_login, "Language", wups_settings.user.language);

    WUPSConfigAPI_Category_AddCategory(root, cat_login);

    /**
     * ========================================================================
     * Classics Live session information submenu
     * ========================================================================
     */

    WUPSConfigCategoryHandle cat_session;
    WUPSConfigAPICreateCategoryOptionsV1 cat_session_options = { .name = "Session information" };
    WUPSConfigAPI_Category_Create(cat_session_options, &cat_session);

    /* Session information */
    if (session.ready)
    {
      char message[256];

      cl_add_readonly(cat_session, "Game name", session.game_name);
      
      snprintf(message, sizeof(message), "%u", session.game_id);
      cl_add_readonly(cat_session, "Game ID", message);

      cl_add_readonly(cat_session, "Checksum", session.checksum);
      
      snprintf(message, sizeof(message), "%08llX", OSGetTitleID());
      cl_add_readonly(cat_session, "Title ID", message);
    }
    else if (wups_settings.enabled)
      WUPSConfigItemStub_AddToCategory(cat_session, "Please start a compatible game to create a Classics Live session.");
    else
      WUPSConfigItemStub_AddToCategory(cat_session, "Please enable Classics Live and start a compatible game.");
      
    WUPSConfigAPI_Category_AddCategory(root, cat_session);

    /**
     * ========================================================================
     * Classics Live debug information submenu
     * ========================================================================
     */
#if CL_WUPS_DEBUG
    WUPSConfigCategoryHandle cat_debug;
    WUPSConfigAPICreateCategoryOptionsV1 cat_debug_options = { .name = "Debug information" };
    WUPSConfigAPI_Category_Create(cat_debug_options, &cat_debug);

    if (memory.region_count && memory.regions)
    {
      cl_add_readonly(cat_debug, "Memory host base", "0x%08X", memory.regions[0].base_host);
      cl_add_readonly(cat_debug, "Memory guest base", "0x%08X", memory.regions[0].base_guest);
      cl_add_readonly(cat_debug, "Memory size", "0x%08X (%u MB)", memory.regions[0].size, memory.regions[0].size >> 20);
    }

    WUPSConfigAPI_Category_AddCategory(root, cat_debug);
#endif
  }
  catch (std::exception &e)
  {
    OSReport("Exception: %s\n", e.what());
    return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
  }

  return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
}
