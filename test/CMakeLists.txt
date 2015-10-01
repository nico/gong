# Test runner infrastructure for Gong. This configures the Gong test trees
# for use by Lit, and delegates to LLVM's lit test handlers.

if (CMAKE_CFG_INTDIR STREQUAL ".")
  set(LLVM_BUILD_MODE ".")
else ()
  set(LLVM_BUILD_MODE "%(build_mode)s")
endif ()

string(REPLACE ${CMAKE_CFG_INTDIR} ${LLVM_BUILD_MODE} GONG_TOOLS_DIR ${LLVM_RUNTIME_OUTPUT_INTDIR})

configure_lit_site_cfg(
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.in
  ${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg
  )

option(GONG_TEST_USE_VG "Run Gong tests under Valgrind" OFF)
if(GONG_TEST_USE_VG)
  set(GONG_TEST_EXTRA_ARGS ${GONG_TEST_EXTRA_ARGS} "--vg")
endif ()

list(APPEND GONG_TEST_DEPS
  gong
  )

set(GONG_TEST_PARAMS
  gong_site_config=${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg
  )

list(APPEND GONG_TEST_DEPS
  llvm-config
  FileCheck count not
  llc opt llvm-symbolizer llvm-profdata llvm-objdump llvm-readobj
  )

add_lit_testsuite(check-gong "Running the Gong regression tests"
  ${CMAKE_CURRENT_BINARY_DIR}
  #LIT ${LLVM_LIT}
  PARAMS ${GONG_TEST_PARAMS}
  DEPENDS ${GONG_TEST_DEPS}
  ARGS ${GONG_TEST_EXTRA_ARGS}
  )
set_target_properties(check-gong PROPERTIES FOLDER "Gong tests")
