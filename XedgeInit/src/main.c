/*
 * Copyright (c) 2025 - 2026 Real Time Logic
 * Xedge (xedge.c) and Barracuda App Server (BAS) initialization and startup code for the Zephyr RTOS
 * Targets: native_sim, NXP i.MX RT, FRDM-RW612, and STM32F746G-DISCO
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/sntp.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/drivers/entropy.h>
#include <zephyr/linker/devicetree_regions.h>

// BAS includes
#include <TargConfig.h>
#include <barracuda.h>
#include <HttpTrace.h>
#include <HttpServer.h>
#include <BaErrorCodes.h>
#include <xedge.h>
#include "mdns.h"

// From Xedge (xedge.c) - the main server loop.
extern void barracuda(void);
/* The LThreadMgr configured in xedge.c */
extern LThreadMgr ltMgr;

LOG_MODULE_REGISTER(xedge_main, LOG_LEVEL_INF);

/* xedgeOpenAUX runs after the Lua thread manager has been constructed. */
K_SEM_DEFINE(xedge_ready, 0, 1);

#if SHARKSSL_PSA_CONSUMER
#include <psa/crypto.h>

/* TF-M has initialized its secure storage before entering Zephyr. Xedge calls
 * this hook before resource verification and Lua TPM initialization.
 */
int xedgeInitPsa(void)
{
   return (int)psa_crypto_init();
}
#endif

// dlmalloc initialization. Enabled by default.
#if BA_ALLOCATOR == BA_ALLOCATOR_DLMALLOC
#include <dlmalloc.h>

static void dlmalloc_exhausted_handler(void)
{
    LOG_ERR("DLMALLOC EXHAUSTED - Out of heap memory!");
    printk("CRITICAL: dlmalloc heap exhausted!\n");
}
#endif


/* Thread stack for Xedge main server loop and all BAS threads
   Inherits CONFIG_XEDGE_STACK_SIZE from prj.conf and set in
   CMakeLists.txt
 */
K_THREAD_STACK_DEFINE(xedge_stack, BA_THREAD_STACK_SIZE);
struct k_thread xedge_thread_data;

/**
 * Trace callback for HTTP tracing
 * This is called by BAS to output diagnostic messages
 */
static void xedge_trace_callback(char* buf, int bufLen)
{
   if(bufLen > 0)
   {
      if(buf[bufLen-1] == '\n')
         buf[bufLen-1] = 0;
      else
         buf[bufLen] = 0; // Null terminate
      LOG_INF("%s", buf);
   }
}

/**
 * Fatal error handler for Barracuda
 * Called when BAS encounters an unrecoverable error
 */
static void xedge_error_handler(BaFatalErrorCodes ecode1, 
                                unsigned int ecode2, 
                                const char* file, 
                                int line)
{
    LOG_ERR("Barracuda Fatal Error: %d %d at %s:%d", 
            ecode1, ecode2, file, line);
    printk("FATAL: Barracuda error %d %d at %s:%d\n", 
           ecode1, ecode2, file, line);
    k_panic();
}

/**
 * Xedge disk I/O initialization (optional)
 * Return -1 to use NetIo instead of DiskIo
 */
#if XEDGE_ENABLE_DISK_IO
int xedgeInitDiskIo(DiskIo* dio)
{
   LOG_INF("main.c - xedgeInitDiskIo: Mounting /xedge");
   if(DiskIo_setRootDir(dio,"/xedge"))
   {
      LOG_ERR("Cannot mount /xedge; see readme for details");
      k_panic();
   }
   return 0;
}
#endif

/*
  The function below is called by the Xedge startup code.
  This is a good place to add your own Lua bindings.
  See example code in BAS/examples/xedge/src led.c and AsynchLua.c
*/
int xedgeOpenAUX(XedgeOpenAUX* aux)
{
   xedgeMdnsInit(aux->L);
   k_sem_give(&xedge_ready);
   return 0; /* OK */
}


/**
 * Xedge server thread
 * This thread runs the Barracuda server (never returns)
 */
static void xedge_server_thread(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    LOG_INF("Starting the Xedge main loop");
    barracuda();
    // Above should not return
    LOG_ERR("barracuda() returned unexpectedly!");
}


/* We wait for the time to be set in function main(). When the time is
   set, main() triggers this function, which calls the Lua
   '_XedgeEvent' function with the argument sntp to signal that we
   have the correct time.

   This callback is called by one of the threads managed by LThreadMgr
   when a job is taken off the queue and executed. The callback
   attempts to find the global Lua function '_XedgeEvent', and if the
   function is found, it will be executed as follows:
   _XedgeEvent("sntp")

   The LThreadMgr object:
   https://realtimelogic.com/ba/doc/en/C/reference/html/md_en_C_md_LuaBindings.html#fullsolution
   Why time is important:
   https://realtimelogic.com/ba/examples/xedge/readme.html#time
*/
static void sntp_event(ThreadJob* job, int msgh, LThreadMgr* mgr)
{
   lua_State* L = job->Lt;
   lua_pushglobaltable(L); /* _G */
   lua_getfield(L, -1, "_XedgeEvent");
   if(lua_isfunction(L, -1))  /* Do we have _G._XedgeEvent */
   {
      /* Call _XedgeEvent("sntp") */
      lua_pushstring(L,"sntp"); /* Arg */
      lua_pcall(L, 1, 0, msgh); /* one arg, no return value */
   }
}



