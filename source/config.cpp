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

#include "config.h"

cl_wups_settings_t wups_settings = { true, true, CL_WUPS_SYNC_METHOD_VSYNC };

WUPS_USE_STORAGE("cl_wups");

void bool_cb(ConfigItemBoolean *item, bool value)
{
  if (item && item->configId)
  {
    if (std::string_view(item->configId) == CL_WUPS_CONFIG_ENABLED)
    {
      wups_settings.enabled = value;
      cl_fe_display_message(CL_MSG_INFO, value ?
        "The plugin will be enabled next time you load a game." :
        "The plugin will be disabled next time you load a game. Exit the game to close this session.");
    }
    else if (std::string_view(item->configId) == CL_WUPS_CONFIG_NETWORK_NOTIFICATIONS)
      wups_settings.network_notifications = value;

    WUPS_StoreBool(nullptr, item->configId, value);
  }
}

void multiple_values_cb(ConfigItemMultipleValues *item, unsigned value)
{
  if (item && item->configId)
  {
    if (std::string_view(item->configId) == CL_WUPS_CONFIG_SYNC_METHOD)
      wups_settings.sync_method = value;

    WUPS_StoreInt(nullptr, item->configId, value);
  }
}

WUPS_GET_CONFIG()
{
  WUPS_OpenStorage();

  WUPSConfigHandle config;
  WUPSConfig_CreateHandled(&config, "Classics Live");

  WUPSConfigCategoryHandle setting;
  WUPSConfig_AddCategoryByNameHandled(config, "Settings", &setting);

  WUPSConfigItemBoolean_AddToCategoryHandled(
    config,
    setting,
    CL_WUPS_CONFIG_ENABLED,
    "Enable Classics Live",
    wups_settings.enabled,
    bool_cb);

  ConfigItemMultipleValuesPair sync_method[2];
  sync_method[0].value = CL_WUPS_SYNC_METHOD_TICKS;
  sync_method[0].valueName = (char*)"milliseconds";
  sync_method[1].value = CL_WUPS_SYNC_METHOD_VSYNC;
  sync_method[1].valueName = (char*)"v-sync";
  WUPSConfigItemMultipleValues_AddToCategoryHandled(
    config,
    setting,
    CL_WUPS_CONFIG_SYNC_METHOD,
    "Sync method",
    wups_settings.sync_method,
    sync_method,
    sizeof(sync_method) / sizeof(sync_method[0]),
    multiple_values_cb);

  WUPSConfigItemBoolean_AddToCategoryHandled(
    config,
    setting,
    CL_WUPS_CONFIG_NETWORK_NOTIFICATIONS,
    "Show network notifications",
    wups_settings.network_notifications,
    bool_cb);

  /* Set up the login info category. For now, it is read-only. */
  WUPSConfigCategoryHandle login;
  WUPSConfig_AddCategoryByNameHandled(config, "Login info", &login);
  cl_user_t user;
  cl_fe_user_data(&user, 0);

  /* Display the username */
  ConfigItemMultipleValuesPair username[1];
  username[0].value = 0;
  username[0].valueName = user.username;
  WUPSConfigItemMultipleValues_AddToCategoryHandled(
    config,
    login,
    "username",
    "Username",
    0,
    username,
    sizeof(username) / sizeof(username[0]),
    multiple_values_cb);

  /* Display the password (masked with asterisks) */
  ConfigItemMultipleValuesPair password[1];
  char hidden_pw[sizeof(user.password)];
  for (unsigned i = 0; i < sizeof(hidden_pw); i++)
    hidden_pw[i] = user.password[i] ? '*' : '\0';
  password[0].value = 0;
  password[0].valueName = hidden_pw;
  WUPSConfigItemMultipleValues_AddToCategoryHandled(
    config,
    login,
    "password",
    "Password",
    0,
    password,
    sizeof(password) / sizeof(password[0]),
    multiple_values_cb);

  /* Display the language */
  ConfigItemMultipleValuesPair language[1];
  language[0].value = 0;
  language[0].valueName = user.language;
  WUPSConfigItemMultipleValues_AddToCategoryHandled(
    config,
    login,
    "language",
    "Language",
    0,
    language,
    sizeof(language) / sizeof(language[0]),
    multiple_values_cb);

  return config;
}

WUPS_CONFIG_CLOSED()
{
  WUPS_CloseStorage();
}
