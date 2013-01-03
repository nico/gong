// RUN: %gong_cc1 -verify %s -sema

package p

func f() {
  a := 1  // expected-note {{previous definition is here}}
  b, c := 2, 3  // expected-note {{previous definition is here}}

  a := 1  // expected-diag {{redefinition of 'a'}}
  b := 1  // expected-diag {{redefinition of 'b'}}
}

func num() {
  a, b := 1, 2, 3 // expected-diag {{too many initializers, expected 2, have 3}}
  d, e, f := 1, 2  // expected-diag {{too few initializers, expected 3, have 2}}
}
