#include <driver/i2c_master.h>
#include <soc/gpio_num.h>

struct I2cMasterBusConfig {
  gpio_num_t sda;
  gpio_num_t scl;
  i2c_clock_source_t clock_source;
};

struct I2CMasterBus {
  i2c_master_bus_handle_t handle;

  I2CMasterBus(I2cMasterBusConfig config) {
    i2c_master_bus_config_t raw_config = {.i2c_port = I2C_NUM_0,
                                          .sda_io_num = config.sda,
                                          .scl_io_num = config.scl,
                                          .clk_source = config.clock_source,
                                          .glitch_ignore_cnt = 7,
                                          .intr_priority = 3,
                                          .trans_queue_depth = 0,
                                          .flags{
                                              .enable_internal_pullup = true,
                                              .allow_pd = false,
                                          }};
    ESP_ERROR_CHECK(i2c_new_master_bus(&raw_config, &this->handle));
  }

  ~I2CMasterBus() { i2c_del_master_bus(handle); }
};
