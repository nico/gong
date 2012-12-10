// RUN: %gong_cc1 -verify %s
// XFAIL: *

package p

// Fine.
type B A
type A int

func f() {
  type lB lA  // should-diag{{undefined 'lA'}}
  type lA int
}

// Fine.
type pBS struct { foo pBA }
type pBA struct { foo *pBS }

type BS struct { foo BA }
type BA struct { foo BS }  // should-diag {{invalid recursive type}}


func myfun() {}
func myfun() {}  // should-diag {{duplicate declaration}}

func init() {}
func init() {}  // This is fine :-/

func init()int {}  // should-diag{{invalid signature for 'init'}}
func (A) init() {}  // This is fine, huh.

func g() {
  myfun()
  init()  // should-diag {{cannot call initializer function}}
}
