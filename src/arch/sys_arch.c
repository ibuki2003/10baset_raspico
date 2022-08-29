#include "arch/cc.h"
#include "arch/sys_arch.h"

#include "lwip/sys.h"
#include "pico/time.h"

u32_t sys_now(void) {
  return to_ms_since_boot(get_absolute_time());
}
