// RUN: %gong_cc1 -verify %s -sema

package p

type A int  // expected-note {{previous definition is here}}
type B A

type A float32  // expected-diag {{redefinition of 'A'}}

type (
  C int  // expected-note 2 {{previous definition is here}}
  C float32  // expected-diag {{redefinition of 'C'}}
)

// FIXME: 'A' should be in the scope of the struct below.
// type A struct { next *A }

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
