/*
 * ethernet interface driver
 * copied from lwIP example code.
 * below is the original copyright.
 */

/* Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

#include "rp_ethernetif/rp_ethernetif.h"

#include "eth_phy.h"
#include "lwip/opt.h"

#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "lwip/ethip6.h"
#include "lwip/etharp.h"
#include "netif/ppp/pppoe.h"
#include "pico/platform.h"
#include "pico/stdio.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

/* Forward declarations. */
void rp_ethernetif_input(struct netif *netif, void *data, uint16_t len);

// https://github.com/kingyoPiyo/Pico-10BASE-T/blob/main/src/udp.c
static uint32_t crc_table[256];
static void _make_crc_table(void) {
  for (uint32_t i = 0; i < 256; i++) {
    uint32_t c = i;
    for (uint32_t j = 0; j < 8; j++) {
      c = c & 1 ? (c >> 1) ^ 0xEDB88320 : (c >> 1);
    }
    crc_table[i] = c;
  }
}

/**
 * In this function, the hardware should be initialized.
 * Called from rp_ethernetif_init().
 *
 * @param netif the already initialized lwip network interface structure
 *        for this rp_ethernetif
 */
static void low_level_init(struct netif *netif) {
  _make_crc_table();
  struct rp_ethernetif *ethernetif = netif->state;

  /* set MAC hardware address length */
  netif->hwaddr_len = ETHARP_HWADDR_LEN;

  /* set MAC hardware address */
  netif->hwaddr[0] = 0xfe;
  netif->hwaddr[1] = 0xff;
  netif->hwaddr[2] = 0xff;
  netif->hwaddr[3] = 0x00;
  netif->hwaddr[4] = 0x00;
  netif->hwaddr[5] = 0x01;

  /* maximum transfer unit */
  netif->mtu = 1500;

  /* device capabilities */
  /* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

#if LWIP_IPV6 && LWIP_IPV6_MLD
  /*
   * For hardware/netifs that implement MAC filtering.
   * All-nodes link-local is handled by default, so we must let the hardware know
   * to allow multicast packets in.
   * Should set mld_mac_filter previously. */
  if (netif->mld_mac_filter != NULL) {
    ip6_addr_t ip6_allnodes_ll;
    ip6_addr_set_allnodes_linklocal(&ip6_allnodes_ll);
    netif->mld_mac_filter(netif, &ip6_allnodes_ll, NETIF_ADD_MAC_FILTER);
  }
#endif /* LWIP_IPV6 && LWIP_IPV6_MLD */

  /* Do whatever else is needed to initialize interface. */
}

