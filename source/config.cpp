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

WUPSConfigAPICallbackStatus ConfigMenuOpenedCallback(WUPSConfigCategoryHandle rootHandle)
{
  try
  {
    WUPSConfigCategory root = WUPSConfigCategory(rootHandle);

    /* Enable Classics Live */
    root.add(WUPSConfigItemBoolean::Create(
      CL_WUPS_CONFIG_ENABLED,
      "Enable Classics Live",
      true,
      wups_settings.enabled,
      &bool_cb));

    /* Show network notifications */
    root.add(WUPSConfigItemBoolean::Create(
      CL_WUPS_CONFIG_NETWORK_NOTIFICATIONS,
      "Show network notifications",
      true,
      wups_settings.network_notifications,
      &bool_cb));

    /* Sync method */
    constexpr WUPSConfigItemMultipleValues::ValuePair syncs[] =
    {
      { CL_WUPS_SYNC_METHOD_TICKS, "milliseconds" },
      { CL_WUPS_SYNC_METHOD_VSYNC, "v-sync" },
    };
    root.add(WUPSConfigItemMultipleValues::CreateFromValue(
      CL_WUPS_CONFIG_SYNC_METHOD,
      "Sync method",
      CL_WUPS_SYNC_METHOD_TICKS,
      wups_settings.sync_method,
      syncs,
      &multiple_values_cb));

    /* Username */
    WUPSConfigItemMultipleValues::ValuePair username[] =
    {
      { 0, wups_settings.user.username },
    };
    root.add(WUPSConfigItemMultipleValues::CreateFromValue(
      "username",
      "Username",
      0,
      0,
      username,
      &multiple_values_cb));

    /* Password */
    char hidden_pw[sizeof(wups_settings.user.password)];
    for (unsigned i = 0; i < sizeof(hidden_pw); i++)
      hidden_pw[i] = wups_settings.user.password[i] ? '*' : '\0';
    WUPSConfigItemMultipleValues::ValuePair password[] =
    {
      { 0, hidden_pw },
    };
    root.add(WUPSConfigItemMultipleValues::CreateFromValue(
      "password",
      "Password",
      0,
      0,
      password,
      &multiple_values_cb));

    /* Language */
    WUPSConfigItemMultipleValues::ValuePair language[] =
    {
      { 0, wups_settings.user.language },
    };
    root.add(WUPSConfigItemMultipleValues::CreateFromValue(
      "language",
      "Language",
      0,
      0,
      language,
      &multiple_values_cb));
  } catch (std::exception &e){
      OSReport("Exception: %s\n", e.what());
      return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
  }

  return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
}
