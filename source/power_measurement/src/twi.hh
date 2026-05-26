#pragma once

#include <avr/io.h>
#include <util/twi.h>

namespace twi {

template <uint32_t freq> constexpr uint8_t twbr_for_freq() {
  constexpr uint32_t RESULT = (F_CPU / freq - 16) / 2;
  static_assert(RESULT <= UINT8_MAX);
  return RESULT;
}

constexpr uint8_t twi_write_addr(uint8_t slave_addr) { return slave_addr << 1; }

constexpr uint8_t twi_read_addr(uint8_t slave_addr) {
  return (slave_addr << 1) | 1;
}

inline void twi_init() {
  TWSR = 0x00;
  TWBR = twbr_for_freq<400000>(); // 100 kbps
  // Enable pull-up resistors
  DDRD |= (1 << DDD1) | (1 << DDD0);
  PORTD |= (1 << PD1) | (1 << PD0);
}

inline void twi_start() { TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTA); }

inline void twi_send_byte() { TWCR = (1 << TWINT) | (1 << TWEN); }

inline void twi_receive_byte(bool is_last) {
  TWCR = (1 << TWINT) | (1 << TWEN) | ((uint8_t)(!is_last) << TWEA);
}

inline void twi_stop() { TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO); }

inline void twi_wait() {
  while (!(TWCR & (1 << TWINT)))
    ;
}

inline uint8_t twi_get_status() { return (TWSR & ~0x03); }

inline uint8_t twi_read(uint8_t addr, uint8_t in[], uint8_t in_len) {
  uint8_t status;
  for (uint8_t retry = 0; retry < 10; retry++) {
    twi_start();
    twi_wait();
    if ((status = twi_get_status()) == TW_MR_ARB_LOST)
      continue;
    if ((status != TW_START) && (status != TW_REP_START))
      break;
    TWDR = twi_read_addr(addr);
      twi_send_byte();
    twi_wait();
    if ((status = twi_get_status()) == TW_MR_ARB_LOST)
      continue;
    if (status != TW_MR_SLA_ACK)
      break;
    for (uint8_t i = 0; i < in_len; i++) {
      twi_receive_byte(false);
      twi_wait();
      if ((status = twi_get_status()) == TW_MR_ARB_LOST)
        goto continue_outer;
      if (status != TW_MR_DATA_ACK)
        goto break_outer;
      in[i] = TWDR;
    }
    twi_stop();
    return 0;
  continue_outer:;
  }
break_outer:
  twi_stop();
  return status;
}

inline uint8_t twi_write(uint8_t addr, const uint8_t out[], uint8_t out_len) {
  uint8_t status;
  for (uint8_t retry = 0; retry < 10; retry++) {
    twi_start();
    twi_wait();
    if ((status = twi_get_status()) == TW_MT_ARB_LOST)
      continue;
    if ((status != TW_START) && (status != TW_REP_START))
      break;
    TWDR = twi_write_addr(addr);
    twi_send_byte();
    twi_wait();
    if ((status = twi_get_status()) == TW_MT_ARB_LOST)
      continue;
    if (status != TW_MT_SLA_ACK)
      break;
    for (uint8_t i = 0; i < out_len; i++) {
      TWDR = out[i];
      twi_send_byte();
      twi_wait();
      if ((status = twi_get_status()) == TW_MT_ARB_LOST)
        goto continue_outer;
      if (status != TW_MT_DATA_ACK)
        goto break_outer;
    }
    twi_stop();
    return 0;
  continue_outer:;
  }
break_outer:
  twi_stop();
  return status;
}

inline uint8_t twi_read_write(uint8_t addr, const uint8_t out[],
                              uint8_t out_len, uint8_t in[], uint8_t in_len) {
  uint8_t status;
  for (uint8_t retry = 0; retry < 10; retry++) {
    if (out_len) {
      twi_start();
      twi_wait();
      if ((status = twi_get_status()) == TW_MT_ARB_LOST)
        continue;
      if ((status != TW_START) && (status != TW_REP_START))
        break;
      TWDR = twi_write_addr(addr);
      twi_send_byte();
      twi_wait();
      if ((status = twi_get_status()) == TW_MT_ARB_LOST)
        continue;
      if (status != TW_MT_SLA_ACK)
        break;
      for (uint8_t i = 0; i < out_len; i++) {
        TWDR = out[i];
        twi_send_byte();
        twi_wait();
        if ((status = twi_get_status()) == TW_MT_ARB_LOST)
          goto continue_outer;
        if (status != TW_MT_DATA_ACK)
          goto break_outer;
      }
    }
    if (in_len) {
      twi_start();
      twi_wait();
      if ((status = twi_get_status()) == TW_MR_ARB_LOST)
        continue;
      if ((status != TW_START) && (status != TW_REP_START))
        break;
      TWDR = twi_read_addr(addr);
      twi_send_byte();
      twi_wait();
      if ((status = twi_get_status()) == TW_MR_ARB_LOST)
        continue;
      if (status != TW_MR_SLA_ACK)
        break;
      for (uint8_t i = 0; i < in_len; i++) {
        twi_receive_byte(false);
        twi_wait();
        if ((status = twi_get_status()) == TW_MR_ARB_LOST)
          goto continue_outer;
        if (status != TW_MR_DATA_ACK)
          goto break_outer;
        in[i] = TWDR;
      }
    }
    if (out_len || in_len) {
      twi_stop();
    }
    return 0;
  continue_outer:;
  }
break_outer:
  twi_stop();
  return status;
}

} // namespace twi
