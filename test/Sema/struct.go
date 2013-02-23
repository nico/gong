// RUN: %gong_cc1 -verify %s -sema

package p

type foo int  // expected-note {{previous definition is here}}
type mytype struct {
  foo foo  // ok
}
type foo int  // expected-diag {{redefinition of 'foo'}}

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

type anon_foo struct {
  foo  // expected-note {{previous definition is here}}
  foo int  // expected-diag {{redefinition of 'foo'}}
}

type anon_pointer_foo struct {
  *foo  // expected-note {{previous definition is here}}
  foo int  // expected-diag {{redefinition of 'foo'}}
}

// FIXME: support recursive pointer types (but diag on recursive non-pointers):
type t3 struct {
  // FIXME: the next 2 lines should-diag {{recursive type 't3'}}
  x t3  // expected-diag {{use of undeclared identifier 't3'}}
  t3  // expected-diag {{use of undeclared identifier 't3'}}
}
type t3_pointer struct {
  // FIXME: the next 2 lines shouldn't diag.
  x *t3_pointer  // expected-diag {{use of undeclared identifier 't3_pointer'}}
  *t3_pointer  // expected-diag {{use of undeclared identifier 't3_pointer'}}
}

var myvar int  // expected-note 2 {{var 'myvar' declared here}}
type anon_var struct {
  myvar  // expected-diag {{'myvar' does not name a type}}
  *myvar  // expected-diag {{'myvar' does not name a type}}
}

// FIXME:
// field lookup
// promoted embedded fields
// recursive struct types (over multiple levels, too)
