#include <string_view>

#include <wups.h>
#include <wups/config/WUPSConfigItemBoolean.h>
#include <wups/config/WUPSConfigItemIntegerRange.h>
#include <wups/config/WUPSConfigItemMultipleValues.h>
#include <wups/storage.h>

extern "C"
{
  #include <classicslive-integration/cl_common.h>
  #include <classicslive-integration/cl_frontend.h>
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
      ((storageRes = WUPSStorageAPI::GetOrStoreDefault(CL_WUPS_CONFIG_NETWORK_NOTIFICATIONS, wups_settings.network_notifications, true)) != WUPS_STORAGE_ERROR_SUCCESS))
    DEBUG_FUNCTION_LINE_ERR("Failed to get or store defaults");

  if ((storageRes = WUPSStorageAPI::SaveStorage()) != WUPS_STORAGE_ERROR_SUCCESS)
    DEBUG_FUNCTION_LINE_ERR("Failed to save storage %s (%d)", WUPSStorageAPI::GetStatusStr(storageRes).data(), storageRes);
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

    /* Username */
    ConfigItemMultipleValuesPair username[] =
    {
      { 0, wups_settings.user.username },
    };
    WUPSConfigItemMultipleValues_AddToCategory(cat_login,
      "username",
      "Username",
      0,
      0,
      username,
      1,
      &multiple_values_cb);

    /* Password */
    char hidden_pw[sizeof(wups_settings.user.password)];
    for (unsigned i = 0; i < sizeof(hidden_pw); i++)
      hidden_pw[i] = wups_settings.user.password[i] ? '*' : '\0';
    ConfigItemMultipleValuesPair password[] =
    {
      { 0, hidden_pw },
    };
    WUPSConfigItemMultipleValues_AddToCategory(cat_login,
      "password",
      "Password",
      0,
      0,
      password,
      1,
      &multiple_values_cb);

    /* Language */
    ConfigItemMultipleValuesPair language[] =
    {
      { 0, wups_settings.user.language },
    };
    WUPSConfigItemMultipleValues_AddToCategory(cat_login,
      "language",
      "Language",
      0,
      0,
      language,
      1,
      &multiple_values_cb);

    WUPSConfigAPI_Category_AddCategory(root, cat_login);
  }
  catch (std::exception &e)
  {
    OSReport("Exception: %s\n", e.what());
    return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
  }

  return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
}