/**
 * Main application entry point
 */
int main(void)
{
    LOG_INF("Xedge application starting...");
    LOG_INF("Board: %s", CONFIG_BOARD);
    LOG_INF("Xedge stack size: %d bytes", BA_THREAD_STACK_SIZE);
#if BA_ALLOCATOR == BA_ALLOCATOR_DLMALLOC
    // Initialize dlmalloc heap
    /* Optional external heap is NOLOAD: SDRAM is initialized after BSS. */
    static char heap_pool[CONFIG_XEDGE_HEAP_SIZE] __aligned(8)
#if DT_HAS_CHOSEN(xedge_heap)
        Z_GENERIC_SECTION(LINKER_DT_NODE_REGION_NAME(DT_CHOSEN(xedge_heap)))
#endif
        ;
#if DT_HAS_CHOSEN(xedge_heap)
    BUILD_ASSERT(CONFIG_XEDGE_HEAP_SIZE <= DT_REG_SIZE(DT_CHOSEN(xedge_heap)),
                 "Xedge heap exceeds its memory region");
#endif
#if defined(CONFIG_BOARD_STM32F746G_DISCO)
    if (!device_is_ready(DEVICE_DT_GET(DT_CHILD(DT_NODELABEL(fmc), sdram))))
    {
        LOG_ERR("Cannot initialize SDRAM for the Xedge heap");
        k_panic();
    }
#endif
#if defined(CONFIG_BOARD_FRDM_RW612)
    if (!device_is_ready(DEVICE_DT_GET(DT_NODELABEL(aps6404l))))
    {
        LOG_ERR("Cannot initialize PSRAM for the Xedge heap");
        k_panic();
    }
#endif
    init_dlmalloc(heap_pool, heap_pool + sizeof(heap_pool));
    dlmalloc_setExhaustedCB(dlmalloc_exhausted_handler);
    LOG_INF("Xedge - dlmalloc initialized - heap size: %d bytes", sizeof(heap_pool));
#endif
    /* Initialize cryptography before TLS or key-generation work starts. */
#if defined(CONFIG_XEDGE_PSA)
    if (sharkssl_entropy() != 0)
    {
        LOG_ERR("Cannot initialize PSA cryptography");
        k_panic();
    }
#else
    const struct device *entropy = DEVICE_DT_GET(DT_CHOSEN(zephyr_entropy));
    uint8_t seed[48];
    if (!device_is_ready(entropy) ||
        entropy_get_entropy(entropy, (uint8_t *)seed, sizeof(seed)) != 0)
    {
        LOG_ERR("Cannot obtain startup entropy");
        k_panic();
    }
    int seeded = sharkssl_entropyEx(seed, sizeof(seed));
    memset(seed, 0, sizeof(seed));
    if (seeded != 0)
    {
        LOG_ERR("Cannot seed SharkSSL");
        k_panic();
    }
#endif

    // Set up error handler and trace callback
    HttpServer_setErrHnd(xedge_error_handler);
    HttpTrace_setFLushCallback(xedge_trace_callback);

    k_thread_create(&xedge_thread_data, xedge_stack,
                    K_THREAD_STACK_SIZEOF(xedge_stack),
                    xedge_server_thread, NULL, NULL, NULL,
                    CONFIG_XEDGE_THREAD_PRIORITY, 0, K_NO_WAIT);

    /* A fast SNTP response must not access an uninitialized ltMgr. */
    k_sem_take(&xedge_ready, K_FOREVER);

    for(;;)
    {
       struct sntp_time sntp_time;
       int ret = sntp_simple("pool.ntp.org", 5000, &sntp_time);
       if (ret == 0)
       {
          struct timespec ts = {
             .tv_sec = sntp_time.seconds,
             .tv_nsec = 0
          };
          clock_settime(CLOCK_REALTIME, &ts);
          /* Initiate executing the Lua func _XedgeEvent("sntp") */
          ThreadJob* job=ThreadJob_lcreate(sizeof(ThreadJob), sntp_event);
          if (!job)
          {
             LOG_ERR("Cannot allocate the SNTP notification");
             k_panic();
          }
          ThreadMutex* soDispMutex = HttpServer_getMutex(ltMgr.server);
          ThreadMutex_set(soDispMutex);
          LThreadMgr_run(&ltMgr, job);
          ThreadMutex_release(soDispMutex);
          break;
       }
       else
       {
          LOG_ERR("NTP sync failed: %s (%d). Did you configure your network?",
                  zsock_gai_strerror(ret), ret);
          k_sleep(K_SECONDS(2));
       }
    }
    
    // Main thread continues - could do other work here
    for(;;)
    {
       k_sleep(K_SECONDS(30));
       LOG_INF("Uptime: %lld seconds", k_uptime_get() / 1000);
    }

    return 0;
}
