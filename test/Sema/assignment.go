// RUN: %gong_cc1 -verify %s -sema

// expected-no-diagnostics  FIXME

package p

func f() {
  var a, b int

  var c, d int = a, b

  var c0, d0 int = a, b, c

  var c1, d1, e1 int = a, b

  var e, f = a, b

  var v int = int  // FIXME: should-diag{{expected expression, got type}}
}
