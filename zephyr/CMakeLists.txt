# Xedge Zephyr Module CMakeLists.txt
# This file is loaded by Zephyr's build system when the module is enabled

if(CONFIG_XEDGE)

# The provider's public ABI must take precedence over BAS's bundled PSA headers.
if(CONFIG_XEDGE_PSA)
  target_include_directories(zephyr_interface BEFORE INTERFACE
    ${CMAKE_BINARY_DIR}/tfm/api_ns/interface/include
    ${ZEPHYR_TF_PSA_CRYPTO_MODULE_DIR}/include)
endif()

# Add include directories for BAS headers
zephyr_include_directories(
  ${CMAKE_CURRENT_LIST_DIR}/../BAS/inc
  ${CMAKE_CURRENT_LIST_DIR}/../BAS/inc/arch/Zephyr
  ${CMAKE_CURRENT_LIST_DIR}/../BAS/examples/xedge/src
)

# Determine which network porting layer to use
# For Zephyr, we use the generic SoDisp with Zephyr sockets
set(XEDGE_NET_ARCH "generic")

# Core BAS library (amalgamated). Keep the portable crypto defaults, including
# native_sim; RW612 delegates crypto to PSA. No optional ASM objects are linked.
zephyr_library_sources(
  ${CMAKE_CURRENT_LIST_DIR}/../BAS/src/BAS.c
)

# Zephyr porting layers
zephyr_library_sources(
  ${CMAKE_CURRENT_LIST_DIR}/../BAS/src/arch/Zephyr/ThreadLib.c
  ${CMAKE_CURRENT_LIST_DIR}/../BAS/src/arch/NET/${XEDGE_NET_ARCH}/SoDisp.c
)

if(CONFIG_BOARD_FRDM_RW612)
  # Flash and PSRAM share FlexSPI. Its reset temporarily prevents PSRAM
  # access, so the flash caller's stack must stay in internal SRAM.
  # ThreadLib.c allocates only thread stacks and their startup semaphores;
  # use the kernel heap for those allocations and both release paths.
  set_property(SOURCE ${CMAKE_CURRENT_LIST_DIR}/../BAS/src/arch/Zephyr/ThreadLib.c
    TARGET_DIRECTORY zephyr APPEND PROPERTY COMPILE_DEFINITIONS
    dlmalloc=k_malloc dlfree=k_free)
endif()

# Memory management - dlmalloc for RTOS
zephyr_library_sources(
  ${CMAKE_CURRENT_LIST_DIR}/../BAS/src/dlmalloc.c
)

# Xedge specific sources
include(${CMAKE_CURRENT_LIST_DIR}/Resources.cmake)
zephyr_library_sources(
  ${CMAKE_CURRENT_LIST_DIR}/../BAS/examples/xedge/src/xedge.c
  ${XEDGE_RESOURCE_FILE}
)

# Preserve the generated ZIP bytes while replacing its direct-memory reader.
if(CONFIG_XEDGE_RESOURCE_FLASH)
  set_property(SOURCE ${XEDGE_RESOURCE_FILE}
    TARGET_DIRECTORY zephyr APPEND PROPERTY COMPILE_DEFINITIONS
    getLspZipReader=xedge_unused_memory_zip_reader)
  zephyr_library_sources(src/flash_zip.c)
  zephyr_linker_sources(ROM_SECTIONS xedge_resources.ld)
endif()

# Optional: File system support (only if CONFIG_XEDGE_ENABLE_DISK_IO is set)
if(CONFIG_XEDGE_ENABLE_DISK_IO AND CONFIG_FILE_SYSTEM)
  zephyr_library_sources(
    ${CMAKE_CURRENT_LIST_DIR}/../BAS/src/DiskIo/Zephyr/BaFile.c
  )
endif()

# Compiler definitions
zephyr_compile_definitions(
  BA_ZEPHYR=1
  BA_ALLOCATOR=BA_ALLOCATOR_DLMALLOC
  BA_ENABLE_EMBEDDED_CA=1
  # Match the OPC UA Lua resources and register the native address-space reader.
  BA_ENABLE_LUA_OPCUA=1
  # ba.createmdns uses Zephyr's responder through XedgeInit/src/mdns.c.
  BA_ENABLE_MDNS=0
  BA_ENABLE_MDNS_LUA=0
  XEDGE_THREAD_POOL_MODE=XEDGE_THREAD_POOL_SHARED_FIXED
  XEDGE_THREAD_COUNT=${CONFIG_XEDGE_MAX_THREADS}
  XEDGE_HTTP_CONNECTIONS=${CONFIG_XEDGE_HTTP_CONNECTIONS}
  BA_THREAD_STACK_SIZE=${CONFIG_XEDGE_STACK_SIZE}
)

# Conditional definitions based on Kconfig
if(CONFIG_XEDGE_PSA)
  zephyr_compile_definitions(
    BA_ENABLE_TPM=1
    SHARKSSL_PSA_CONSUMER=1
    BA_PSA_GLOBALKEY_USE_DEVICE_ROOT=1
  )
elseif(CONFIG_XEDGE_ENABLE_ENCRYPTION_KEY)
  zephyr_compile_definitions(BA_ENABLE_TPM=1)
endif()

if(NOT CONFIG_XEDGE_ENABLE_ENCRYPTION_KEY)
  zephyr_compile_definitions(BA_ENABLE_SOFTTPM_KEY_MATERIAL=0)
endif()

# Conditional definitions based on Kconfig
if(NOT CONFIG_XEDGE_ENABLE_SHARKTRUST)
  zephyr_compile_definitions(BA_ENABLE_SHARKTRUST=0)
endif()

# Conditional definitions based on Kconfig
if(CONFIG_XEDGE_ENABLE_REVCON)
  zephyr_compile_definitions(BA_ENABLE_REVERSE_CONNECTION=1)
endif()

# Conditional definitions based on Kconfig
if(CONFIG_XEDGE_ENABLE_ZIPSIGNATURE)
  zephyr_compile_definitions(BA_ENABLE_ZIP_SIGNATURE=1)
endif()

# Conditional definitions based on Kconfig
if(NOT CONFIG_XEDGE_ENABLE_DISK_IO)
  zephyr_compile_definitions(
    XEDGE_ENABLE_DISK_IO=0
  )
endif()

if(CONFIG_XEDGE_ENABLE_DEBUGGER)
  zephyr_compile_definitions(
    BA_ENABLE_LUA_DEBUGGER=1
  )
endif()

# Compiler flags to handle BAS/Lua code warnings
#zephyr_library_compile_options()

endif() # CONFIG_XEDGE
