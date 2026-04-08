#pragma once

#include "comm_common.hh"
#include "lmic/lmic.h"
#include <cassert>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <optional>

struct CommHub : Comm {
  static CommHub &init(const bool is_heltec_v3, const BaseType_t core,
                       const uint8_t room) {
    assert(!is_running);
    instance.start(is_heltec_v3, core, room);
    return instance;
  }

  static CommHub &get() {
    assert(is_running);
    return instance;
  }

  std::optional<CommPacket> receivePacket(TickType_t timeout);

private:
  static CommHub instance;

  uint8_t room;

  CommHub() {}

  void start(bool is_heltec_v3, BaseType_t core, uint8_t room);
  static void run(Comm *args);
  static void process_event(void *args, ev_t event);
  static void process_rx(void *args, uint8_t port, const uint8_t *msg,
                         size_t msg_size);
};
