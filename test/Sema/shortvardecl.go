// RUN: %gong_cc1 -verify %s -sema

package p

func f() {
  a := 1  // expected-note {{'a' declared here}}
  b, c := 2, 3  // expected-note {{'b' declared here}}

  a := 1  // expected-diag {{no new variables declared}}
  b := 1  // expected-diag {{no new variables declared}}

  // http://tip.golang.org/ref/spec#Short_variable_declarations
  // "a short variable declaration may redeclare variables provided they were
  // originally declared in the same block with the same type, and at least
  // one of the non-blank variables is new."
  a, d := 1, 2

  // See here for a spec clarification request:
  // https://code.google.com/p/go/issues/detail?id=4612
  e, e := 1, 2  // expected-diag {{redefinition of 'e'}} expected-note {{previous definition is here}}
  a, f, f := 1, 2, 3  // expected-diag {{redefinition of 'f'}} expected-note {{previous definition is here}}
  g, a, a := 1, 2, 3
}

func num() {
  a, b := 1, 2, 3 // expected-diag {{too many initializers, expected 2, have 3}}
  d, e, f := 1, 2  // expected-diag {{too few initializers, expected 3, have 2}}
}
