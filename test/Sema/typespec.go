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


var vA int = 4  // expected-note {{previous definition is here}}
var vB v = 4A

var vA float32 = 4  // expected-diag {{redefinition of 'vA'}}

var (
  vC int = 4  // expected-note {{previous definition is here}}
  vC float32 = 4  // expected-diag {{redefinition of 'vC'}}
)


const cA int = 4  // expected-note {{previous definition is here}}
const cB cA = 4

const cA float32 = 3  // expected-diag {{redefinition of 'cA'}}

const (
  cC int = 5 // expected-note {{previous definition is here}}
  cC float32 = 5  // expected-diag {{redefinition of 'cC'}}
)


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
