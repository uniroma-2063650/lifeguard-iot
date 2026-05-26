#pragma once

#include <stdio.h>

#define LOGI(TAG, fmt, ...)                                                    \
  do {                                                                         \
    printf("[INFO][%4s] " fmt "\n", TAG, ##__VA_ARGS__);                       \
  } while (0);
#define LOGV(TAG, fmt, ...)                                                    \
  do {                                                                         \
    /* printf("[VRBS][%4s] " fmt "\n", TAG, ##__VA_ARGS__); */                 \
  } while (0);
