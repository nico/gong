// RUN: %gong_cc1 -verify %s -sema
//expected-no-diagnostics

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

  (a).a
  ((((((a)))))).a
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
  ((((((a)))))).a
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
  ((((((a)))))).a
}

// FIXME: embedded fields

func types() {
  // FIXME: These should all diag instead of crash.
  //[4]int{}.hi
  //[]int{}.ho
  //map[int]int{}.ho
  //(map[int]int).ho
}
