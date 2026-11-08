/* Copyright 2026 Real Time Logic. Preserve NXP's large TF-M profile. */
#ifndef XEDGE_RW612_TFM_CONFIG_H
#define XEDGE_RW612_TFM_CONFIG_H

#include "profile/config_profile_large.h"
#define CONFIG_TFM_POST_PARTITION_INIT_HOOK 1

/* TLS connections retain hash operations and volatile keys simultaneously. */
#undef CRYPTO_CONC_OPER_NUM
#define CRYPTO_CONC_OPER_NUM 64
#define MBEDTLS_PSA_KEY_SLOT_COUNT 64

/* TLS 1.3 uses the separate HKDF stages, not only combined HKDF. */
#define PSA_WANT_ALG_HKDF_EXTRACT 1
#define PSA_WANT_ALG_HKDF_EXPAND 1

/* SharkCA enrollment derives its registration proof from the shared secret. */
#define PSA_WANT_ALG_PBKDF2_HMAC 1

#endif
