// RUN: %gong_cc1 -verify %s -sema

package p

func f() {
  var ia, ib int

  // FIXME: make type printer print actual type names :-)
  var ba bool = ia // expected-diag {{variable of type <name> cannot be assigned an expression of type <name>}}

  var ic, id int = ia, ib

  // FIXME: needs diag:
  var ic0, id0 int = ia, ib, ic

  // FIXME: needs better diag:
  var ic1, id1, ie1 int = ia, ib // expected-diag {{variable of type <name> cannot be assigned an expression of type <name>}}

  var ie, if_ = ia, ib

  // FIXME: needs better diag:
  var iv int = int // expected-diag {{variable of type <name> cannot be assigned an expression of type <name>}}
}

func diag_on_different_name_types() {
  type t1 struct {}
  type t2 struct {}

  var a t1
  var b t2 = a // expected-diag {{variable of type <name> cannot be assigned an expression of type <name>}}
  var c t1 = a
}

func identical_pointer_types() {
  var a0 *int
  var b0 int = a0 // expected-diag {{variable of type <name> cannot be assigned an expression of type <name>}}
  var c0 *int = a0
  var d0 **int = a0 // expected-diag {{variable of type <name> cannot be assigned an expression of type <name>}}

  var a1 **int
  var b1 int = a1 // expected-diag {{variable of type <name> cannot be assigned an expression of type <name>}}
  var c1 *int = a1 // expected-diag {{variable of type <name> cannot be assigned an expression of type <name>}}
  var d1 **int = a1
}

