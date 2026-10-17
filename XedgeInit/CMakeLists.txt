cmake_minimum_required(VERSION 3.20.0)

# Find Zephyr package (this sets up the build system)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(XedgeInit)

# Application source files
target_sources(app PRIVATE src/main.c)

if(CONFIG_FLASH_MCUX_FLEXSPI_XIP)
  # The controller addresses must remain readable while the XIP flash is busy.
  # Zephyr relocates this driver's code; also relocate its constant configuration.
  zephyr_code_relocate(
    FILES ${ZEPHYR_BASE}/drivers/memc/memc_mcux_flexspi.c
    LOCATION ${CONFIG_FLASH_MCUX_FLEXSPI_XIP_MEM}_RODATA
  )
endif()
