// RUN: %gong_cc1 -verify %s -sema

package p

var vA int = 4  // expected-note {{previous definition is here}}
var vB v = 4A

var vA float32 = 4  // expected-diag {{redefinition of 'vA'}}

var (
  vC int = 4  // expected-note {{previous definition is here}}
  vC float32 = 4  // expected-diag {{redefinition of 'vC'}}
)
