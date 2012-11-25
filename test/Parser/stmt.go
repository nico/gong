// RUN: %gong_cc1 -verify %s

package p

func f() {
  // Declaration
  //FIXME

  // LabeledStmt
  lab:
  lab: break

  // SimpleStmts, EmptyStmt:
  ;
  // SimpleStmts, ExpressionStmt:
  //FIXME
  // SimpleStmts, SendStmt:
  //FIXME
  // SimpleStmts, IncDecStmt:
  //FIXME
  // SimpleStmts, Assignment:
  a, b, c = 1, 2, 3
  //FIXME: a = 4
  //FIXME: a[i], a[j] = 4, 5
  // SimpleStmts, ShortVarDecl:
  a, b, c := 1, 2, 3
  a := 1
  //FIXME: nice diag for a[i] := 1 (just fixit to '=')

  // GoStmt
  // FIXME: `go;` should diag
  go 4
  // ReturnStmt
  return
  return 4
  //return 4,  // FIXME: should diag
  return 4,5
  //FIXME
  // BreakStmt
  break
  break foo
  // ContinueStmt
  continue
  continue foo
  // GotoStmt
  goto foo
  goto 4  // expected-diag{{expected identifier}}
  // FallthroughStmt
  fallthrough
  // Block
  //FIXME
  // IfStmt
  //FIXME
  // SwitchStmt
  //FIXME
  // ForStmt
  //FIXME
  // DeferStmt
  // FIXME: `defer;` should diag
  defer 4
}
