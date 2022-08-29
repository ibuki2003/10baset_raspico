#pragma once

#include "lwip/etharp.h"
/**
 * Helper struct to hold private data used to operate your ethernet interface.
 * Keeping the ethernet address of the MAC in this struct is not necessary
 * as it is already kept in the struct netif.
 * But this is only an example, anyway...
 */
struct rp_ethernetif {
  struct eth_addr *ethaddr;
  /* Add whatever per-interface state that is needed here. */
};

err_t rp_ethernetif_init(struct netif *netif);

void rp_ethernetif_input(struct netif *netif, void* data, uint16_t len);
