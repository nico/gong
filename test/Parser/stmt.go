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
  4 + 5 * 4

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
  // FIXME: These would be nicer as "op= needs single expression" or similar.
  a, b, c += 4, 5, 6  // expected-diag{{expected ':=' or '='}}
  a[i], b += 3, 4  // expected-diag{{expected '='}}
  a, b = range 4  // expected-diag{{'range' is only valid in for statements}}

  // SimpleStmts, ShortVarDecl:
  a, b, c := 1, 2, 3
  a := 1
  a[i] := 1  // expected-diag{{unexpected expression before ':='}}

  // GoStmt
  // FIXME: `go;` should diag
  go 4

  // ReturnStmt
  return
  return 4
  //return 4,  // FIXME: should-diag
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
  if ; a < 4 { a = 4 }
  if 4; true {}
  if '4'; true {}
  if "asdf"; true {}
  if `asdf`; true {}
  if +4; true {}
  if []int{1, 2}; true {}
  if func(){}(); true {}
  if chan int(4); true {}
  if interface{}(a); true {}
  if interface{}(a).(int); true {}
  // FIXME: if a := 4; {} // should-diag

  // SwitchStmt
  //   ExprSwitchStmt
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
  switch i := 0; j := 0 {} // expected-diag{{expected expression or type switch guard}}

  switch i.(int) {}  // Note: This is an ExprSwitchStmt, Not a TypeSwitchStmt
  //   TypeSwitchStmts
  switch a.(type) {}
  switch a := a.(type) {}
  switch a.(type)++ {}  // expected-diag{{unexpected '.(type)'}} expected-diag{{expected expression or type switch guard}}
  switch a.(type) + 4 {}  // expected-diag{{unexpected '.(type)'}}
  switch a = a.(type) {}  // expected-diag{{unexpected '.(type)'}} expected-diag{{expected expression or type switch guard}}
  switch a = 4; a := a.(type) {}
  switch a := 4; a := a.(type) {}
  switch a.(type); a.(type) {}  // expected-diag{{expected '{'}}
  switch a.(type).(type) {}  // expected-diag{{unexpected '.(type)'}}
  switch a.(int).(type) {}  // Valid!
  switch a.foo.(type) {}
  switch a[i].(type) {}
  switch a().(type) {}
  switch a.(type).(int) {}  // expected-diag{{unexpected '.(type)'}}
  switch a.(type).foo {}  // expected-diag{{unexpected '.(type)'}}
  switch a.(type)() {}  // expected-diag{{unexpected '.(type)'}}
  switch a.(type)[3] {}  // expected-diag{{unexpected '.(type)'}}
  switch a.(type), a.(type) {}  // expected-diag 2 {{unexpected '.(type)'}} expected-diag{{expected assignment operator}} expected-diag{{expected expression or type switch guard}}
  switch a.(type) := 4 {}  // expected-diag{{unexpected expression before ':='}} expected-diag{{unexpected '.(type)'}} expected-diag{{expected expression or type switch guard}}
  switch interface{}(4).(type) {}
  //FIXME: TypeSwitchCases


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

  for a := range "hi" {}
  for a = range "hi" {}
  for a, b := range "hi" {}
  for a, b = range "hi" {}
  for a[i] := range "hi" {}
  for a[i] = range "hi" {}
  for a[i], b[i] := range "hi" {}
  for a[i], b[i] = range "hi" {}
  for a[i], b[i] += range "hi" {}  // expected-diag{{expected ':=' or '='}}

  // FIXME: Check that range isn't permitted in other exprs or other-styled
  // fors.  Not even as child of a range expression.
  // FIXME: Check simplestmts aren't allowed in the condition of a for clause.

  // DeferStmt
  // FIXME: `defer;` should diag
  defer 4
}
