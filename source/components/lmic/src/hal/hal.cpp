#include "lmic_hal/hal.hh"
#include "lmic/lmic.h"
#include <cstdint>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_private/gpio.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <soc/gpio_sig_map.h>
#include <stdio.h>

using namespace ESP_IDF_LMIC;

HalBoardCallbacks HalBoardCallbacks::instance{};

// -----------------------------------------------------------------------------
// I/O
// -----------------------------------------------------------------------------

static HalConfig hal_config;
static TaskHandle_t task;
static lmic_hal_failure_handler_t *custom_hal_failure_handler = NULL;

static void lmic_hal_io_init() {
  ASSERT(hal_config.board.nss != UNUSED_PIN);
  ASSERT(hal_config.board.dio[0] != UNUSED_PIN);
#if (defined(CFG_sx1276_radio) || defined(CFG_sx1272_radio))
  ASSERT(hal_config.board.dio[1] != UNUSED_PIN ||
         hal_config.board.dio[2] != UNUSED_PIN);
#endif

  gpio_config_t config = {
      .pin_bit_mask = ((uint64_t)1 << hal_config.board.nss),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_ERROR_CHECK(gpio_output_disable(hal_config.board.nss));
  ESP_ERROR_CHECK(gpio_output_enable(hal_config.board.nss));
  ESP_ERROR_CHECK(gpio_config(&config));
  ESP_ERROR_CHECK(gpio_set_level(hal_config.board.nss, 1));

  if (hal_config.board.rxtx != UNUSED_PIN) {
    // initialize to RX
    gpio_config_t config = {
        .pin_bit_mask = ((uint64_t)1 << hal_config.board.rxtx),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&config));
    ESP_ERROR_CHECK(gpio_set_level(hal_config.board.rxtx,
                                   hal_config.board.rxtx_rx_active != 0));
  }
  if (hal_config.board.rst != UNUSED_PIN) {
    // initialize RST to floating
    gpio_config_t config = {
        .pin_bit_mask = ((uint64_t)1 << hal_config.board.rst),
        .mode = GPIO_MODE_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&config));
  }

  if (hal_config.board.busy != UNUSED_PIN) {
    gpio_config_t config = {
        .pin_bit_mask = ((uint64_t)1 << hal_config.board.busy),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&config));
  }

  LMIC_LOG_DEBUG("Board pins configured: NSS = %d, RX/TX = %d (active %d), RST "
                 "= %d, BUSY = %d",
                 hal_config.board.nss, hal_config.board.rxtx,
                 hal_config.board.rxtx_rx_active, hal_config.board.rst,
                 hal_config.board.busy);
}

// val == 1  => tx
void lmic_hal_pin_rxtx(u1_t val) {
  if (hal_config.board.rxtx != UNUSED_PIN)
    ESP_ERROR_CHECK(gpio_set_level(hal_config.board.rxtx,
                                   val != hal_config.board.rxtx_rx_active));
}

// set radio RST pin to given value (or keep floating!)
void lmic_hal_pin_rst(u1_t val) {
  if (hal_config.board.rst == UNUSED_PIN)
    return;

  if (val == 0 || val == 1) { // drive pin
    ESP_ERROR_CHECK(gpio_set_direction(hal_config.board.rst, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(hal_config.board.rst, val));
  } else { // keep pin floating
    ESP_ERROR_CHECK(
        gpio_set_direction(hal_config.board.rst, GPIO_MODE_DISABLE));
  }
  LMIC_LOG_DEBUG("RST %02X", val);
}

s1_t lmic_hal_getRssiCal(void) { return hal_config.board.rssi_cal; }

// -----------------------------------------------------------------------------
// Interrupts
// -----------------------------------------------------------------------------

static ostime_t interrupt_time[NUM_DIO] = {0};

void ESP_IDF_LMIC::lmic_hal_wake_task() { xTaskNotifyGive(task); }

void ESP_IDF_LMIC::lmic_hal_wake_task_from_isr() {
  BaseType_t higher_priority_task_woken;
  vTaskNotifyGiveFromISR(task, &higher_priority_task_woken);
  portYIELD_FROM_ISR(higher_priority_task_woken);
}

template <uint8_t dio_num> static void lmic_hal_dio_isr(void *arg) {
  if (interrupt_time[dio_num] == 0) {
    ostime_t now = lmic_hal_ticks();
    interrupt_time[dio_num] = now ? now : 1;
    ESP_IDF_LMIC::lmic_hal_wake_task();
  }
}

static const gpio_isr_t interrupt_fns[NUM_DIO] = {
    lmic_hal_dio_isr<0>, lmic_hal_dio_isr<1>, lmic_hal_dio_isr<2>};
static_assert(
    NUM_DIO == 3,
    "number of interrupts must be 3 for initializing interrupt_fns[]");

static void lmic_hal_interrupt_init() {
  for (uint8_t i = 0; i < NUM_DIO; ++i) {
    if (hal_config.board.dio[i] == UNUSED_PIN)
      continue;

    gpio_config_t config = {
        .pin_bit_mask = ((uint64_t)1 << hal_config.board.dio[i]),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&config));
    ESP_ERROR_CHECK(gpio_intr_enable(hal_config.board.dio[i]));
    ESP_ERROR_CHECK(
        gpio_isr_handler_add(hal_config.board.dio[i], interrupt_fns[i], NULL));
  }
}

