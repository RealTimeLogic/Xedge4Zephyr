/*
 * Copyright (c) 2025 Real Time Logic
 * Xedge (xedge.c) and Barracuda App Server (BAS) initialization and startup code for the Zephyr RTOS
 * Target: native_sim, but should work for any target build
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/sntp.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>

// BAS includes
#include <TargConfig.h>
#include <barracuda.h>
#include <HttpTrace.h>
#include <HttpServer.h>
#include <BaErrorCodes.h>
#include <xedge.h>

// From Xedge (xedge.c) - the main server loop.
extern void barracuda(void);
/* The LThreadMgr configured in xedge.c */
extern LThreadMgr ltMgr;

LOG_MODULE_REGISTER(xedge_main, LOG_LEVEL_INF);

// dlmalloc initialization. Enabled by default.
#ifdef USE_DLMALLOC
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
K_THREAD_STACK_DEFINE(xedge_stack, BA_STACKSZ);
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

// xedgeOpenAUX not needed - NO_XEDGE_AUX is defined

/**
 * Xedge disk I/O initialization (optional)
 * Return -1 to use NetIo instead of DiskIo
 */
#ifndef NO_BAIO_DISK
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
    LOG_INF("Xedge stack size: %d bytes", BA_STACKSZ);
#ifdef USE_DLMALLOC
    // Initialize dlmalloc heap
    static char heap_pool[CONFIG_XEDGE_HEAP_SIZE];
    init_dlmalloc(heap_pool, heap_pool + sizeof(heap_pool));
    dlmalloc_setExhaustedCB(dlmalloc_exhausted_handler);
    LOG_INF("Xedge - dlmalloc initialized - heap size: %d bytes", sizeof(heap_pool));
#endif
    // Set up error handler and trace callback
    HttpServer_setErrHnd(xedge_error_handler);
    HttpTrace_setFLushCallback(xedge_trace_callback);

    k_thread_create(&xedge_thread_data, xedge_stack,
                    K_THREAD_STACK_SIZEOF(xedge_stack),
                    xedge_server_thread, NULL, NULL, NULL,
                    CONFIG_XEDGE_THREAD_PRIORITY, 0, K_NO_WAIT);

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
          ThreadMutex* soDispMutex = HttpServer_getMutex(ltMgr.server);
          ThreadMutex_set(soDispMutex);
          LThreadMgr_run(&ltMgr, job);
          ThreadMutex_release(soDispMutex);
          break;
       }
       else
       {
          LOG_ERR("NTP sync failed: %d. Did you configure your network?", ret);
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