// https://github.com/kingyoPiyo/Pico-10BASE-T/blob/main/src/udp.c
// Manchester table
// input 8bit, output 32bit, LSB first
// b00 -> IDLE
// b01 -> LOW
// b10 -> HIGH
// b11 -> not use.
const static uint32_t tbl_manchester[256] = {
    0x66666666, 0x66666669, 0x66666696, 0x66666699, 0x66666966, 0x66666969, 0x66666996, 0x66666999, 0x66669666, 0x66669669, 0x66669696, 0x66669699, 0x66669966, 0x66669969, 0x66669996, 0x66669999, 
    0x66696666, 0x66696669, 0x66696696, 0x66696699, 0x66696966, 0x66696969, 0x66696996, 0x66696999, 0x66699666, 0x66699669, 0x66699696, 0x66699699, 0x66699966, 0x66699969, 0x66699996, 0x66699999, 
    0x66966666, 0x66966669, 0x66966696, 0x66966699, 0x66966966, 0x66966969, 0x66966996, 0x66966999, 0x66969666, 0x66969669, 0x66969696, 0x66969699, 0x66969966, 0x66969969, 0x66969996, 0x66969999, 
    0x66996666, 0x66996669, 0x66996696, 0x66996699, 0x66996966, 0x66996969, 0x66996996, 0x66996999, 0x66999666, 0x66999669, 0x66999696, 0x66999699, 0x66999966, 0x66999969, 0x66999996, 0x66999999, 
    0x69666666, 0x69666669, 0x69666696, 0x69666699, 0x69666966, 0x69666969, 0x69666996, 0x69666999, 0x69669666, 0x69669669, 0x69669696, 0x69669699, 0x69669966, 0x69669969, 0x69669996, 0x69669999, 
    0x69696666, 0x69696669, 0x69696696, 0x69696699, 0x69696966, 0x69696969, 0x69696996, 0x69696999, 0x69699666, 0x69699669, 0x69699696, 0x69699699, 0x69699966, 0x69699969, 0x69699996, 0x69699999, 
    0x69966666, 0x69966669, 0x69966696, 0x69966699, 0x69966966, 0x69966969, 0x69966996, 0x69966999, 0x69969666, 0x69969669, 0x69969696, 0x69969699, 0x69969966, 0x69969969, 0x69969996, 0x69969999, 
    0x69996666, 0x69996669, 0x69996696, 0x69996699, 0x69996966, 0x69996969, 0x69996996, 0x69996999, 0x69999666, 0x69999669, 0x69999696, 0x69999699, 0x69999966, 0x69999969, 0x69999996, 0x69999999, 
    0x96666666, 0x96666669, 0x96666696, 0x96666699, 0x96666966, 0x96666969, 0x96666996, 0x96666999, 0x96669666, 0x96669669, 0x96669696, 0x96669699, 0x96669966, 0x96669969, 0x96669996, 0x96669999, 
    0x96696666, 0x96696669, 0x96696696, 0x96696699, 0x96696966, 0x96696969, 0x96696996, 0x96696999, 0x96699666, 0x96699669, 0x96699696, 0x96699699, 0x96699966, 0x96699969, 0x96699996, 0x96699999, 
    0x96966666, 0x96966669, 0x96966696, 0x96966699, 0x96966966, 0x96966969, 0x96966996, 0x96966999, 0x96969666, 0x96969669, 0x96969696, 0x96969699, 0x96969966, 0x96969969, 0x96969996, 0x96969999, 
    0x96996666, 0x96996669, 0x96996696, 0x96996699, 0x96996966, 0x96996969, 0x96996996, 0x96996999, 0x96999666, 0x96999669, 0x96999696, 0x96999699, 0x96999966, 0x96999969, 0x96999996, 0x96999999, 
    0x99666666, 0x99666669, 0x99666696, 0x99666699, 0x99666966, 0x99666969, 0x99666996, 0x99666999, 0x99669666, 0x99669669, 0x99669696, 0x99669699, 0x99669966, 0x99669969, 0x99669996, 0x99669999, 
    0x99696666, 0x99696669, 0x99696696, 0x99696699, 0x99696966, 0x99696969, 0x99696996, 0x99696999, 0x99699666, 0x99699669, 0x99699696, 0x99699699, 0x99699966, 0x99699969, 0x99699996, 0x99699999, 
    0x99966666, 0x99966669, 0x99966696, 0x99966699, 0x99966966, 0x99966969, 0x99966996, 0x99966999, 0x99969666, 0x99969669, 0x99969696, 0x99969699, 0x99969966, 0x99969969, 0x99969996, 0x99969999, 
    0x99996666, 0x99996669, 0x99996696, 0x99996699, 0x99996966, 0x99996969, 0x99996996, 0x99996999, 0x99999666, 0x99999669, 0x99999696, 0x99999699, 0x99999966, 0x99999969, 0x99999996, 0x99999999,
};

/**
 * This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif the lwip network interface structure for this rp_ethernetif
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
 * @return ERR_OK if the packet could be sent
 *         an err_t value if the packet couldn't be sent
 *
 * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
 *       strange results. You might consider waiting for space in the DMA queue
 *       to become available since the stack doesn't retry to send a packet
 *       dropped because of memory failure (except for the TCP timers).
 */

