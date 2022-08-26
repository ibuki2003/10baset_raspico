#pragma once

#include "hardware/pio.h"
#include "eth_phy_rx.pio.h"

static inline void eth_phy_rx_program_init(PIO pio, uint sm, uint offset, uint pin) {
  pio_sm_set_pins_with_mask(pio, sm, 0, 1u << pin);
  pio_sm_set_pindirs_with_mask(pio, sm, 0u, 1u << pin);

  pio_sm_config c = eth_phy_rx_program_get_default_config(offset);
  sm_config_set_in_shift(&c, true, true, 32);

  /* sm_config_set_sideset_pins(&c, pin); */
  sm_config_set_jmp_pin(&c, pin);
  /* sm_config_set_out_pins(&c, pin, 1); */
  sm_config_set_in_pins(&c, pin);

  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);

  sm_config_set_clkdiv_int_frac(&c, 1, 64);

  pio_sm_init(pio, sm, offset, &c);
  pio_sm_set_enabled(pio, sm, true);
}

