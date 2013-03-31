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

func identical_struct_types() {
  var a0 struct{}
  var b0 struct{} = a0

  var a1 struct{ a int }
  var b1 struct{ a int } = a1

  var a2 struct{ a int }
  var b2 struct{ b int } = a2 // expected-diag {{variable of type <name> cannot be assigned an expression of type <name>}}

  var a3 struct{ a int "tag" }
  var b3 struct{ a int "tag" } = a3

  var a4 struct{ a int "tag" }
  var b4 struct{ a int "othertag" } = a4 // FIXME should-diag {{variable of type <name> cannot be assigned an expression of type <name>}}

  var a5 struct{ int }
  var b5 struct{ int } = a5

  var a6 struct{ int }
  var b6 struct{ *int } = a6 // FIXME should-diag {{variable of type <name> cannot be assigned an expression of type <name>}}

  var a7 struct{ *int }
  var b7 struct{ *int } = a7

  var a8 struct{ int "tag" }
  var b8 struct{ int "tag" } = a8

  var a9 struct{ int "tag" }
  var b9 struct{ int "othertag" } = a9  // FIXME should-diag {{variable of type <name> cannot be assigned an expression of type <name>}}

  var a10 struct { a, b int }
  var b10 struct { a int } = a10  // expected-diag {{variable of type <name> cannot be assigned an expression of type <name>}}

  var a11 struct { a, b int }
  var b11 struct { a int; b int } = a11

  var a12 struct { a, b int }
  var b12 struct { b int; a int } = a12  // expected-diag {{variable of type <name> cannot be assigned an expression of type <name>}}

  var a13 struct { a, b int }
  var b13 struct { b, a int } = a13  // expected-diag {{variable of type <name> cannot be assigned an expression of type <name>}}

  // FIXME: tests for structs with blank identifiers.

  // FIXME: Once packages are implemented, test for
  // "Lower-case field names from different packages are always different."

  // FIXME: Once recursive struct types work, add tests for these.
}
