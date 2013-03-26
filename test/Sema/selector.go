// RUN: %gong_cc1 -verify %s -sema

package p

// FIXME: instead of <name>, print real type descriptions in diags.

func err() {
  undefined.asdf  // expected-diag {{use of undeclared identifier 'undefined'}}

  // FIXME: These currently all cause crashes
  //'a'.b
  //"a".c
  //`a`.d
  //4 .a  // space so this isn't lexed as float
  //4.05c4 .a

  //(4 + 5).asdf
}

func f() {
  f.a  // expected-diag {{selector base type <name> is not a struct}}
}

func happy() {
  type foo struct {
    a, b, c int
    s struct { a int }
  }

  var a foo
  a.a
  a.b
  a.c
  a.d  // expected-diag{{no field 'd' in <type>}}
  a.s.a
  a.s.b  // expected-diag{{no field 'b' in <type>}}

  type bar foo
  type baz bar
  var abaz baz
  abaz.a
  abaz.b
  abaz.c
  abaz.d  // expected-diag{{no field 'd' in <type>}}

  const c foo = foo{}
  // FIXME: this should work, not diag
  c.a  // expected-diag {{selector base type <name> is not a struct}}

  (a).a
  (a).b  // FIXME: should-diag
  ((((((a)))))).a
  ((((((a)))))).b  // FIXME: should-diag

  // FIXME: should work, not crash
  //struct { a int }{}.a
  // FIXME: should-diag, not crash
  //struct{}{}.a
}

func happy_direct_type() {
  var a struct {
    a, b, c int
    s struct { a int }
  }
  a.a
  a.b
  a.c
  a.d  // expected-diag{{no field 'd' in <type>}}
  a.s.a
  a.s.b  // expected-diag{{no field 'b' in <type>}}

  (a).a
  (a).b  // FIXME: should-diag
  ((((((a)))))).a
  ((((((a)))))).b  // FIXME: should-diag
}

func happy_pointer() {
  type foo struct {
    a, b, c int
    s *struct { a int }
  }

  var a *foo
  a.a
  a.b
  a.c
  a.d  // expected-diag{{no field 'd' in <type>}}
  a.s.a
  a.s.b  // expected-diag{{no field 'b' in <type>}}

  (a).a
  (a).b  // FIXME: should-diag
  ((((((a)))))).a
  ((((((a)))))).b  // FIXME: should-diag

  // FIXME: should work, not crash
  //(&struct { a int }{}).a
}

func pointer_pointer() {
  // Check that lookups in pointer-to-pointer-to-struct diags.
  type foo struct {
    a, b, c int
    s struct { a int }
  }

  var a **foo
  a.a  // expected-diag {{selector base type <name> is not a struct}}
  a.b  // expected-diag {{selector base type <name> is not a struct}}
  a.c  // expected-diag {{selector base type <name> is not a struct}}
  a.d  // expected-diag {{selector base type <name> is not a struct}}
  a.s.a  // expected-diag {{selector base type <name> is not a struct}}
  a.s.b  // expected-diag {{selector base type <name> is not a struct}}

  (a).a  // expected-diag {{selector base type <name> is not a struct}}
  (a).b  // expected-diag {{selector base type <name> is not a struct}}
  ((((((a)))))).a  // expected-diag {{selector base type <name> is not a struct}}
  ((((((a)))))).b  // expected-diag {{selector base type <name> is not a struct}}

  // FIXME: should diag, not crash
  //(& &struct { a int }{}).a
}

// FIXME: embedded fields
// FIXME: ignore blank identifiers

func types() {
  // FIXME: These should all diag instead of crash.
  //[4]int{}.hi
  //[]int{}.ho
  //map[int]int{}.ho
  //(map[int]int).ho

  type foo int
  foo.bar  // expected-diag {{selector base type <name> is not a struct}}
}

func embedded_fields() {
  var a struct { int }
  a.int

  type str struct { str int }
  var b struct { str }
  b.str

  // FIXME: implement promoted fields.
  type promoted struct { a, b, c int }
  var c struct { promoted }
  c.a
  c.b
  c.c
  c.d  // expected-diag {{no field 'd' in <type>}}
  c.promoted
  c.promoted.a
  c.promoted.b
  c.promoted.c
  c.promoted.d  // expected-diag {{no field 'd' in <type>}}

  type promoted1 struct { a, b int }
  type promoted2 struct { a, c int }
  var d struct { promoted1; *promoted2 }
  d.a  // expected-diag {{name 'a' is ambiguous, could be any of promoted1, promoted2}}
  d.b
  d.c
  d.d  // expected-diag {{no field 'd' in <type>}}
  d.promoted1
  d.promoted1.a
  d.promoted1.b
  d.promoted1.c  // expected-diag {{no field 'c' in <type>}}
  d.promoted2
  d.promoted2.a
  d.promoted2.b  // expected-diag {{no field 'b' in <type>}}
  d.promoted2.c

  type pro1_d1 struct { a int }
  type pro2_d1 struct { a int }
  type pro1_d2 struct { pro1_d1 }
  type pro2_d2 struct { pro2_d1 }
  type tp struct { pro1_d2; pro2_d1 }
  var vp tp
  vp.a
  type tp2 struct { pro1_d2; pro2_d2 }
  var vp2 tp2
  vp2.a  // expected-diag {{name 'a' is ambiguous, could be any of pro1_d2.pro1_d1, pro2_d2.pro2_d1}}
}

// Test that the scope for anonymous fields is popped.
type mytype struct {}  // expected-note {{previous definition is here}}
type mystruct struct {
  mytype
}
type mytype bool  // expected-diag {{redefinition of 'mytype'}}
func embedded_fields_pop_scope() {
  var a mytype
}
