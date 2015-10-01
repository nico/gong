set(LLVM_LINK_COMPONENTS
  Support
  )

add_gong_library(gongSema
  IdentifierResolver.cpp
  Sema.cpp
  SemaDecl.cpp
  SemaExpr.cpp
  SemaExprMember.cpp
  SemaLookup.cpp
  SemaStmt.cpp

  LINK_LIBS
  gongAST
  gongBasic
  gongLex
  )
