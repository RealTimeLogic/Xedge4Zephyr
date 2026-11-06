/* Copyright 2026 Real Time Logic. Shared by devicetree and secure firmware. */
#ifndef XEDGE_FRDM_RW612_MEMORY_H
#define XEDGE_FRDM_RW612_MEMORY_H

/* Flash offsets 16-32 MiB. Keep boot/update slots, PSA storage and radio
 * firmware in the first 16 MiB separate from Xedge application files.
 */
#define XEDGE_STORAGE_OFFSET 0x01000000
#define XEDGE_STORAGE_SIZE   0x01000000
#define XEDGE_STORAGE_BASE   (0x08000000 + XEDGE_STORAGE_OFFSET)
#define XEDGE_STORAGE_LIMIT  (XEDGE_STORAGE_BASE + XEDGE_STORAGE_SIZE - 1)

#endif
