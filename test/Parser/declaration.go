// RUN: %gong_cc1 -verify %s

package p

// ConstDecl
const foo
const foo, bar

const foo = 4
const foo = []int{1, 2, 3}

const foo, bar = 1, 2

const foo, bar int = 1, 2

const foo int 4  // expected-diag{{expected '='}}
const foo 4  // expected-diag{{expected '=' or type}}

//const { foo, bar }
//const {
//  bar int = 19; foo = 20
//  baz
//}
