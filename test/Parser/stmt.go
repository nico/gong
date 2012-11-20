// RUN: %gong_cc1 -verify %s

package p

func f() {
  // Declaration
  //FIXME
  // GoStmt
  // FIXME: `go;` should diag
  go 4
  // ReturnStmt
  //return
  //return 4
  //return 4,  should diag
  //return 4,5
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
