// RUN: %gong_cc1 -verify %s -sema
//expected-no-diagnostics

package p

type s struct {
  x, y int
}

func f() {
  type foo struct {
    bar int
  }
  var bar foo
}
