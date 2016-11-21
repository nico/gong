// RUN: %gong_cc1 -verify %s -sema

package p

// FIXME: Remove once built-in true/false/nil work.
const true = 1 == 1
const false = 1 != 1
const nil = 0

// http://tip.golang.org/ref/spec#Blocks

type A int
type C int  // expected-note {{previous definition is here}}
func C() {}  // expected-diag {{redefinition of 'C'}}

func f() {
  type A int  // expected-note {{previous definition is here}}
  type A int  // expected-diag {{redefinition of 'A'}}

  {
    type A int  // expected-note {{previous definition is here}}
    type A int  // expected-diag {{redefinition of 'A'}}
    type D int
  }

  if true {
    type A int  // expected-note {{previous definition is here}}
    type A int  // expected-diag {{redefinition of 'A'}}
    type D int
  } else {
    type A int  // expected-note {{previous definition is here}}
    type A int  // expected-diag {{redefinition of 'A'}}
    type D int
  }

  for {
    type A int  // expected-note {{previous definition is here}}
    type A int  // expected-diag {{redefinition of 'A'}}
    type D int
  }

  switch {
    case true:
      type A int  // expected-note {{previous definition is here}}
      type A int  // expected-diag {{redefinition of 'A'}}
      type D int
    case false:
      type A int  // expected-note {{previous definition is here}}
      type A int  // expected-diag {{redefinition of 'A'}}
      type D int
  }

  a := 4  // FIXME: Remove! This is needed because the case below does a
          //        lookup for "a" on the lhs of ':=' :-/
          // See also https://code.google.com/p/go/issues/detail?id=4653
  select {
    default:
      type A int  // expected-note {{previous definition is here}}
      type A int  // expected-diag {{redefinition of 'A'}}
      type D int
    case a := <-chan int(nil):
      type A int  // expected-note {{previous definition is here}}
      type A int  // expected-diag {{redefinition of 'A'}}
      type D int
  }

  type D int
}

type D int

func scope_if() {
  a := 1  // expected-note {{declared here}}
  if a := 1; true {
    a // check that |a| is defined at this point.
    a := 1  // expected-note {{declared here}}
    a := 1  // expected-diag {{no new variables declared}}
  } else {
    a // check that |a| is defined at this point.
    a := 1  // expected-note {{declared here}}
    a := 1  // expected-diag {{no new variables declared}}
  }
  if b := 1; true {
    b // check that |b| is defined at this point.
  } else {
    b // check that |b| is defined at this point.
  }
  a := 1  // expected-diag {{no new variables declared}}
}

func scope_for() {
  a := 1  // expected-note {{declared here}}
  for a := 1; ; {
    a // check that |a| is defined at this point.
    a := 1  // expected-note {{declared here}}
    a := 1  // expected-diag {{no new variables declared}}
  }
  for b := 1; ; {
    b // check that |b| is defined at this point.
  }
  a := 1  // expected-diag {{no new variables declared}}
}

func scope_switch() {
  // ExprSwitchStmt
  a := 1  // expected-note {{declared here}}
  switch a := 1; {
  default:
    a // check that |a| is defined at this point.
    a := 1  // expected-note {{declared here}}
    a := 1  // expected-diag {{no new variables declared}}
  }
  switch b := 1; {
  default:
    b // check that |b| is defined at this point.
  }
  a := 1  // expected-diag {{no new variables declared}}

  // TypeSwitchStmt
  switch a := 1; b := a.(type) {
  default:
    b // check that |b| is defined at this point.
    b := 1  // expected-note {{declared here}}
    b := 1  // expected-diag {{no new variables declared}}
  }
}