void lmic_hal_processPendingIRQs() {
  uint8_t i;
  for (i = 0; i < NUM_DIO; ++i) {
    if (hal_config.board.dio[i] == UNUSED_PIN)
      continue;
    ostime_t fired_time = interrupt_time[i];
    if (fired_time) {
      interrupt_time[i] = 0;
      radio_irq_handler_v2(i, fired_time);
    }
  }
}

static uint8_t irqlevel = 0;

uint8_t lmic_hal_getIrqLevel(void) { return irqlevel; }

void lmic_hal_disableIRQs() {
  if (irqlevel++ == 0) {
    for (uint8_t i = 0; i < NUM_DIO; ++i) {
      if (hal_config.board.dio[i] == UNUSED_PIN)
        continue;
      ESP_ERROR_CHECK(gpio_intr_disable(hal_config.board.dio[i]));
    }
  }
}

void lmic_hal_enableIRQs() {
  if (--irqlevel == 0) {
    for (uint8_t i = 0; i < NUM_DIO; ++i) {
      if (hal_config.board.dio[i] == UNUSED_PIN)
        continue;
      ESP_ERROR_CHECK(gpio_intr_enable(hal_config.board.dio[i]));
    }
  }
}

// -----------------------------------------------------------------------------
// SPI
// -----------------------------------------------------------------------------

static spi_device_handle_t spi_device;

static void lmic_hal_spi_init() {
  spi_bus_config_t bus_config = {
      .mosi_io_num = hal_config.board.spi_mosi,
      .miso_io_num = hal_config.board.spi_miso,
      .sclk_io_num = hal_config.board.spi_sck,
      .data2_io_num = -1,
      .data3_io_num = -1,
      .data4_io_num = -1,
      .data5_io_num = -1,
      .data6_io_num = -1,
      .data7_io_num = -1,
      .data_io_default_level = false,
      .max_transfer_sz = 0,
      .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_SCLK |
               SPICOMMON_BUSFLAG_MISO | SPICOMMON_BUSFLAG_MOSI,
      .isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO,
      .intr_flags = 0};
  ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO));

  uint32_t spi_freq =
      hal_config.board.spi_freq ? hal_config.board.spi_freq : LMIC_SPI_FREQ;
  spi_device_interface_config_t dev_config = {
      .command_bits = 8,
      .address_bits = 0,
      .dummy_bits = 0,
      .mode = 0,
      .clock_source = SPI_CLK_SRC_DEFAULT,
      .duty_cycle_pos = 0,
      .cs_ena_pretrans = 0,
      .cs_ena_posttrans = 0,
      .clock_speed_hz = (int)spi_freq,
      .input_delay_ns = 0,
      .sample_point = SPI_SAMPLING_POINT_PHASE_0,
      .spics_io_num = -1,
      .flags = 0,
      .queue_size = 1,
      .pre_cb = nullptr,
      .post_cb = nullptr,
  };
  ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dev_config, &spi_device));
}

#if (defined(CFG_sx1261_radio) || defined(CFG_sx1262_radio))
bit_t lmic_hal_radio_spi_is_busy() {
  // SX126x uses BUSY pin
  return gpio_get_level(hal_config.board.busy);
}
#else
// supply a definition just in case, because the declaration is not conditional
bit_t lmic_hal_radio_spi_is_busy() { return false; }
#endif // (defined(CFG_sx1261_radio) || defined(CFG_sx1262_radio))

