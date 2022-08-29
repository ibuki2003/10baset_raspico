#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "lwip/netif.h"

void _eth_phy_init_rx(void);
void _eth_phy_init_tx(void);

extern struct netif *global_netif;

/*
 * initialize some table for sending packets
 */
inline void eth_phy_init(struct netif *netif) {
  global_netif = netif;
  _eth_phy_init_rx();
  _eth_phy_init_tx();
}

/**
 * send a encoded packet
 * @param buf buffer ptr
 * @param len the length of the packet in elements (represents raw data bytes)
 * @param status
 */
void eth_phy_enqueue_packet(uint32_t *buf, size_t length);

/**
 * read packets received from buffer and call corresponding handlers
 *
 * should be called in the main loop.
 */
void eth_phy_read_buffer();

