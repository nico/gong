// RUN: %gong_cc1 -verify %s -sema

package p

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
  // FIXME: update diag once function types exist!
  f.a  // expected-diag {{selector base type '<unknown type>' is not a struct}}
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
  a.d  // expected-diag {{no field 'd' in 'foo'}}
  a.s.a
  a.s.b  // expected-diag-re {{no field 'b' in '<struct.*/selector.go:26:7>'}}

  type bar foo
  type baz bar
  var abaz baz
  abaz.a
  abaz.b
  abaz.c
  abaz.d  // expected-diag{{no field 'd' in 'baz'}}

  const c foo = foo{}
  // FIXME: this should work, not diag
  c.a  // expected-diag {{selector base type '<unknown type>' is not a struct}}

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
  a.d // expected-diag-re {{no field 'd' in '<struct.*/selector.go:61:9>'}}
  a.s.a
  a.s.b // expected-diag-re {{no field 'b' in '<struct.*/selector.go:63:7>'}}

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
  a.d  // expected-diag{{no field 'd' in '*foo'}}
  a.s.a
  a.s.b // expected-diag-re {{no field 'b' in '\*<struct.*/selector.go:81:8>'}}

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
  a.a  // expected-diag {{selector base type '*foo' is not a struct}}
  a.b  // expected-diag {{selector base type '*foo' is not a struct}}
  a.c  // expected-diag {{selector base type '*foo' is not a struct}}
  a.d  // expected-diag {{selector base type '*foo' is not a struct}}
  a.s.a  // expected-diag {{selector base type '*foo' is not a struct}}
  a.s.b  // expected-diag {{selector base type '*foo' is not a struct}}

  (a).a  // expected-diag {{selector base type '*foo' is not a struct}}
  (a).b  // expected-diag {{selector base type '*foo' is not a struct}}
  ((((((a)))))).a  // expected-diag {{selector base type '*foo' is not a struct}}
  ((((((a)))))).b  // expected-diag {{selector base type '*foo' is not a struct}}

  // FIXME: should diag, not crash
  //(& &struct { a int }{}).a
}

// FIXME: ignore blank identifiers

func types() {
  // FIXME: These should all diag instead of crash.
  //[4]int{}.hi
  //[]int{}.ho
  //map[int]int{}.ho
  //(map[int]int).ho

  // FIXME: better diag
  type foo int
  foo.bar  // expected-diag {{selector base type '<unknown type>' is not a struct}}
}

func embedded_fields() {
  var a struct { int }
  a.int
  a.foo // expected-diag-re {{no field 'foo' in '<struct.*/selector.go:140:9>'}}

  type str struct { str int }
  var b struct { str }
  b.str

  type promoted struct { a, b, c int }
  var c struct { promoted }
  c.a
  c.b
  c.c
  c.d // expected-diag-re {{no field 'd' in '<struct.*/selector.go:149:9>'}}
  c.promoted
  c.promoted.a
  c.promoted.b
  c.promoted.c
  c.promoted.d  // expected-diag {{no field 'd' in 'promoted'}}

  type promoted1 struct { a, b int }  // expected-note {{could be in 'promoted1'}}
  type promoted2 struct { a, c int }  // expected-note {{could be in 'promoted2'}}
  var d struct { promoted1; *promoted2 }
  d.a  // expected-diag {{name 'a' is ambiguous}}
  d.b
  d.c
  d.d // expected-diag-re {{no field 'd' in '<struct.*/selector.go:162:9>'}}
  d.promoted1
  d.promoted1.a
  d.promoted1.b
  d.promoted1.c  // expected-diag {{no field 'c' in 'promoted1'}}
  d.promoted2
  d.promoted2.a
  d.promoted2.b  // expected-diag {{no field 'b' in '*promoted2'}}
  d.promoted2.c

  type pro1_d1 struct { a int }  // expected-note {{could be in 'pro1_d2.pro1_d1'}}
  type pro2_d1 struct { a int }  // expected-note {{could be in 'pro2_d2.pro2_d1'}}
  type pro1_d2 struct { pro1_d1 }
  type pro2_d2 struct { pro2_d1 }
  type tp struct { pro1_d2; pro2_d1 }
  var vp tp
  vp.a
  type tp2 struct { pro1_d2; pro2_d2 }
  var vp2 tp2
  vp2.a  // expected-diag {{name 'a' is ambiguous}}
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
