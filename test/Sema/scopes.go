// RUN: %gong_cc1 -verify %s -sema

package p

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
    // FIXME: check that |a| is defined at this point.
    a := 1  // expected-note {{declared here}}
    a := 1  // expected-diag {{no new variables declared}}
  } else {
    // FIXME: check that |a| is defined at this point.
    a := 1  // expected-note {{declared here}}
    a := 1  // expected-diag {{no new variables declared}}
  }
  a := 1  // expected-diag {{no new variables declared}}
}

func scope_for() {
  a := 1  // expected-note {{declared here}}
  for a := 1; ; {
    // FIXME: check that |a| is defined at this point.
    a := 1  // expected-note {{declared here}}
    a := 1  // expected-diag {{no new variables declared}}
  }
  a := 1  // expected-diag {{no new variables declared}}
}

func scope_switch() {
  a := 1  // expected-note {{declared here}}
  switch a := 1; {
  default:
    // FIXME: check that |a| is defined at this point.
    a := 1  // expected-note {{declared here}}
    a := 1  // expected-diag {{no new variables declared}}
  }
  a := 1  // expected-diag {{no new variables declared}}
}
