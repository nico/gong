// RUN: %gong_cc1 -verify %s -sema

package p

// FIXME: Remove once built-in iota works.
const iota = 4

func noInitializers() {
  const (
    cC int = iota  // expected-note {{previous definition is here}}
    cC             // expected-diag {{redefinition of 'cC'}}
  )
}

func withInitializers() {
  const cA int = 4  // expected-note {{previous definition is here}}
  const cB cA = 4

  const cA float32 = 3  // expected-diag {{redefinition of 'cA'}}

  const (
    cC int = 5 // expected-note {{previous definition is here}}
    cC float32 = 5  // expected-diag {{redefinition of 'cC'}}
  )
}
