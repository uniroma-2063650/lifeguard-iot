#pragma once

#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <optional>

struct CommPacketHeartData {
  std::optional<uint8_t> hr;
  std::optional<uint8_t> spo2;

  uint32_t pack() const {
    return hr.value_or(-1) | ((uint32_t)(spo2.value_or(-1) & 0x7F) << 8);
  }

  static CommPacketHeartData unpack(const uint32_t raw) {
    const uint8_t raw_hr = (uint8_t)raw;
    const uint8_t raw_spo2 = (uint8_t)((raw >> 8) & 0x7F);
    return {
        .hr = raw_hr == 0xFF ? std::optional<uint8_t>{} : raw_hr,
        .spo2 = raw_spo2 == 0x7F ? std::optional<uint8_t>{} : raw_spo2,
    };
  }
};

enum class CommPacketKind : uint8_t {
  HEART_DATA = 0,
};

template <typename T> union MaybeUninit {
  T value;
  MaybeUninit() {}
};

struct CommPacket {
  uint8_t room;
  uint8_t bed;
  CommPacketKind kind;
  union {
    CommPacketHeartData heart_data;
  };

  static constexpr size_t PACKED_SIZE = 26;

  // Packed representation (26 bits):
  // - bed: 2 bits (0-3)
  // - room: 8 bits (0-255)
  // - kind: 1 bit (0-1)
  // - data: 18 bits
  //   - CommPacketHeartData:
  //     - hr: 8 bits (0-254, 255 = invalid)
  //     - spo2: 7 bits (0-100, 127 = invalid)
  uint32_t pack() const {
    uint32_t raw_metadata =
        room | ((uint32_t)(bed & 3) << 8) | ((uint32_t)kind << 10);
    uint32_t raw_data;
    switch (kind) {
    case CommPacketKind::HEART_DATA:
      raw_data = heart_data.pack();
    }
    return raw_metadata | (raw_data << 11);
  }

  static CommPacket unpack(const uint32_t raw) {
    CommPacket packet = {.room = (uint8_t)raw,
                         .bed = (uint8_t)((raw >> 8) & 3),
                         .kind = (CommPacketKind)(raw >> 10),
                         .heart_data{}};
    uint32_t raw_data = raw >> 11;
    switch (packet.kind) {
    case CommPacketKind::HEART_DATA:
      packet.heart_data = CommPacketHeartData::unpack(raw_data);
    }
    return packet;
  }
};

struct Comm {
  TaskHandle_t getTask() const { return task; }
  QueueHandle_t getQueue() const { return queue; }

protected:
  static bool is_running;

  TaskHandle_t task = nullptr;
  QueueHandle_t queue = nullptr;
  bool is_heltec_v3;

  Comm() {}

  void start(bool is_heltec_v3, BaseType_t core, void (*run)(Comm *));
  void start_in_task();
};
