// RUN: %gong_cc1 -verify %s

package p

func f() {
  // Declaration
  //FIXME: var, const
  type t int

  // LabeledStmt
  lab:
  lab: break

  // SimpleStmts, EmptyStmt:
  ;

  // SimpleStmts, ExpressionStmt:
  a * b + c
  // FIXME: 4 + 5 * 4

  // SimpleStmts, SendStmt:
  a <- 5

  // SimpleStmts, IncDecStmt:
  a++
  a--

  // SimpleStmts, Assignment:
  a, b, c = 1, 2, 3
  a = 4
  a[i] = 4
  a[i], a[j] = 4, 5
  a += 4

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
  {}
  {
    a := 1
  }
  { a := 1 }
  { fallthrough a }  // expected-diag{{expected ';'}}

  // IfStmt
  if a := 4; 5 < 6 {
  }
  if a {
  }
  if a < b { }
  if a := 4; 5 < 6 {
  } else {
  }
  if a := 4; 5 < 6 {
  } else if b := 5; 6 < 7 {
  }
  // FIXME: if a := 4; {} should diag

  // SwitchStmt
  switch {}
  switch foo {}
  switch i := 0; foo {}
  switch { default: a = 4 }
  switch {
  case a+b, c+d:
    a += 4
    if a < b {}
    fallthrough
  default: continue
  }
  //FIXME: TypeSwitchStmts

  // SelectStmt
  select {}
  select {
  default:
  }
  select {
  default:
    return
  }
  select { default: fallthrough }
  select { default: fallthrough a }  //expected-diag {{expected ';'}}
  select {
  case a<-4:        // Send
  case <-4:         // Unnamed receive
  case a = <-4:     // Named receive
  case a := <-4:    // Named receive
  case a,b = <-4:   // Named receive
  case a,b := <-4:  // Named receive
  }
  select {
  case a, b:  // expected-diag{{expected ':=' or '='}}
  }

  // ForStmt
  for {}
  for a < b {}
  for ;; {}
  for a := 4;; {}
  for ; 5 < 6; {}
  for ;; a++ {}
  for a := 4; 5 < 6; {}
  for a := 4;; a++ {}
  for ; 5 < 6; a++ {}
  for a := 4; 5 < 6; a++ {}

  //FIXME
  //for a := range "hi" {}
  //for a = range "hi" {}
  //for a, b := range "hi" {}
  //for a, b= range "hi" {}

  // FIXME: Check that range isn't permitted in other exprs or other-styled
  // fors.  Not even as child of a range expression.
  // FIXME: Check simplestmts aren't allowed in the condition of a for clause.

  // DeferStmt
  // FIXME: `defer;` should diag
  defer 4
}
