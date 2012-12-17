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

func initfun() {
  var init = func() {}
  var init = func() {}  // should-diag {{duplicate declaration}}
}

func init()int {}  // should-diag{{invalid signature for 'init'}}
func (A) init() {}  // This is fine, huh.

func g() {
  myfun()
  init()  // should-diag {{cannot call initializer function}}
}

var _ int
var _ int  // Ok, the blank identifier doesn't introduce new bindings.
type _ int
type _ int

func (A) mymeth() {}
func (A) mymeth() {}  // should-diag {{duplicate declaration}}

func (B) mymeth() {}  // Fine.
