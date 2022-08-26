#include "../eth_phy.h"

#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/dma.h"

#include "eth_phy_rx_pio.h"

#define RX_PIN 18

#define RX_BUF_SIZE_BYTES 8192
#define RX_BUF_RING_BITS 14 // 2 bytes * 8192 entries = 16384 bytes

volatile uint16_t dbuf[RX_BUF_SIZE_BYTES] __attribute__((aligned(RX_BUF_SIZE_BYTES*2)));
uint dbuf_csr = 0;
volatile uint dbuf_tail = 0; // updated by recv_handler

#define pio pio1
#define pio_sm 0

uint dmach;

static void dma_handler();
static void recv_handler();

void _eth_phy_init_rx(void) {
  gpio_init_mask(1u << RX_PIN);
  gpio_set_dir_in_masked(1u << RX_PIN);

  // pio
  uint offset;
  offset = pio_add_program(pio, (pio_program_t*)&eth_phy_rx_program);
  eth_phy_rx_program_init(pio, pio_sm, offset, 18);

  pio_set_irq0_source_enabled(pio, pis_interrupt0, true);
  irq_set_exclusive_handler(PIO1_IRQ_0, recv_handler);
  irq_set_enabled(PIO1_IRQ_0, true);

  // dma
  dmach = dma_claim_unused_channel(true);
  dma_channel_config conf = dma_channel_get_default_config(dmach) ;	// DMA channel no.
  channel_config_set_read_increment(&conf, false);
  channel_config_set_write_increment(&conf, true);
  channel_config_set_transfer_data_size(&conf, DMA_SIZE_32);
  channel_config_set_dreq(&conf, pio_get_dreq(pio, pio_sm, false));
  channel_config_set_ring(&conf, true, RX_BUF_RING_BITS);

  // as many as possible
  dma_channel_configure(dmach, &conf, dbuf, &pio->rxf[pio_sm], 0xffffffff, false);

  dma_channel_set_irq1_enabled(dmach, true);
  irq_set_exclusive_handler(DMA_IRQ_1, dma_handler);
  irq_set_enabled(DMA_IRQ_1, true);

  dma_channel_start(dmach);

}

static void dma_handler() {
  dma_hw->ints1 = 1u << dmach; // reset interrupt
  // restart dma
  dma_channel_set_trans_count(0, 0xffffffff, true);
}

static void recv_handler() {
  dbuf_tail = (uint16_t*)(dma_hw->ch[0].write_addr) - &dbuf[0];
  pio->irq = 0b1;
}

#define CBUF_SIZE 1522 // max ethernet frame size

uint8_t cbuf[CBUF_SIZE];

void eth_phy_read_buffer() {
  // read packets
  bool reading = 0;
  uint csr = 0;
  while (dbuf_csr != dbuf_tail) {
    if (reading) {
      if ((dbuf[dbuf_csr] & 0x00ff) == 0) { // end of packet
        // TODO: call handler

        reading = false;
      } else {
        if (
            csr >= CBUF_SIZE || // buffer overflow :(
            (dbuf[dbuf_csr] & 0xaaaa) >> 1 != (~dbuf[dbuf_csr] & 0x5555)) { // invalid manchester
          // skip packet
          while (dbuf_csr != dbuf_tail && (dbuf[dbuf_csr] & 0xff00))
            dbuf_csr = (dbuf_csr + 1) % RX_BUF_SIZE_BYTES;
          reading = false;
          continue;
        }

        // decode manchester encoding
        cbuf[csr] = (
            ((dbuf[dbuf_csr] & 0x8000) >> 8) |
            ((dbuf[dbuf_csr] & 0x2000) >> 7) |
            ((dbuf[dbuf_csr] & 0x0800) >> 6) |
            ((dbuf[dbuf_csr] & 0x0200) >> 5) |
            ((dbuf[dbuf_csr] & 0x0080) >> 4) |
            ((dbuf[dbuf_csr] & 0x0020) >> 3) |
            ((dbuf[dbuf_csr] & 0x0008) >> 2) |
            ((dbuf[dbuf_csr] & 0x0002) >> 1)
            );
        csr++;
      }
    } else if (dbuf[dbuf_csr] == 0x6666) { // preamble found
      if ((dbuf_tail + RX_BUF_SIZE_BYTES - dbuf_csr) % RX_BUF_SIZE_BYTES < 8) {
        // not enough data for a full frame
        dbuf_csr = dbuf_tail; // remaining data will be destroyed
        break;
      }

      csr = 0;
      for (; csr < 7; csr++) {
        if (dbuf[dbuf_csr] != 0x6666) break;
        dbuf_csr = (dbuf_csr + 1) % RX_BUF_SIZE_BYTES;
      }
      if (csr != 7 || dbuf[dbuf_csr] != 0xa666) continue; // not a valid preamble

      reading = 1;
      csr = 0;
    }
    dbuf_csr = (dbuf_csr + 1) % RX_BUF_SIZE_BYTES;
  }
}
