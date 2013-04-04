// RUN: %gong_cc1 %s -sema 2>&1 | FileCheck %s
// Note: This doesn't run with -verify. This is a regression test for a crash
//       that only happened without -verify.

package p

func identical_struct_types() {

  // CHECK-NOT: Stack dump
  var a10 struct {}
  var b10 struct { a int } = a10

  var a11 struct { a, b int }
  var b11 struct { a int; b int } = a11

  type t18 struct{}
  type t18_2 t18
  var a18 struct{}
  var b18 t18_2 = a18
}
