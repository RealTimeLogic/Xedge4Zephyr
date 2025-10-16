# Xedge Zephyr Module CMakeLists.txt
# This file is loaded by Zephyr's build system when the module is enabled

if(CONFIG_XEDGE)

# Add include directories for BAS headers
zephyr_include_directories(
  ${CMAKE_CURRENT_LIST_DIR}/../BAS/inc
  ${CMAKE_CURRENT_LIST_DIR}/../BAS/inc/arch/Zephyr
  ${CMAKE_CURRENT_LIST_DIR}/../BAS/examples/xedge/src
)

# Determine which network porting layer to use
# For Zephyr, we use the generic SoDisp with Zephyr sockets
set(XEDGE_NET_ARCH "generic")

# Core BAS library (amalgamated)
zephyr_library_sources(
  ${CMAKE_CURRENT_LIST_DIR}/../BAS/src/BAS.c
)

# Zephyr porting layers
zephyr_library_sources(
  ${CMAKE_CURRENT_LIST_DIR}/../BAS/src/arch/Zephyr/ThreadLib.c
  ${CMAKE_CURRENT_LIST_DIR}/../BAS/src/arch/NET/${XEDGE_NET_ARCH}/SoDisp.c
)

# Memory management - dlmalloc for RTOS
zephyr_library_sources(
  ${CMAKE_CURRENT_LIST_DIR}/../BAS/src/dlmalloc.c
)

# Xedge specific sources
zephyr_library_sources(
  ${CMAKE_CURRENT_LIST_DIR}/../BAS/examples/xedge/src/xedge.c
  ${CMAKE_CURRENT_LIST_DIR}/../BAS/examples/xedge/XedgeZip.c
)

# Optional: File system support (only if CONFIG_XEDGE_ENABLE_DISK_IO is set)
if(CONFIG_XEDGE_ENABLE_DISK_IO AND CONFIG_FILE_SYSTEM)
  zephyr_library_sources(
    ${CMAKE_CURRENT_LIST_DIR}/../BAS/src/DiskIo/Zephyr/BaFile.c
  )
endif()

# Compiler definitions
zephyr_compile_definitions(
  BA_ZEPHYR=1
  USE_DLMALLOC=1
  B_LITTLE_ENDIAN=1
  MAXTHREADS=${CONFIG_XEDGE_MAX_THREADS}
  BA_STACKSZ=16384
  NO_ENCRYPTIONKEY=1
  NO_SHARKTRUST=1
)

# Conditional definitions based on Kconfig
if(NOT CONFIG_XEDGE_ENABLE_DISK_IO)
  zephyr_compile_definitions(
    NO_BAIO_DISK=1
  )
endif()

# Always define NO_XEDGE_AUX since we provide a stub in main.c
zephyr_compile_definitions(
  NO_XEDGE_AUX=1
)

if(CONFIG_XEDGE_ENABLE_DEBUGGER)
  zephyr_compile_definitions(
    USE_DBGMON=1
  )
endif()

# Compiler flags to handle BAS/Lua code warnings
zephyr_library_compile_options(
  -Wno-char-subscripts
  -Wno-implicit-fallthrough
  -Wno-unused-function
  -Wno-unused-value
  -Wno-unused-variable
  -Wno-format
  -Wno-format-security
  -Wno-pointer-sign
  $<$<COMPILE_LANGUAGE:C>:-Wno-declaration-after-statement>
  $<$<COMPILE_LANGUAGE:C>:-Wno-implicit-function-declaration>
)

# Link math library (required for Lua)
zephyr_link_libraries(m)

endif() # CONFIG_XEDGE

