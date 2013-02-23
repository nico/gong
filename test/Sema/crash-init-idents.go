// RUN: %gong_cc1 %s -sema 2>&1 | FileCheck %s
// CHECK-NOT: Assertion failed
package p

type t1 struct {
  x, y int
}

type t3 struct {
  x int
  *t3
}

type t4 struct {
  t1
}

type t2 struct {
  int
  *t4
}

func bla() {
  f := t2{}
  var g = t3{3, &g}
  type foo struct {
    s int
  }
  var s foo
}