void lmic_hal_spi_write(u1_t cmd, const u1_t *buf, size_t len) {
  gpio_num_t nss = hal_config.board.nss;

  ESP_ERROR_CHECK(gpio_set_level(nss, 0));

  // SX126x modems use BUSY pin. Only interact with SPI when BUSY goes LOW
#if (defined(CFG_sx1261_radio) || defined(CFG_sx1262_radio))
  while (lmic_hal_radio_spi_is_busy())
    ;
#endif

  uint64_t buf_val = 0;
  for (int i = 0; i < len; i++) {
    buf_val <<= 8;
    buf_val |= buf[i];
  }
  LMIC_LOG_DEBUG("SPI write: cmd %02X len %02zX bytes %08llX (%p)", cmd, len, buf_val, buf);

  spi_transaction_t transaction = {
      .flags = 0,
      .cmd = cmd,
      .addr = 0,
      .length = len * 8,
      .rxlength = 0,
      .override_freq_hz = 0,
      .user = nullptr,
      .tx_buffer = buf,
      .rx_buffer = nullptr,
  };
  ESP_ERROR_CHECK(spi_device_transmit(spi_device, &transaction));

  ESP_ERROR_CHECK(gpio_set_level(nss, 1));
}

void lmic_hal_spi_read(u1_t cmd, u1_t *buf, size_t len) {
  gpio_num_t nss = hal_config.board.nss;

  ESP_ERROR_CHECK(gpio_set_level(nss, 0));

  // SX126x modems use BUSY pin. Only interact with SPI when BUSY goes LOW
#if (defined(CFG_sx1261_radio) || defined(CFG_sx1262_radio))
  while (lmic_hal_radio_spi_is_busy())
    ;
#endif

  LMIC_LOG_DEBUG("SPI read: cmd %02X len %02X", cmd, len);

  spi_transaction_t transaction = {
      .flags = 0,
      .cmd = cmd,
      .addr = 0,
      .length = len * 8,
      .rxlength = 0,
      .override_freq_hz = 0,
      .user = nullptr,
      .tx_buffer = nullptr,
      .rx_buffer = buf,
  };
  ESP_ERROR_CHECK(spi_device_transmit(spi_device, &transaction));

  ESP_ERROR_CHECK(gpio_set_level(nss, 1));
}

// SX126x modems behave slightly differently to SX127x. They will often need to
// transfer multiple bytes before reading
#if (defined(CFG_sx1261_radio) || defined(CFG_sx1262_radio))
void lmic_hal_spi_read_sx126x(u1_t cmd, u1_t *addr, size_t addr_len, u1_t *buf,
                              size_t buf_len) {
  gpio_num_t nss = hal_config.board.nss;

  ESP_ERROR_CHECK(gpio_set_level(nss, 0));

  while (lmic_hal_radio_spi_is_busy())
    ;

  uint64_t addr_int = 0;
  for (int i = 0; i < addr_len; i++) {
    addr_int <<= 8;
    addr_int |= addr[i];
  }

  LMIC_LOG_DEBUG("SPI SX126x read: cmd %02X addr %08" PRIX64
                 " (%02zu) read %02zu",
                 cmd, addr_int, addr_len, buf_len);

  spi_transaction_ext_t transaction = {.base =
                                           {
                                               .flags = SPI_TRANS_VARIABLE_ADDR,
                                               .cmd = cmd,
                                               .addr = addr_int,
                                               .length = buf_len * 8,
                                               .rxlength = 0,
                                               .override_freq_hz = 0,
                                               .user = nullptr,
                                               .tx_buffer = nullptr,
                                               .rx_buffer = buf,
                                           },
                                       .command_bits = 0,
                                       .address_bits = (uint8_t)(addr_len * 8),
                                       .dummy_bits = 0};
  ESP_ERROR_CHECK(spi_device_transmit(spi_device, &transaction.base));

  ESP_ERROR_CHECK(gpio_set_level(nss, 1));
}
#endif

// -----------------------------------------------------------------------------
// Time
// -----------------------------------------------------------------------------

static esp_timer_handle_t timer;

static void lmic_hal_timer_isr(void *arg) {
  BaseType_t higher_priority_task_woken;
  vTaskNotifyGiveFromISR(task, &higher_priority_task_woken);
  if (higher_priority_task_woken == pdTRUE) {
    esp_timer_isr_dispatch_need_yield();
  }
}

static void lmic_hal_time_init() {
  esp_timer_create_args_t timer_create_args = {
      .callback = lmic_hal_timer_isr,
      .arg = NULL,
      .dispatch_method = ESP_TIMER_ISR,
      .name = "LMIC timer",
      .skip_unhandled_events = true,
  };
  ESP_ERROR_CHECK(esp_timer_create(&timer_create_args, &timer));
}

u4_t lmic_hal_ticks() { return esp_timer_get_time() >> US_PER_OSTICK_EXPONENT; }

// Returns the number of ticks until time. Negative values indicate that
// time has already passed.
static s4_t delta_time(u4_t time) { return (s4_t)(time - lmic_hal_ticks()); }

