/* Copyright 2026 Real Time Logic. FRDM-RW612 secure memory configuration. */
#include "tfm_hal_isolation.h"
#include "target_cfg.h"
#include "region_defs.h"
#include "../../boards/frdm_rw612_memory.h"

#if defined(TFM_EL2GO_DATA_IMPORT_REGION)
#error Xedge storage uses SAU region 6; EdgeLock 2GO needs a different layout
#endif

/* NXP's platform helper configures the flash controller's security rules. */
extern void enable_mem_rule_for_partition(uint32_t base, uint32_t limit);

/* Run in the secure partition manager before Zephyr starts. Only the separate
 * Xedge filesystem becomes nonsecure; TF-M key storage keeps its protection.
 */
FIH_RET_TYPE(enum tfm_hal_status_t) tfm_hal_post_partition_init_hook(void)
{
   __DMB();
   enable_mem_rule_for_partition(XEDGE_STORAGE_BASE, XEDGE_STORAGE_LIMIT);
   SAU->RNR = 6U;
   SAU->RBAR = XEDGE_STORAGE_BASE & SAU_RBAR_BADDR_Msk;
   SAU->RLAR = (XEDGE_STORAGE_LIMIT & SAU_RLAR_LADDR_Msk) | SAU_RLAR_ENABLE_Msk;
   __DSB();
   __ISB();
   FIH_RET(fih_int_encode(TFM_HAL_SUCCESS));
}
