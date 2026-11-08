# Follow Xedge32: generate from the submodule into each build's own directory.
get_filename_component(xedge_source "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(xedge_psa 0)
if(CONFIG_XEDGE_PSA)
  set(xedge_psa 1)
endif()
set(XEDGE_RESOURCE_FILE "${CMAKE_CURRENT_BINARY_DIR}/resources/${xedge_psa}/XedgeZip.c")
file(GLOB_RECURSE xedge_resource_inputs CONFIGURE_DEPENDS
  "${xedge_source}/BAS-Resources/src/core/*"
  "${xedge_source}/BAS-Resources/src/xedge/*"
  "${xedge_source}/BAS-Resources/src/opcua/*")
if(CONFIG_XEDGE_PSA)
  list(APPEND xedge_resource_inputs "${xedge_source}/BAS-Resources/src/mako/.lua/psatpm.lua")
endif()
add_custom_command(OUTPUT "${XEDGE_RESOURCE_FILE}"
  COMMAND "${Python3_EXECUTABLE}" "${xedge_source}/scripts/build_resources.py"
    --source "${xedge_source}" --output "${XEDGE_RESOURCE_FILE}" --psa ${xedge_psa}
  DEPENDS ${xedge_resource_inputs} "${CMAKE_CURRENT_LIST_FILE}"
    "${xedge_source}/scripts/build_resources.py"
    "${xedge_source}/BAS-Resources/build/Xedge.sh"
    "${xedge_source}/BAS/tools/bin2c.c"
  VERBATIM)
add_custom_target(xedge_resources DEPENDS "${XEDGE_RESOURCE_FILE}")
add_dependencies(zephyr xedge_resources)
