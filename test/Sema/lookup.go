// RUN: %gong_cc1 -verify %s -sema

package p

func f() {
  4 * foo  // expected-diag {{use of undeclared identifier 'foo'}}
  foo := 3
  4 * foo
  {
    4 * foo
  }
}
