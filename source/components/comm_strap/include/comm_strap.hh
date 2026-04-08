#pragma once

#include "comm_common.hh"
#include "lmic/lmic.h"
#include <cassert>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

struct CommStrap : Comm {
  static CommStrap &init(const bool is_heltec_v3, const BaseType_t core) {
    assert(!is_running);
    instance.start(is_heltec_v3, core);
    return instance;
  }

  static CommStrap &get() {
    assert(is_running);
    return instance;
  }

  void sendPacket(CommPacket packet);

private:
  static CommStrap instance;

  CommStrap() {}

  void start(bool is_heltec_v3, BaseType_t core);
  static void run(Comm *args);
  static void process_event(void *args, ev_t event);
};
