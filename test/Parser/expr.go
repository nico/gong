// RUN: %gong_cc1 -verify %s

package p

type t [4]int;
type t ['4']int;
type t [""]int;

type t [func() {}]int;
type t [func a() {}]int;  // expected-diag{{expected '('}}
