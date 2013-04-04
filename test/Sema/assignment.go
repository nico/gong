// RUN: %gong_cc1 -verify %s -sema

package p

func f() {
  var ia, ib int

  var ba bool = ia // expected-diag {{variable of type 'bool' cannot be assigned an expression of type 'int'}}

  var ic, id int = ia, ib

  // FIXME: needs diag:
  var ic0, id0 int = ia, ib, ic

  // FIXME: needs diag:
  var ic1, id1, ie1 int = ia, ib

  var ie, if_ = ia, ib

  // FIXME: needs better diag:
  var iv int = int // expected-diag {{variable of type 'int' cannot be assigned an expression of type '<unknown type>'}}
}

func diag_on_different_name_types() {
  type t1 struct {}
  type t2 struct {}

  var a t1
  var b t2 = a // expected-diag {{variable of type 't2' cannot be assigned an expression of type 't1'}}
  var c t1 = a
}

func identical_pointer_types() {
  var a0 *int
  var b0 int = a0 // expected-diag {{variable of type 'int' cannot be assigned an expression of type '*int'}}
  var c0 *int = a0
  var d0 **int = a0 // expected-diag {{variable of type '**int' cannot be assigned an expression of type '*int'}}

  var a1 **int
  var b1 int = a1 // expected-diag {{variable of type 'int' cannot be assigned an expression of type '**int'}}
  var c1 *int = a1 // expected-diag {{variable of type '*int' cannot be assigned an expression of type '**int'}}
  var d1 **int = a1
}

func identical_struct_types() {
  var a0 struct{}
  var b0 struct{} = a0

  var a1 struct{ a int }
  var b1 struct{ a int } = a1

  var a2 struct{ a int }
  var b2 struct{ b int } = a2 // expected-diag-re {{variable of type '<struct.*/assignment.go:53:10>' cannot be assigned an expression of type '<struct.*/assignment.go:52:10>'}}

  var a3 struct{ a int "tag" }
  var b3 struct{ a int "tag" } = a3

  var a4 struct{ a int "tag" }
  var b4 struct{ a int "othertag" } = a4 // FIXME should-diag {{variable of type <type> cannot be assigned an expression of type <type>}}

  var a5 struct{ int }
  var b5 struct{ int } = a5

  var a6 struct{ int }
  var b6 struct{ *int } = a6 // expected-diag-re {{variable of type '<struct.*/assignment.go:65:10>' cannot be assigned an expression of type '<struct.*/assignment.go:64:10>'}}

  var a7 struct{ *int }
  var b7 struct{ *int } = a7

  var a8 struct{ int "tag" }
  var b8 struct{ int "tag" } = a8

  var a9 struct{ int "tag" }
  var b9 struct{ int "othertag" } = a9  // FIXME should-diag {{variable of type <type> cannot be assigned an expression of type <type>}}

  var a10 struct { a, b int }
  var b10 struct { a int } = a10 // expected-diag-re {{variable of type '<struct.*/assignment.go:77:11>' cannot be assigned an expression of type '<struct.*/assignment.go:76:11>'}}

  var a11 struct { a, b int }
  var b11 struct { a int; b int } = a11

  var a12 struct { a, b int }
  var b12 struct { b int; a int } = a12 // expected-diag-re {{variable of type '<struct.*/assignment.go:83:11>' cannot be assigned an expression of type '<struct.*/assignment.go:82:11>'}}

  var a13 struct { a, b int }
  var b13 struct { b, a int } = a13 // expected-diag-re {{variable of type '<struct.*/assignment.go:86:11>' cannot be assigned an expression of type '<struct.*/assignment.go:85:11>'}}

  var a14 struct { a struct{} };
  var b14 struct { a struct{} } = a14;

  var a15 struct { a *struct{} };
  var b15 struct { a *struct{} } = a15;

  type t16 struct {}
  type u16 t16
  var a16 t16
  var b16 u16 = a16  // expected-diag {{variable of type 'u16' cannot be assigned an expression of type 't16'}}

  type t17 struct{}
  var a17 struct{}
  var b17 t17 = a17

  type t18 struct{}
  type t18_2 t18
  var a18 struct{}
  var b18 t18_2 = a18

  type t19 struct{}
  type t19_2 struct{}
  var a19 t19
  var b19 t19_2 = a19  // expected-diag {{variable of type 't19_2' cannot be assigned an expression of type 't19'}}

  // FIXME: tests for structs with blank identifiers.

  // FIXME: Once packages are implemented, test for
  // "Lower-case field names from different packages are always different."

  // FIXME: Once recursive struct types work, add tests for these.
}

func short_var_decl() {
  var a int
  var b struct{}
  a, c := b, b // expected-diag-re {{variable of type 'int' cannot be assigned an expression of type '<struct.*/assignment.go:123:9>'}}
}
