// RUN: %gong_cc1 -verify %s -sema

package p

const cA int = 4  // expected-note {{previous definition is here}}
const cB cA = 4

const cA float32 = 3  // expected-diag {{redefinition of 'cA'}}

const (
  cC int = 5 // expected-note {{previous definition is here}}
  cC float32 = 5  // expected-diag {{redefinition of 'cC'}}
)
