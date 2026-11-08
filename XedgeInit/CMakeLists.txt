cmake_minimum_required(VERSION 3.20.0)

# Find Zephyr package (this sets up the build system)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(XedgeInit)

# Application source files
target_sources(app PRIVATE src/main.c src/mdns.c)
include(${CMAKE_CURRENT_SOURCE_DIR}/../zephyr/Dns.cmake)

if(CONFIG_BOARD_FRDM_RW612 AND CONFIG_XEDGE_PSA)
  set_property(TARGET zephyr_property_target APPEND PROPERTY TFM_CMAKE_OPTIONS
    -DTFM_EXTRA_CONFIG_PATH=${CMAKE_CURRENT_SOURCE_DIR}/tfm/frdm_rw612/config.cmake)
  # Match the algorithms enabled by NXP's TF-M crypto configuration.
  zephyr_compile_definitions(
    SHARKSSL_USE_CHACHA20=0
    SHARKSSL_USE_POLY1305=0
    SHARKSSL_ECC_USE_BRAINPOOLP256R1=0
    SHARKSSL_ECC_USE_BRAINPOOLP384R1=0
    SHARKSSL_ECC_USE_BRAINPOOLP512R1=0
  )
endif()

if(CONFIG_FLASH_MCUX_FLEXSPI_XIP)
  # The controller addresses must remain readable while the XIP flash is busy.
  # Zephyr relocates this driver's code; also relocate its constant configuration.
  zephyr_code_relocate(
    FILES ${ZEPHYR_BASE}/drivers/memc/memc_mcux_flexspi.c
    LOCATION ${CONFIG_FLASH_MCUX_FLEXSPI_XIP_MEM}_RODATA
  )
endif()
