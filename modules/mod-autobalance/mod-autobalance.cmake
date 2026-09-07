# Compatibility shim for yakanikel-droid/541ac.
#
# This core registers the empty src/server/scripts/Custom directory as a static
# script module and generates a call to AddCustomScripts(), but it does not
# provide the function when that directory contains no C++ sources.  Supply a
# no-op implementation only for that exact case so the worldserver can link.
if(TARGET scripts AND SCRIPTS MATCHES "static")
  file(GLOB ACORE_CUSTOM_SCRIPT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/server/scripts/Custom/*.cpp")

  if(NOT ACORE_CUSTOM_SCRIPT_SOURCES)
    set(AUTOBALANCE_CUSTOM_LOADER_SHIM
      "${CMAKE_CURRENT_BINARY_DIR}/mod-autobalance/EmptyCustomScriptLoader.cpp")
    configure_file(
      "${CMAKE_CURRENT_LIST_DIR}/src/compat/EmptyCustomScriptLoader.cpp.in"
      "${AUTOBALANCE_CUSTOM_LOADER_SHIM}"
      COPYONLY)
    # Keep the shim in the scripts archive: it is referenced by another
    # member of that archive, while the core links the modules archive first.
    target_sources(scripts PRIVATE "${AUTOBALANCE_CUSTOM_LOADER_SHIM}")
  endif()
endif()
