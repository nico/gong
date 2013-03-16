// RUN: %gong_cc1 -verify %s -sema

package p

// FIXME: instead of <name>, print real type descriptions in diags.

func err() {
  // FIXME: These currently all cause crashes
  //'a'.b
  //"a".c
  //`a`.d
  //4 .a  // space so this isn't lexed as float
  //4.05c4 .a
  //undefined.asdf

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
  a.d  // expected-diag{{no field 'd'}}
  a.s.a
  a.s.b  // expected-diag{{no field 'b'}}

  type bar foo
  type baz bar
  var abaz baz
  abaz.a
  abaz.b
  abaz.c
  abaz.d  // expected-diag{{no field 'd'}}

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
  a.d  // expected-diag{{no field 'd'}}
  a.s.a
  a.s.b  // expected-diag{{no field 'b'}}

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
  a.d  // expected-diag{{no field 'd'}}
  a.s.a
  a.s.b  // expected-diag{{no field 'b'}}

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
  //a.s.a  FIXME: should-diag, not crash
  //a.s.b  FIXME: should-diag, not crash

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
