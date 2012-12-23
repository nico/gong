// RUN: %gong_cc1 -verify %s -sema
// XFAIL: *

package p

type B A
type A int

type A float32  // should-diag
