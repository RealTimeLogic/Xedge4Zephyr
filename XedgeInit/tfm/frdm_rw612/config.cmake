# Extend the vendor platform through TF-M's application configuration hooks.
set(PROJECT_CONFIG_HEADER_FILE "${CMAKE_CURRENT_LIST_DIR}/config.h"
  CACHE FILEPATH "Xedge TF-M configuration" FORCE)

function(xedge_add_secure_storage_mapping)
  target_sources(platform_s PRIVATE ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/storage.c)
endfunction()

# platform_s is created after the extra configuration file is read.
cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR}
  CALL xedge_add_secure_storage_mapping)
