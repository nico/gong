add_subdirectory(utf)

set(LLVM_LINK_COMPONENTS support)

add_gong_library(gongLex
  Lexer.cpp

  LINK_LIBS
  gongBasic
  )
