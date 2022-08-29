#include <stdio.h>
#include <string.h>
#include "hardware/gpio.h"
#include "lwip/err.h"
#include "lwip/pbuf.h"
#include "pico/platform.h"
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"
#include "lwip/tcp.h"

#include "eth_phy.h"

#include "rp_ethernetif/rp_ethernetif.h"


#define HW_PINNUM_LED0      (25)    // Pico onboard LED

static struct repeating_timer blink_timer;

static bool blink_callback(struct repeating_timer *t) {
  static bool led0_state = true;

  gpio_put(HW_PINNUM_LED0, led0_state);
  led0_state = !led0_state;

  return true;
}

#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
struct packed_struct_test {
  PACK_STRUCT_FLD_8(u8_t  dummy1);
  PACK_STRUCT_FIELD(u32_t dummy2);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/epstruct.h"
#endif
#define PACKED_STRUCT_TEST_EXPECTED_SIZE 5

err_t http_server_recv_callback(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err);

err_t http_server_accepted_callback(void *arg, struct tcp_pcb *pcb, err_t err) {
  if (pcb != NULL) {
    tcp_arg(pcb, NULL);
    tcp_sent(pcb, NULL);
    tcp_recv(pcb, http_server_recv_callback);
  }
  return ERR_OK;
}

const char http_server_response[] =
  "HTTP/1.1 200 OK\r\n"
  "Content-Type: text/html\r\n"
  "Server: Raspberry Pi Pico\r\n"
  "Connection: close\r\n"
  "\r\n"
  "<html><body>\r\n"
  "<h1>It works!</h1>\r\n"
  "Hello Ethernet from RasPico!\r\n"
  "</body></html>\r\n";

err_t http_server_recv_callback(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
  struct pbuf *q = p;
  while(q != NULL) {
    q = q->next;
  }
  tcp_recved(pcb, p->tot_len);

  tcp_write(pcb, http_server_response, sizeof(http_server_response) - 1, TCP_WRITE_FLAG_COPY);
  tcp_close(pcb);
  return ERR_OK;
}


int main() {

  stdio_init_all();

  gpio_init(HW_PINNUM_LED0);
  gpio_set_dir(HW_PINNUM_LED0, GPIO_OUT);
  add_repeating_timer_ms(-500, blink_callback, NULL, &blink_timer);

  struct netif netif;

  struct ip4_addr ipaddr;
  struct ip4_addr netmask;
  struct ip4_addr gateway;

  IP4_ADDR(&ipaddr,  192, 168,   0, 220);
  IP4_ADDR(&gateway, 192, 168,   0,   1);
  IP4_ADDR(&netmask, 255, 255, 255,   0);

  lwip_init();
  netif_add(&netif, &ipaddr, &netmask, &gateway, NULL, rp_ethernetif_init, ethernet_input);
  netif_set_default(&netif);
  netif_set_up(&netif);

  eth_phy_init(&netif);

  struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
  tcp_bind(pcb, IP4_ADDR_ANY, 80);

  pcb = tcp_listen(pcb);
  tcp_accept(pcb, http_server_accepted_callback);

  while(1) {
    eth_phy_read_buffer();

    /* Cyclic lwIP timers check */
    sys_check_timeouts();

    /* your application goes here */
  }

  while (1) tight_loop_contents();
}
