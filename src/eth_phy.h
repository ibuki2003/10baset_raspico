#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void _eth_phy_init_rx(void);
void _eth_phy_init_tx(void);

/*
 * initialize some table for sending packets
 */
inline void eth_phy_init(void) {
  _eth_phy_init_rx();
  _eth_phy_init_tx();
}

/**
 * send a encoded packet
 * @param buf buffer ptr
 * @param len the length of the packet in elements (represents raw data bytes)
 * @param status
 * @return packet (internal) id if send successfully, -1 otherwise
 */
int eth_phy_enqueue_packet(uint32_t *buf, size_t length);

/**
 * check if a queued packet is sent.
 * @param id the packet id
 * @return true if sent, false otherwise
 */
bool eth_phy_queue_sent(int id);

/**
 * read packets received from buffer and call corresponding handlers
 *
 * should be called in the main loop.
 */
void eth_phy_read_buffer();

