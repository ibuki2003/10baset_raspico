#include <stdio.h>
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include "eth_phy.h"

#define HW_PINNUM_LED0      (25)    // Pico onboard LED

static struct repeating_timer blink_timer;

static bool blink_callback(struct repeating_timer *t) {
  static bool led0_state = true;

  gpio_put(HW_PINNUM_LED0, led0_state);
  led0_state = !led0_state;

  return true;
}

int main() {

  stdio_init_all();

  gpio_init(HW_PINNUM_LED0);
  gpio_set_dir(HW_PINNUM_LED0, GPIO_OUT);
  add_repeating_timer_ms(-500, blink_callback, NULL, &blink_timer);

  eth_phy_init();

  while (1) {
    eth_phy_read_buffer();
  }
}