static err_t low_level_output(struct netif *netif, struct pbuf *p) {
  struct rp_ethernetif *ethernetif = netif->state;
  struct pbuf *q;

#if ETH_PAD_SIZE
  pbuf_remove_header(p, ETH_PAD_SIZE); /* drop the padding word */
#endif

  uint16_t len = max(p->tot_len, 60) + 4 + 8 + 12; // FCS + preamble + IPG

  uint32_t *raw_buf = malloc(len * sizeof(uint32_t));
  uint16_t i = 0;

  for (i = 0; i < 7; i++) raw_buf[i] = tbl_manchester[0x55];
  raw_buf[i++] = tbl_manchester[0xD5];

  uint32_t crc = 0xffffffff;

  for (q = p; q != NULL; q = q->next) {
    /* Send the data from the pbuf to the interface, one pbuf at a
       time. The size of the data in each pbuf is kept in the ->len
       variable. */
    for (uint16_t j = 0; j < q->len; ++j) {
      raw_buf[i++] = tbl_manchester[((uint8_t*)q->payload)[j]];
      crc = (crc >> 8) ^ crc_table[(crc ^ ((uint8_t *)q->payload)[j]) & 0xff];
    }
    stdio_flush();
  }

  for (uint16_t j = p->tot_len; j < 60; ++j) {
    raw_buf[i++] = tbl_manchester[0];
    crc = (crc >> 8) ^ crc_table[(crc ^ 0) & 0xff];
  }
  crc = ~crc;

  raw_buf[i++] = tbl_manchester[(crc >> 0) & 0xff];
  raw_buf[i++] = tbl_manchester[(crc >> 8) & 0xff];
  raw_buf[i++] = tbl_manchester[(crc >> 16) & 0xff];
  raw_buf[i++] = tbl_manchester[(crc >> 24) & 0xff];

  raw_buf[i++] = 0x00000AAA;
  for (uint16_t j = 0; j < 11; ++j) raw_buf[i++] = 0;

  /* signal that packet should be sent(); */
  eth_phy_enqueue_packet(raw_buf, len);

  free(raw_buf);


  MIB2_STATS_NETIF_ADD(netif, ifoutoctets, p->tot_len);
  if (((u8_t *)p->payload)[0] & 1) {
    /* broadcast or multicast packet*/
    MIB2_STATS_NETIF_INC(netif, ifoutnucastpkts);
  } else {
    /* unicast packet */
    MIB2_STATS_NETIF_INC(netif, ifoutucastpkts);
  }
  /* increase ifoutdiscards or ifouterrors on error */

#if ETH_PAD_SIZE
  pbuf_add_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif

  LINK_STATS_INC(link.xmit);

  return ERR_OK;
}

/**
 * Should allocate a pbuf and transfer the bytes of the incoming
 * packet from the interface into the pbuf.
 *
 * @param netif the lwip network interface structure for this rp_ethernetif
 * @return a pbuf filled with the received packet (including MAC header)
 *         NULL on memory error
 */
static struct pbuf * low_level_input(struct netif *netif, void* buf, u16_t len) {
  len -= 4; // FCS
  // TODO: validate CRC
  struct rp_ethernetif *ethernetif = netif->state;
  struct pbuf *p, *q;

#if ETH_PAD_SIZE
  len += ETH_PAD_SIZE; /* allow room for Ethernet padding */
#endif

