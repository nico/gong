// RUN: %gong_cc1 -verify %s
// https://code.google.com/p/go/issues/detail?id=4501
//expected-no-diagnostics
package main

type I interface {
	m() int
}

type T struct{}

func (T) m() int { println("foo"); return 0 }

var t T

func main() {
	// these work
	var (
		_ = T.m(t)
		_ = (T).m(t)
	)
	T.m(t)
	(T).m(t)

	var (
		_ = I.m(t)
		_ = (I).m(t)
	)
	I.m(t)
	(I).m(t)
}

// these work
var (
	_ = T.m(t)
	_ = (T).m(t)
)

// these don't compile with 6g
var (
	_ = I.m(t)
	_ = (I).m(t)
)

