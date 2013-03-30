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
