#include <string.h>
#include <cstdlib>
#include <vector>

#include "hardware/adc.h"
#include "pico/stdlib.h"

#include "picosystem.hpp"


namespace picosystem {

  Surface _screen = Surface(120, 120);
  Surface &screen = _screen;

  void init_picosystem() {

    gpio_set_function(pin::LED_R, GPIO_FUNC_SIO);
    gpio_set_dir(pin::LED_R, GPIO_OUT);
    gpio_put(pin::LED_R, 0);

    gpio_set_function(pin::LED_G, GPIO_FUNC_SIO);
    gpio_set_dir(pin::LED_G, GPIO_OUT);
    gpio_put(pin::LED_G, 0);

    gpio_set_function(pin::LED_B, GPIO_FUNC_SIO);
    gpio_set_dir(pin::LED_B, GPIO_OUT);
    gpio_put(pin::LED_B, 0);

    gpio_set_function(pin::CHARGE_LED_ENABLE, GPIO_FUNC_SIO);
    gpio_set_dir(pin::CHARGE_LED_ENABLE, GPIO_OUT);
    gpio_put(pin::CHARGE_LED_ENABLE, 0);

    adc_init();
    adc_gpio_init(pin::BATTERY_SENSE);


    gpio_put(pin::LED_G, 0);
    gpio_put(pin::LED_B, 0);

    init_screen();
    set_backlight(100);

    init_graphics();

    intro();

    gpio_set_function(button::A,    GPIO_FUNC_SIO);
    gpio_set_function(button::B,    GPIO_FUNC_SIO);
    gpio_set_function(button::X,    GPIO_FUNC_SIO);
    gpio_set_function(button::Y,    GPIO_FUNC_SIO);
    gpio_set_function(dpad::UP,     GPIO_FUNC_SIO);
    gpio_set_function(dpad::DOWN,   GPIO_FUNC_SIO);
    gpio_set_function(dpad::LEFT,   GPIO_FUNC_SIO);
    gpio_set_function(dpad::RIGHT,  GPIO_FUNC_SIO);

    gpio_set_dir(button::A,    GPIO_IN);
    gpio_set_dir(button::B,    GPIO_IN);
    gpio_set_dir(button::X,    GPIO_IN);
    gpio_set_dir(button::Y,    GPIO_IN);
    gpio_set_dir(dpad::UP,     GPIO_IN);
    gpio_set_dir(dpad::DOWN,   GPIO_IN);
    gpio_set_dir(dpad::LEFT,   GPIO_IN);
    gpio_set_dir(dpad::RIGHT,  GPIO_IN);

    gpio_pull_up(button::A);
    gpio_pull_up(button::B);
    gpio_pull_up(button::X);
    gpio_pull_up(button::Y);
    gpio_pull_up(dpad::UP);
    gpio_pull_up(dpad::DOWN);
    gpio_pull_up(dpad::LEFT);
    gpio_pull_up(dpad::RIGHT);
  }

  float battery_voltage() {
    adc_select_input(0);
    return (float(adc_read()) / (1 << 12)) * 2.0f * 3.3f;
  }

  uint32_t time() {
    absolute_time_t t = get_absolute_time();
    return to_ms_since_boot(t);
  }

  bool pressed(uint8_t button) {
    return gpio_get(button) == 0;
  }
}
