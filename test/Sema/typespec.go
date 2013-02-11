// RUN: %gong_cc1 -verify %s -sema

package p

type A int  // expected-note {{previous definition is here}}
type B A

type A float32  // expected-diag {{redefinition of 'A'}}

type (
  C int  // expected-note {{previous definition is here}}
  C float32  // expected-diag {{redefinition of 'C'}}
)

func f() {  // expected-note {{func 'f' declared here}}
  var vfoo int  // expected-note {{var 'vfoo' declared here}}
  type vbar vfoo  // expected-diag {{'vfoo' does not name a type}}

  // FIXME: Needs both note and diag
  const cfoo = 4
  type cbar cfoo

  type fbar f  // expected-diag {{'f' does not name a type}}

  // FIXME: `type pbar pfoo` with pfoo a parameter decl,
  //                                   a method object,
  //                                   a return value name
}
