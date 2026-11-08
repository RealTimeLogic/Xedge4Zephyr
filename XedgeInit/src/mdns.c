/* Copyright (c) 2026 Real Time Logic
 * Lua ownership of Zephyr's single native mDNS hostname.
 */
#include <zephyr/net/hostname.h>
#include <zephyr/net/dns_resolve.h>
#include <zephyr/logging/log.h>
#include <lauxlib.h>
#include <string.h>
#include "mdns.h"

LOG_MODULE_REGISTER(xedge_mdns, LOG_LEVEL_INF);

#if !defined(CONFIG_MDNS_RESPONDER) || !defined(CONFIG_NET_HOSTNAME_DYNAMIC)
#error Xedge requires Zephyr native mDNS and dynamic hostname support
#endif

#define MDNS_META "Xedge.Zephyr.mDNS"

typedef struct
{
   char name[NET_HOSTNAME_SIZE];
   char previous[NET_HOSTNAME_SIZE];
} XedgeMdns;

/* Lua owns the userdata. All accesses run under the BAS dispatcher mutex;
 * close/GC clears this non-owning pointer before the userdata is released.
 */
static XedgeMdns* active;

static int mdnsError(lua_State* L, const char* message)
{
   lua_pushnil(L);
   lua_pushstring(L,message);
   return 2;
}

static int mdnsClose(lua_State* L)
{
   XedgeMdns* o=luaL_checkudata(L,1,MDNS_META);
   if(active == o)
   {
      /* Do not undo a hostname change made elsewhere in the application. */
      if(!strcmp(net_hostname_get(),o->name))
         net_hostname_set(o->previous,strlen(o->previous));
      active=NULL;
   }
   return 0;
}

static int mdnsStatus(lua_State* L)
{
   XedgeMdns* o=luaL_checkudata(L,1,MDNS_META);
   if(active != o)
      return mdnsError(L,"closed");
   if(strcmp(net_hostname_get(),o->name))
      return mdnsError(L,"hostname changed");
   lua_pushboolean(L,1);
   return 1;
}

static int mdnsCreate(lua_State* L)
{
   XedgeMdns* o;
   size_t len;
   const char* name;

   luaL_checktype(L,1,LUA_TSTRING);
   name=lua_tolstring(L,1,&len);
   luaL_argcheck(L,len == strlen(name),1,"name contains NUL");
   if(!len || len > 63 || len > NET_HOSTNAME_MAX_LEN ||
      name[0] == '-' || name[len-1] == '-')
      return mdnsError(L,"invalid name");
   for(size_t i=0; i<len; ++i)
   {
      unsigned char c=(unsigned char)name[i];
      if(!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '-'))
         return mdnsError(L,"invalid name");
   }
   if(active)
      return mdnsError(L,"mDNS hostname already in use");

   /* Allocate and attach the finalizer before changing native state. */
   o=lua_newuserdatauv(L,sizeof(*o),0);
   luaL_setmetatable(L,MDNS_META);
   memcpy(o->name,name,len);
   o->name[len]=0;
   strcpy(o->previous,net_hostname_get());
   if(net_hostname_set(o->name,len))
      return mdnsError(L,"cannot set hostname");
   active=o;
   return 1;
}

void xedgeMdnsInit(lua_State* L)
{
   static const char* servers[] = {
#if defined(CONFIG_NET_IPV4)
      "224.0.0.251:5353",
#endif
#if defined(CONFIG_NET_IPV6)
      "[ff02::fb]:5353",
#endif
      NULL
   };
   static const luaL_Reg methods[] = {
      {"status",mdnsStatus},
      {"close",mdnsClose},
      {"__close",mdnsClose},
      {"__gc",mdnsClose},
      {NULL,NULL}
   };
   /* Network setup has completed before Xedge starts. Append multicast DNS
    * after DHCP's unicast server: Zephyr tries the first eligible server.
    * Static configurations already containing these entries are unchanged.
    */
   int err=dns_resolve_reconfigure(dns_resolve_get_default(),servers,NULL,
                                   DNS_SOURCE_MANUAL);
   if(err)
      LOG_ERR("Cannot initialize mDNS resolver (%d)",err);
   if(luaL_newmetatable(L,MDNS_META))
   {
      lua_pushvalue(L,-1);
      lua_setfield(L,-2,"__index");
      luaL_setfuncs(L,methods,0);
   }
   lua_pop(L,1);
   lua_getglobal(L,"ba");
   lua_pushcfunction(L,mdnsCreate);
   lua_setfield(L,-2,"createmdns");
   lua_pop(L,1);
}
