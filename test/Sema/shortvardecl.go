// RUN: %gong_cc1 -verify %s -sema

package p

func f() {
  a := 1  // expected-note {{previous definition is here}}
  b, c := 2, 3  // expected-note {{previous definition is here}}

  a := 1  // expected-diag {{redefinition of 'a'}}
  b := 1  // expected-diag {{redefinition of 'b'}}
}

