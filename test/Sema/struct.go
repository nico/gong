// RUN: %gong_cc1 -verify %s -sema

package p

type s struct {
  x, y int
}

func f() {
  type foo struct {
    bar int
  }
  var bar foo
}

type d struct {
  x int  // expected-note {{previous definition is here}}
  x int  // expected-diag {{redefinition of 'x'}}
}

// FIXME:
// anonymous fields
// field lookup
// promoted embedded fields
// recursive struct types (over multiple levels, too)
