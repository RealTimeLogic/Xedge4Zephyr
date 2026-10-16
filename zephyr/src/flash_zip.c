/* Copyright (c) 2026 Real Time Logic
 * Read Xedge's immutable ZIP through the flash driver. The driver serializes
 * these reads with LittleFS writes on the same external flash device.
 */
#include <barracuda.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(xedge_resources, LOG_LEVEL_INF);

extern const unsigned char __xedge_resources_start[];
extern const unsigned char __xedge_resources_end[];
static const struct flash_area *resource_area;

static int read_resources(CspReader *reader, void *data, U32 offset,
                          U32 size, int blockStart)
{
    ARG_UNUSED(reader);
    ARG_UNUSED(blockStart);
    return flash_area_read(resource_area, offset, data, size);
}

ZipReader *getLspZipReader(void)
{
    static ZipReader reader;
    U32 size = (U32)((uintptr_t)__xedge_resources_end -
                    (uintptr_t)__xedge_resources_start);

    ZipReader_constructor(&reader, read_resources, size);
    int rc = flash_area_open(FIXED_PARTITION_ID(xedge_resources), &resource_area);
    if (rc != 0)
    {
        LOG_ERR("Cannot open Xedge resource flash: %d", rc);
        return &reader; /* Leave invalid so Xedge reports a startup failure. */
    }
    CspReader_setIsValid(&reader);
    return &reader;
}
