// RUN: %gong_cc1 -verify %s -sema

package p

func noInitializers() {
  var vA int  // expected-note {{previous definition is here}}
  var vB v

  var vA float32  // expected-diag {{redefinition of 'vA'}}

  var (
    vC int  // expected-note {{previous definition is here}}
    vC float32  // expected-diag {{redefinition of 'vC'}}
  )
}

func withInitializers() {
  var vA int = 4  // expected-note {{previous definition is here}}
  var vB v = 4

  var vA float32 = 4  // expected-diag {{redefinition of 'vA'}}

  var (
    vC int = 4  // expected-note {{previous definition is here}}
    vC float32 = 4  // expected-diag {{redefinition of 'vC'}}
  )
}
