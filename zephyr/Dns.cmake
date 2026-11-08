# Older Zephyr DNS dispatchers call a paired callback with the original
# context. A resolver then interprets the responder context as resolver state.
# Backport the paired-context argument fix in a build-local source copy.
# Upstream: https://github.com/zephyrproject-rtos/zephyr/blob/main/subsys/net/lib/dns/dispatcher.c
function(xedge_fix_dns_dispatcher)
  if(NOT CONFIG_MDNS_RESPONDER OR NOT CONFIG_MDNS_RESOLVER)
    return()
  endif()

  set(original "${ZEPHYR_BASE}/subsys/net/lib/dns/dispatcher.c")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${original}")
  file(READ "${original}" source)
  string(FIND "${source}" "dispatcher->pair->cb(dispatcher," needs_fix)
  if(needs_fix EQUAL -1)
    return()
  endif()
  string(REPLACE "dispatcher->pair->cb(dispatcher,"
    "dispatcher->pair->cb(dispatcher->pair," source "${source}")

  set(dns_target subsys__net__lib__dns)
  if(NOT TARGET ${dns_target})
    message(FATAL_ERROR "Cannot apply the Zephyr DNS paired-context fix: DNS target missing")
  endif()
  get_target_property(sources ${dns_target} SOURCES)
  set(replaced FALSE)
  foreach(item IN LISTS sources)
    if(item STREQUAL "dispatcher.c" OR item STREQUAL "${original}")
      list(REMOVE_ITEM sources "${item}")
      set(replaced TRUE)
    endif()
  endforeach()
  if(NOT replaced)
    message(FATAL_ERROR "Cannot locate the Zephyr DNS dispatcher source")
  endif()

  set(generated "${CMAKE_BINARY_DIR}/xedge_dns/dispatcher.c")
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/xedge_dns")
  file(CONFIGURE OUTPUT "${generated}" CONTENT "${source}" @ONLY)
  set_property(TARGET ${dns_target} PROPERTY SOURCES "${sources}")
  target_sources(${dns_target} PRIVATE "${generated}")
  target_include_directories(${dns_target} PRIVATE "${ZEPHYR_BASE}/subsys/net/lib/dns")
  message(STATUS "Applied Zephyr DNS paired-context fix to build-local dispatcher.c")
endfunction()

# Zephyr creates subsystem targets after processing the application CMake file.
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL xedge_fix_dns_dispatcher)
