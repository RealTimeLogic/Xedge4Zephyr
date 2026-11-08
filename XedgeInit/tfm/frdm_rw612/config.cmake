# Extend the vendor platform through TF-M's application configuration hooks.
set(PROJECT_CONFIG_HEADER_FILE "${CMAKE_CURRENT_LIST_DIR}/config.h"
  CACHE FILEPATH "Xedge TF-M configuration" FORCE)

function(xedge_add_secure_storage_mapping)
  target_sources(platform_s PRIVATE
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/storage.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/flash.c)
  # ROM IAP must leave Zephyr's flash/PSRAM command table intact.
  target_link_options(tfm_s PRIVATE
    -Wl,--wrap=iap_mem_write
    -Wl,--wrap=iap_mem_write_blocked
    -Wl,--wrap=iap_mem_erase
    -Wl,--wrap=iap_mem_flush)
endfunction()

# platform_s is created after the extra configuration file is read.
cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR}
  CALL xedge_add_secure_storage_mapping)
