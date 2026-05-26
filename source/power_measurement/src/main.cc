#include "ina219.hh"
#include "uart.h"
#include <util/delay.h>

constexpr const char *TAG = "main";

int main() {
  printf_init();
  ina219::reset();
  while (true) {
    ina219::wait_conversion_ready();
    double bus_voltage = ina219::read_bus_voltage();
    double shunt_voltage = ina219::read_shunt_voltage();
    double current = ina219::read_current();
    double power = ina219::read_power();
    LOGI(TAG, "B: %.2f V, S: %.2f mV, C: %.2f mA, P: %.2f mW", bus_voltage,
         shunt_voltage, current, power);
  }
}
