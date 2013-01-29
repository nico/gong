// RUN: %gong_cc1 -verify %s

package p

// ConstDecl
const foo
const foo, bar
const foo int  // expected-diag{{expected '='}}

const foo = 4
const foo = []int{1, 2, 3}

const foo, bar = 1, 2
const foo, bar, = 1, 2  // expected-diag{{expected identifier}}

const foo, bar int = 1, 2

const foo int 4  // expected-diag{{expected '='}}
const foo 4  // expected-diag{{expected '=' or type}}

const ( foo )
const ( foo int )  // expected-diag{{expected '='}}
const ( foo, bar )
const ( foo, bar int )  // expected-diag{{expected '='}}
const (
  bar int = 19; foo = 20
  baz
)
const a = 4 4  // expected-diag-re{{expected ';'$}}
const (
  a = 4 4  // expected-diag{{expected ';' or ')'}}
)

const { foo, bar }  // expected-diag{{expected identifier or '('}}

func f() { const foo }
func f() { const foo int }  // expected-diag{{expected '='}}
func f() { const foo 4 }  // expected-diag{{expected '=' or type}}


// TypeDecls is covered by test/Parser/type.go.


// VarDecl
var foo  // expected-diag{{expected '=' or type}}
var foo, bar  // expected-diag{{expected '=' or type}}
var foo int

var foo = 4
var foo = []int{1, 2, 3}

var foo, bar = 1, 2
var foo, bar, = 1, 2  // expected-diag{{expected identifier}}
var foo, bar, []int  // expected-diag{{expected identifier}}

var foo, bar int = 1, 2

var foo int 4  // expected-diag{{expected '='}}
var foo 4  // expected-diag{{expected '=' or type}}

var ( foo ) // expected-diag{{expected '=' or type}}
var ( foo int )
var ( foo, bar ) // expected-diag{{expected '=' or type}}
var ( foo, bar int )
var (
  bar int = 19; foo = 20
  baz  // expected-diag{{expected '=' or type}}
)

func f() { var foo }  // expected-diag{{expected '=' or type}}
func f() { var foo int }
