/* Copyright 2026 Real Time Logic. SPDX-License-Identifier: Apache-2.0 */
#include "fsl_romapi_iap.h"
#include "fsl_flexspi.h"

/* RW612 ROM IAP uses LUT sequence 1 as scratch space. Zephyr uses that
 * sequence for page programming and later sequences for PSRAM. Preserve the
 * complete table across secure storage writes before returning to Zephyr.
 * The caller already excludes interrupts; retain its original mask. All
 * accesses while restoring the table execute from secure SRAM.
 */
static uint32_t save_lut(uint32_t *lut)
{
   uint32_t mask = __get_PRIMASK();
   __disable_irq();
   for (unsigned int i = 0; i < FLEXSPI_LUT_COUNT; ++i)
   {
      lut[i] = FLEXSPI->LUT[i];
   }
   return mask;
}

AT_QUICKACCESS_SECTION_CODE(static void __attribute__((noinline))
restore_lut(const uint32_t *lut, uint32_t mask))
{
   while (!FLEXSPI_GetBusIdleStatus(FLEXSPI))
   {
   }
   FLEXSPI->LUTKEY = FLEXSPI_LUTKEY_KEY(0x5AF05AF0U);
   FLEXSPI->LUTCR = FLEXSPI_LUTCR_UNLOCK_MASK;
   for (unsigned int i = 0; i < FLEXSPI_LUT_COUNT; ++i)
   {
      FLEXSPI->LUT[i] = lut[i];
   }
   FLEXSPI->LUTKEY = FLEXSPI_LUTKEY_KEY(0x5AF05AF0U);
   FLEXSPI->LUTCR = FLEXSPI_LUTCR_LOCK_MASK;
   __DSB();
   __ISB();
   __set_PRIMASK(mask);
}

extern status_t __real_iap_mem_write(api_core_context_t *, uint32_t,
                                   uint32_t, const uint8_t *, uint32_t);
extern status_t __real_iap_mem_write_blocked(api_core_context_t *, uint32_t,
                                           uint32_t, const uint8_t *, uint32_t);
extern status_t __real_iap_mem_erase(api_core_context_t *, uint32_t,
                                   uint32_t, uint32_t);
extern status_t __real_iap_mem_flush(api_core_context_t *);

status_t __wrap_iap_mem_write(api_core_context_t *ctx, uint32_t address,
                             uint32_t size, const uint8_t *data, uint32_t id)
{
   uint32_t lut[FLEXSPI_LUT_COUNT];
   uint32_t mask = save_lut(lut);
   status_t status = __real_iap_mem_write(ctx, address, size, data, id);
   restore_lut(lut, mask);
   return status;
}

status_t __wrap_iap_mem_write_blocked(api_core_context_t *ctx, uint32_t address,
                                     uint32_t size, const uint8_t *data, uint32_t id)
{
   uint32_t lut[FLEXSPI_LUT_COUNT];
   uint32_t mask = save_lut(lut);
   status_t status = __real_iap_mem_write_blocked(ctx, address, size, data, id);
   restore_lut(lut, mask);
   return status;
}

status_t __wrap_iap_mem_erase(api_core_context_t *ctx, uint32_t address,
                             uint32_t size, uint32_t id)
{
   uint32_t lut[FLEXSPI_LUT_COUNT];
   uint32_t mask = save_lut(lut);
   status_t status = __real_iap_mem_erase(ctx, address, size, id);
   restore_lut(lut, mask);
   return status;
}

status_t __wrap_iap_mem_flush(api_core_context_t *ctx)
{
   uint32_t lut[FLEXSPI_LUT_COUNT];
   uint32_t mask = save_lut(lut);
   status_t status = __real_iap_mem_flush(ctx);
   restore_lut(lut, mask);
   return status;
}
