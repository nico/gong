// RUN: %gong_cc1 -verify %s

package p

type t [4]int;
type t ['4']int;
type t [""]int;

type t [func() {}]int;
type t [func a() {}]int;  // expected-diag{{expected '('}}

type t [struct{}{}]int;
type t [[]int{}]int;
type t [[4]int{}]int;
type t [[...]int{}]int;
type t [map[string]int{}]int;


// suffix
type t ['4'.foo]int;
type t ['4'.([]int)]int;

type t ['4'[4]]int;
type t ['4'[:]]int;
type t ['4'[4:]]int;
type t ['4'[:4]]int;
type t ['4'[4:4]]int;

type t ["4"[:][0].(int)]int;

type t ['4'[]]int;  //expected-diag {{expected expression}}

type t ['4'.]int;  //expected-diag {{expected identifier or '('}}

type t ['4'.()]int;  //expected-diag {{expected type}}

// FIXME: The 2nd diag shouldn't be emitted.
type t ['4'.(int]int;  //expected-diag {{expected ')'}} expected-diag{{expected ']'}}

