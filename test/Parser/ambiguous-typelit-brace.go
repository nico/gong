// RUN: %gong_cc1 -verify %s

package testpackage

import "imported"

type mytype struct{}

func f() {
  // This test checks that composite typename literals between if/for/switch and
  // their body are only allowed when wrapped in parentheses.  The currently
  // emitted diagnostics should be better, but they are emitted on the right
  // lines.

  var a mytype
  if a == mytype{} { }  // expected-diag {{expected ';'}}
  if a == (mytype{}) { }
  if a == foo[mytype{}.intfield] { }

  if mytype{} == a { }  // expected-diag {{expected ';'}} expected-diag {{expected statement}}
  if (mytype{}) == a { }
  if (mytype{} == a) { }

  if a == imported.ptype{} { }  // expected-diag {{expected ';'}}
  if a == (imported.ptype{}) { }
  if (a == imported.ptype{}) { }

  if imported.ptype{} == a { }  // expected-diag {{expected ';'}} expected-diag {{expected statement}}
  if (imported.ptype{}) == a { }
  if (imported.ptype{} == a) { }

  // This could arguably emit a parse-time diagnostic. It's a sema-time
  // diagnostic in gc.
  if a == mytype { }
  if a == (mytype) { }
  if (a == mytype) { }

  if mytype == a { }
  if (mytype) == a { }
  if (mytype == a) { }

  // This could arguably emit a parse-time diagnostic. It's a sema-time
  // diagnostic in gc.
  if a == imported.ptype { }
  if a == (imported.ptype) { }
  if (a == imported.ptype) { }

  if imported.ptype == a { }
  if (imported.ptype) == a { }
  if (imported.ptype == a) { }

  // FIXME: Understand why these diags are produced.
  if ba := a == mytype{}; ba && a == (mytype{}) { }  // expected-diag {{expected expression}} expected-diag {{expected ';'}} expected-diag {{expected ';'}}
  if ba := a == (mytype{}); ba && a == (mytype{}) { }

  // This is fine without parens.
  if a == struct{}{} { }
  if a == struct{}(a) { }
  if a == [4]int{} { }
  if a == [4]int([4]int{}) { }
  if a == [4]int() { }  // FIXME should-diag{{expected expression in conversion}}
  if a == map[int]int{} {}
  if a == map[int]int(a) {}
  if a == map[int]int() {}  // FIXME should-diag{{expected expression in conversion}}

  if i := (mytype{4}).f; i > 4 {}
  if i := mytype{4}.f; i > 4 {}  // expected-diag {{expected expression}} expected-diag 2 {{expected ';'}} expected-diag {{expected statement}} expected-diag {{expected ';'}}


  for a == mytype{} { }  // expected-diag {{expected ';'}}
  for a == (mytype{}) { }
  for (a == mytype{}) { }

  // This could arguably emit a parse-time diagnostic. It's a sema-time
  // diagnostic in gc.
  for a == mytype { }
  for a == (mytype) { }
  for (a == mytype) { }

  for a == imported.ptype{} { }  // expected-diag {{expected ';'}}
  for a == (imported.ptype{}) { }
  for (a == imported.ptype{}) { }

  // This could arguably emit a parse-time diagnostic. It's a sema-time
  // diagnostic in gc.
  for a == imported.ptype { }
  for a == (imported.ptype) { }
  for (a == imported.ptype) { }

  for ba := a == mytype{}; ba && a == (mytype{}) ; { }  // expected-diag {{expected expression}}
  for ba := a == (mytype{}); ba && a == (mytype{}) ; { }
  for ba := a == (mytype{}); ba && a == (mytype{}) ; ba = a == mytype{} { }  // expected-diag {{expected ';'}}
  for ba := a == (mytype{}); ba && a == (mytype{}) ; ba = a == (mytype{}) { }
  for ba := range mytype{} { }  // expected-diag {{expected ';'}}
  for ba := range (mytype{}) { }


  switch a == mytype{} { }  // expected-diag {{expected ';'}}
  switch a == (mytype{}) { }
  switch (a == mytype{}) { }

  // This could arguably emit a parse-time diagnostic. It's a sema-time
  // diagnostic in gc.
  switch a == mytype { }
  switch a == (mytype) { }
  switch (a == mytype) { }

  switch a == imported.ptype{} { }  // expected-diag {{expected ';'}}
  switch a == (imported.ptype{}) { }
  switch (a == imported.ptype{}) { }

  // This could arguably emit a parse-time diagnostic. It's a sema-time
  // diagnostic in gc.
  switch a == imported.ptype { }
  switch a == (imported.ptype) { }
  switch (a == imported.ptype) { }

  switch ba := a == mytype{}; ba && a == (mytype{}) { }  // expected-diag {{expected expression or type switch guard}} expected-diag {{expected ';'}}
  switch ba := a == (mytype{}); ba && a == (mytype{}) { }


  // These should all be fine.
  b := a == mytype{}
  b := (a == mytype{})
  b := a == (mytype{})
  b := a == imported.ptype{}
  b := (a == imported.ptype{})
  b := a == (imported.ptype{})
  b := a == mytype    // Doesn't make sense semantically, but should parse fine.
  b := (a == mytype)  // Likewise.
  b := a == (mytype)  // Likewise.
  b := a == imported.ptype    // Likewise.
  b := (a == imported.ptype)  // Likewise.
  b := a == (imported.ptype)  // Likewise.
  switch {
    case a == mytype{}:
    case a == (mytype{}):
    case (a == mytype{}):
    case mytype{} == a:
    case (mytype{}) == a:
    case (mytype{} == a):
    case a == imported.ptype{}:
    case a == (imported.ptype{}):
    case (a == imported.ptype{}):
    case imported.ptype{} == a:
    case (imported.ptype{}) == a:
    case (imported.ptype{} == a):
    case a == mytype:
    case a == (mytype):
    case (a == mytype):
    case mytype == a:
    case (mytype) == a:
    case (mytype == a):
    case a == imported.ptype:
    case a == (imported.ptype):
    case (a == imported.ptype):
    case imported.ptype == a:
    case (imported.ptype) == a:
    case (imported.ptype == a):
  }
  if true { b := a == mytype{} }
  for { b := a == mytype{} }
  switch { default: b := a == mytype{} }

  // gc doesn't warn about those, but the spec isn't very clear on these
  // (golang issue 4482):
  for a[mytype{4}.f] = range "foo" {}
  if 4 == a[mytype{4}.f] {}
  
}

// Shouldn't crash.
func f() {
  i := struct{M *map[string]int}{}
  *i.M = map[string]int{}
  if true {}
}
