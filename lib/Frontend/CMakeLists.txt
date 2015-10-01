set(LLVM_LINK_COMPONENTS
  BitReader
  Option
  Support
  )

add_gong_library(gongFrontend
  DiagnosticRenderer.cpp
  TextDiagnostic.cpp
  TextDiagnosticBuffer.cpp
  TextDiagnosticPrinter.cpp
  VerifyDiagnosticConsumer.cpp

  LINK_LIBS
  gongAST
  gongBasic
  gongLex
  gongParse
  gongSema
  )
