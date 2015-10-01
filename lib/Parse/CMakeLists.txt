set(LLVM_LINK_COMPONENTS
  MC
  MCParser
  Support
  )

add_gong_library(gongParse
  MinimalAction.cpp
  ParseExpr.cpp
  ParseStmt.cpp
  Parser.cpp
  RAIIObjectsForParser.h

  LINK_LIBS
  gongAST
  gongBasic
  gongLex
  gongSema
  )
