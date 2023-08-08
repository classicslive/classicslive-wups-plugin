#ifndef CL_WUPS_CONFIG_H
#define CL_WUPS_CONFIG_H

#define CL_WUPS_CONFIG_ENABLED "enabled"
#define CL_WUPS_CONFIG_NETWORK_NOTIFICATIONS "network_notifications"
#define CL_WUPS_CONFIG_SYNC_METHOD "sync_method"

enum
{
  CL_WUPS_SYNC_METHOD_TICKS = 0,
  CL_WUPS_SYNC_METHOD_VSYNC
};

typedef struct
{
  bool enabled;
  bool network_notifications;
  unsigned sync_method;
} cl_wups_settings_t;

extern cl_wups_settings_t wups_settings;

#endif
