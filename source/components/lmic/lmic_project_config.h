#include "sdkconfig.h"
#include <esp_log.h>

// project-specific definitions
#ifdef CONFIG_LMIC_REGION_eu868
#define CFG_eu868 CONFIG_LMIC_REGION_eu868
#endif
#ifdef CONFIG_LMIC_REGION_us915
#define CFG_us915 CONFIG_LMIC_REGION_us915
#endif
#ifdef CONFIG_LMIC_REGION_au915
#define CFG_au915 CONFIG_LMIC_REGION_au915
#endif
#ifdef CONFIG_LMIC_REGION_as923
#define CFG_as923 CONFIG_LMIC_REGION_as923
#endif
#ifdef CONFIG_LMIC_REGION_kr920
#define CFG_kr920 CONFIG_LMIC_REGION_kr920
#endif
#ifdef CONFIG_LMIC_REGION_in866
#define CFG_in866 CONFIG_LMIC_REGION_in866
#endif

#ifdef CONFIG_LMIC_COUNTRY_CODE_JP
#define LMIC_COUNTRY_CODE LMIC_COUNTRY_CODE_JP
#endif

#ifdef CONFIG_LMIC_RADIO_sx1276
#define CFG_sx1276_radio CONFIG_LMIC_RADIO_sx1276
#endif
#ifdef CONFIG_LMIC_RADIO_sx1261
#define CFG_sx1261_radio CONFIG_LMIC_RADIO_sx1261
#endif
#ifdef CONFIG_LMIC_RADIO_sx1262
#define CFG_sx1262_radio CONFIG_LMIC_RADIO_sx1262
#endif

#define LMIC_LOG_DEBUG_FN(...) ESP_LOGI("LMIC", ## __VA_ARGS__)
#define LMIC_LOG_TRACE_FN(...) ESP_LOGI("LMIC", ## __VA_ARGS__)
#define LMIC_DEBUG_LEVEL 1
#define LMIC_TRACE_LEVEL 1
#define LMIC_ENABLE_event_logging 1
