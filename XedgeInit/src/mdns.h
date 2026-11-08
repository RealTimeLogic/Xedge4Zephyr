/* Copyright (c) 2026 Real Time Logic */
#ifndef XEDGE_ZEPHYR_MDNS_H
#define XEDGE_ZEPHYR_MDNS_H

#include <lua.h>

/* Install ba.createmdns; called with the BAS dispatcher mutex held.
 * Leaves the Lua stack unchanged.
 */
void xedgeMdnsInit(lua_State* L);

#endif
