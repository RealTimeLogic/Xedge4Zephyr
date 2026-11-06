/* Copyright 2026 Real Time Logic. Preserve NXP's large TF-M profile. */
#ifndef XEDGE_RW612_TFM_CONFIG_H
#define XEDGE_RW612_TFM_CONFIG_H

#include "profile/config_profile_large.h"
#define CONFIG_TFM_POST_PARTITION_INIT_HOOK 1

/* TLS connections retain hash operations and volatile keys simultaneously. */
#undef CRYPTO_CONC_OPER_NUM
#define CRYPTO_CONC_OPER_NUM 64
#define MBEDTLS_PSA_KEY_SLOT_COUNT 64

#endif
