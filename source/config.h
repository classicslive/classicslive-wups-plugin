#ifndef CL_WUPS_CONFIG_H
#define CL_WUPS_CONFIG_H

extern "C"
{
  #include <classicslive-integration/cl_types.h>
};

#define CL_WUPS_CONFIG_ENABLED "enabled"
#define CL_WUPS_CONFIG_NETWORK_NOTIFICATIONS "network_notifications"
#define CL_WUPS_CONFIG_SYNC_METHOD "sync_method"
#define CL_WUPS_CONFIG_USERNAME "username"
#define CL_WUPS_CONFIG_PASSWORD "password"
#define CL_WUPS_CONFIG_TOKEN "token"

#define CL_WUPS_SYNC_METHOD_TICKS 0
#define CL_WUPS_SYNC_METHOD_VSYNC 1

typedef struct cl_wups_settings_t
{
  bool enabled;
  bool network_notifications;
  int sync_method;
  cl_user_t user;
} cl_wups_settings_t;

void InitConfig(void);

extern cl_wups_settings_t wups_settings;

#endif
