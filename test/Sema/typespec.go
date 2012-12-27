// RUN: %gong_cc1 -verify %s -sema

package p

type A int  // expected-note {{previous definition is here}}
type B A

type A float32  // expected-diag {{redefinition of 'A'}}

type (
  C int  // expected-note {{previous definition is here}}
  C float32  // expected-diag {{redefinition of 'C'}}
)