  /* We allocate a pbuf chain of pbufs from the pool. */
  p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);

  if (p != NULL) {

#if ETH_PAD_SIZE
    pbuf_remove_header(p, ETH_PAD_SIZE); /* drop the padding word */
#endif

    /* We iterate over the pbuf chain until we have read the entire
     * packet into the pbuf. */
    void* ptr = buf;
    for (q = p; q != NULL; q = q->next) {
      /* Read enough bytes to fill this pbuf in the chain. The
       * available data in the pbuf is given by the q->len
       * variable.
       * This does not necessarily have to be a memcpy, you can also preallocate
       * pbufs for a DMA-enabled MAC and after receiving truncate it to the
       * actually received size. In this case, ensure the tot_len member of the
       * pbuf is the sum of the chained pbuf len members.
       */
      /* read data into(q->payload, q->len); */
      memcpy(q->payload, ptr, min(q->len, len + buf - ptr));

      ptr += q->len;
    }

    MIB2_STATS_NETIF_ADD(netif, ifinoctets, p->tot_len);
    if (((u8_t *)p->payload)[0] & 1) {
      /* broadcast or multicast packet*/
      MIB2_STATS_NETIF_INC(netif, ifinnucastpkts);
    } else {
      /* unicast packet*/
      MIB2_STATS_NETIF_INC(netif, ifinucastpkts);
    }
#if ETH_PAD_SIZE
    pbuf_add_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif

    LINK_STATS_INC(link.recv);
  } else {
    // drop packet
    LINK_STATS_INC(link.memerr);
    LINK_STATS_INC(link.drop);
    MIB2_STATS_NETIF_INC(netif, ifindiscards);
  }

  return p;
}

/**
 * This function should be called when a packet is ready to be read
 * from the interface. It uses the function low_level_input() that
 * should handle the actual reception of bytes from the network
 * interface. Then the type of the received packet is determined and
 * the appropriate input function is called.
 *
 * @param netif the lwip network interface structure for this rp_ethernetif
 */
void rp_ethernetif_input(struct netif *netif, void* buf, u16_t len) {
  struct rp_ethernetif *ethernetif;
  struct eth_hdr *ethhdr;
  struct pbuf *p;

  ethernetif = netif->state;

  /* move received packet into a new pbuf */
  p = low_level_input(netif, buf, len);
  /* if no packet could be read, silently ignore this */
  if (p != NULL) {
    /* pass all packets to ethernet_input, which decides what packets it supports */
    if (netif->input(p, netif) != ERR_OK) {
      LWIP_DEBUGF(NETIF_DEBUG, ("rp_ethernetif_input: IP input error\n"));
      pbuf_free(p);
      p = NULL;
    }
  }
}

/**
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * @param netif the lwip network interface structure for this rp_ethernetif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 */
err_t
rp_ethernetif_init(struct netif *netif)
{
  struct rp_ethernetif *ethernetif;

  LWIP_ASSERT("netif != NULL", (netif != NULL));

  ethernetif = mem_malloc(sizeof(struct rp_ethernetif));
  if (ethernetif == NULL) {
    LWIP_DEBUGF(NETIF_DEBUG, ("rp_ethernetif_init: out of memory\n"));
    return ERR_MEM;
  }

#if LWIP_NETIF_HOSTNAME
  /* Initialize interface hostname */
  netif->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */

  /*
   * Initialize the snmp variables and counters inside the struct netif.
   * The last argument should be replaced with your link speed, in units
   * of bits per second.
   */
  MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, LINK_SPEED_OF_YOUR_NETIF_IN_BPS);

  netif->state = ethernetif;
  netif->name[0] = 'e';
  netif->name[1] = 'n';
  /* We directly use etharp_output() here to save a function call.
   * You can instead declare your own function an call etharp_output()
   * from it if you have to do some checks before sending (e.g. if link
   * is available...) */
#if LWIP_IPV4
  netif->output = etharp_output;
#endif /* LWIP_IPV4 */
#if LWIP_IPV6
  netif->output_ip6 = ethip6_output;
#endif /* LWIP_IPV6 */
  netif->linkoutput = low_level_output;

  ethernetif->ethaddr = (struct eth_addr *) & (netif->hwaddr[0]);

  /* initialize the hardware */
  low_level_init(netif);

  return ERR_OK;
}

