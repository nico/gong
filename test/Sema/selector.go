// RUN: %gong_cc1 -verify %s -sema

package p

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
  // FIXME
  f.a  // should-diag
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
  a.d  // should-diag FIXME
  //a.s.a  // FIXME: should work, not crash
  //a.s.b  // FIXME: should-diag, not crash

  const c foo = foo{}
  c.a

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
  //a.s.a  // FIXME: should work, not crash
  //a.s.b  // FIXME: should-diag, not crash

  (a).a
  (a).b  // FIXME: should-diag
  ((((((a)))))).a
  ((((((a)))))).b  // FIXME: should-diag
}

func happy_pointer() {
  type foo struct {
    a, b, c int
    s struct { a int }
  }

  var a *foo
  a.a
  a.b
  a.c
  a.d  // should-diag FIXME
  //a.s.a  // FIXME: should work, not crash
  //a.s.b  // FIXME: should-diag, not crash

  (a).a
  (a).b  // FIXME: should-diag
  ((((((a)))))).a
  ((((((a)))))).b  // FIXME: should-diag

  // FIXME: should work, not crash
  //(&struct { a int }{}).a
}

func pointer_pointer() {
  // FIXME: These should all diag.
  type foo struct {
    a, b, c int
    s struct { a int }
  }

  var a **foo
  a.a
  a.b
  a.c
  a.d
  //a.s.a
  //a.s.b

  (a).a
  (a).b
  ((((((a)))))).a
  ((((((a)))))).b

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
  //foo.bar
}