u4_t lmic_hal_waitUntil(u4_t time) {
  s4_t remaining_osticks = delta_time(time);
  LMIC_LOG_DEBUG("Waiting for %d ticks (now is %u, until %u)",
                 remaining_osticks, lmic_hal_ticks(), time);
  if (remaining_osticks <= 0)
    return -remaining_osticks;

  for (;;) {
    s4_t remaining_freertos_ticks =
        pdMS_TO_TICKS(osticks2ms(remaining_osticks));
    LMIC_LOG_DEBUG("FreeRTOS ticks: %u", remaining_freertos_ticks);
    if (remaining_freertos_ticks < 2)
      break;
    vTaskDelay(remaining_freertos_ticks - 1);
    remaining_osticks = delta_time(time);
    if (remaining_osticks <= 0)
      return -remaining_osticks;
  }

  while (delta_time(time) > 0)
    ;

  return 0;
}

u1_t lmic_hal_checkTimer(u4_t time) { return delta_time(time) <= 0; }

void lmic_hal_sleep(s4_t deadline) {
  if (deadline == -1) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
  } else {
    s4_t remaining_osticks = delta_time(deadline);
    if (remaining_osticks <= 0) {
      return;
    }
    u4_t timeout_us = osticks2us(remaining_osticks);

    ESP_ERROR_CHECK(esp_timer_start_once(timer, timeout_us));
    TickType_t timeout_ticks =
        pdMS_TO_TICKS(timeout_us + (portTICK_PERIOD_MS * 1000 - 1));
    ulTaskNotifyTake(pdTRUE, timeout_ticks);
    esp_timer_stop(timer);
  }
}

// -----------------------------------------------------------------------------
// Init + Misc.
// -----------------------------------------------------------------------------

void lmic_hal_init(void) { esp_system_abort("use os_init_ex"); }

void lmic_hal_init_ex(const void *args) {
  HalConfig *const config = (HalConfig *)args;
  if (!lmic_hal_init_with_config(config)) {
    lmic_hal_failed(__FILE__, __LINE__);
  }
}

bool ESP_IDF_LMIC::lmic_hal_init_with_config(HalConfig *config) {
  if (!config)
    return false;

  hal_config = *config;

  LMIC_LOG_DEBUG("Callbacks: %p", hal_config.board.callbacks);
  void (HalBoardCallbacks::*mfp)() = &HalBoardCallbacks::begin;
  LMIC_LOG_DEBUG("Begin: %p", mfp);

  task = xTaskGetCurrentTaskHandle();

  hal_config.board.callbacks->begin();

  // configure radio I/O
  lmic_hal_io_init();
  // Configure interrupt I/O
  lmic_hal_interrupt_init();
  // configure radio SPI
  lmic_hal_spi_init();
  // configure timer and interrupt handler
  lmic_hal_time_init();
  // declare success
  return true;
}

void lmic_hal_failed(const char *file, u2_t line) {
  if (custom_hal_failure_handler != NULL) {
    (*custom_hal_failure_handler)(file, line);
  }

#if defined(LMIC_FAILURE)
  LMIC_FAILURE("FAILURE %s:%" PRIu16, file, line);
#endif
  abort();
}

void lmic_hal_set_failure_handler(lmic_hal_failure_handler_t *handler) {
  custom_hal_failure_handler = handler;
}

ostime_t lmic_hal_setModuleActive(bit_t val) {
  // setModuleActive() takes a c++ bool, so
  // it effectively says "val != 0". We
  // don't have to.
  return hal_config.board.callbacks->setModuleActive(val);
}

bit_t lmic_hal_queryUsingTcxo(void) {
  return hal_config.board.flags.using_tcxo;
}

bit_t lmic_hal_queryUsingDcdc(void) {
  return hal_config.board.flags.using_dcdc;
}

bit_t lmic_hal_queryUsingDIO2AsRfSwitch(void) {
  return hal_config.board.flags.using_dio2_as_rf_switch;
}

bit_t lmic_hal_queryUsingDIO3AsTCXOSwitch(void) {
  return hal_config.board.flags.using_dio3_as_tcxo_switch;
}

uint8_t lmic_hal_getTxPowerPolicy(u1_t inputPolicy, s1_t requestedPower,
                                  u4_t frequency) {
  return (uint8_t)hal_config.board.callbacks->getTxPowerPolicy(
      HalBoardCallbacks::TxPowerPolicy_t(inputPolicy), requestedPower,
      frequency);
}

void LMICOS_logEvent(const char* message) {
  LMIC_LOG_DEBUG("Event: %s", message);
}

void LMICOS_logEventUint32(const char* message, uint32_t datum) {
  LMIC_LOG_DEBUG("Event: %s: %" PRIu32, message, datum);
}
