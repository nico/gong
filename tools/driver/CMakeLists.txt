set( LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Analysis
  CodeGen
  Core
  IPO
  InstCombine
  Instrumentation
  MC
  MCParser
  ObjCARCOpts
  Option
  ScalarOpts
  Support
  TransformUtils
  Vectorize
  )

add_gong_executable(gong
  driver.cpp
  )

target_link_libraries(gong
  gongBasic
  gongFrontend
  )
