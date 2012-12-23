// RUN: %gong_cc1 -verify %s -sema

package p

type B A
type A int  // expected-note{{previous definition is here}}

type A float32  // expected-diag{{redefinition of 'A'}}
