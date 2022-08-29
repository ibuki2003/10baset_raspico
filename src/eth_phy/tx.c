#include "../eth_phy.h"

#include "eth_phy_tx.pio.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "pico/time.h"

// 16, 17 represent TX-, TX+
#define TX_PIN 16

#define pio_ser_wr pio0
#define pio_sm 0

static uint dmach;

static struct repeating_timer pulse_timer;
static bool pulse_callback(struct repeating_timer *t);
static volatile uint8_t pulse_pause = 0;

void _eth_phy_init_tx(void) {
  gpio_init_mask(3u << TX_PIN);
  gpio_set_dir_out_masked(3u << TX_PIN);

  // pio init
  uint offset = pio_add_program(pio_ser_wr, (const pio_program_t*)&ser_10base_t_program);
  ser_10base_t_program_init(pio_ser_wr, pio_sm, offset, TX_PIN);

  // DMA setup
  dmach = dma_claim_unused_channel(true);
  dma_channel_config conf = dma_channel_get_default_config(dmach) ;   // DMA channel no.

  channel_config_set_read_increment(&conf, true);
  channel_config_set_write_increment(&conf, false);
  channel_config_set_transfer_data_size(&conf, DMA_SIZE_32);
  channel_config_set_dreq(&conf, pio_get_dreq(pio_ser_wr, pio_sm, true));

  dma_channel_set_config(dmach, &conf, false);
  dma_channel_set_write_addr(dmach, &pio_ser_wr->txf[pio_sm], false);

  // NLP timer
  add_repeating_timer_ms(-16, pulse_callback, NULL, &pulse_timer);
}

// generate NLP
static bool pulse_callback(struct repeating_timer *t) {
  if (pulse_pause) {
    if (pulse_pause < 10) pulse_pause--;
    return true;
  }
  ser_10base_t_tx_10b(pio_ser_wr, pio_sm, 0x0000000A);

  return true;
}

void eth_phy_enqueue_packet(uint32_t *buf, size_t length) {
  pulse_pause = 10;

  dma_channel_set_read_addr(dmach, buf, false);
  dma_channel_set_trans_count(dmach, length, true);
  dma_channel_wait_for_finish_blocking(dmach);

  pulse_pause = 1;
}
