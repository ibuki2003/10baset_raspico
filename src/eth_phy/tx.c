#include "../eth_phy.h"

#include "eth_phy_tx.pio.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "pico/time.h"

// 16, 17 represent TX-, TX+
#define TX_PIN 16

// number of packets
#define TX_QUEUE_SIZE 1024

#define pio_ser_wr pio0
#define pio_sm 0

struct {
  uint32_t* buf;
  size_t len;
} static queue[TX_QUEUE_SIZE];

// NOTE: stores (not circular) unique id
static volatile unsigned int queue_head = 0;
static unsigned int queue_tail = 0;

static uint dmach;

static void dma_handler();

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

  dma_channel_set_irq0_enabled(dmach, true);
  irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
  irq_set_enabled(DMA_IRQ_0, true);

  // NLP timer
  add_repeating_timer_ms(-16, pulse_callback, NULL, &pulse_timer);
}

static bool dma_busy = false;
static void dma_handler() {
  dma_hw->ints0 = 1u << dmach;
  if (queue_head != queue_tail) {
    dma_busy = true;
    pulse_pause = 10;
    dma_channel_set_read_addr(dmach, queue[queue_head % TX_QUEUE_SIZE].buf, false);
    dma_channel_set_trans_count(dmach, queue[queue_head % TX_QUEUE_SIZE].len, true);
    queue_head++;
  } else {
    dma_busy = false;
    pulse_pause = 1;
  }
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

int eth_phy_enqueue_packet(uint32_t *buf, size_t length) {
  if (queue_tail - queue_head >= TX_QUEUE_SIZE) {
    // queue full
    return -1;
  }

  queue[queue_tail % TX_QUEUE_SIZE].buf = buf;
  queue[queue_tail % TX_QUEUE_SIZE].len = length;

  queue_tail++;

  // HACK: fire dma chain
  if (!dma_busy) dma_handler();

  return queue_tail - 1;
}

bool eth_phy_queue_sent(int id) {
  if (id < 0 || id >= queue_tail) {
    return false;
  }
  return queue_head > id;
}

