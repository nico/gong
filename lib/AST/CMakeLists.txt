set(LLVM_LINK_COMPONENTS support)

add_gong_library(gongAST
  ASTContext.cpp
  ASTDiagnostic.cpp
  Decl.cpp
  DeclBase.cpp
  Expr.cpp
  PromotedFields.cpp
  Stmt.cpp
  Type.cpp
  TypePrinter.cpp

  LINK_LIBS
  clangBasic
  clangLex
  )
