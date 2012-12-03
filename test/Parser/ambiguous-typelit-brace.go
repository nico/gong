// RUN: %gong_cc1 -verify %s
// XFAIL: *

package testpackage

import "imported"

type mytype struct{}

func f() {
  var a mytype
  if a == mytype{} { }  // should-diag
  if a == (mytype{}) { }
  if (a == mytype{}) { }

  if mytype{} == a { }  // should-diag
  if (mytype{}) == a { }
  if (mytype{} == a) { }

  if a == imported.ptype{} { }  // should-diag
  if a == (imported.ptype{}) { }
  if (a == imported.ptype{}) { }

  if imported.ptype{} == a { }  // should-diag
  if (imported.ptype{}) == a { }
  if (imported.ptype{} == a) { }

  if a == mytype { }  // should-diag
  if a == (mytype) { }
  if (a == mytype) { }

  if mytype == a { }  // should-diag
  if (mytype) == a { }
  if (mytype == a) { }

  if a == imported.ptype { }  // should-diag
  if a == (imported.ptype) { }
  if (a == imported.ptype) { }

  if imported.ptype == a { }  // should-diag
  if (imported.ptype) == a { }
  if (imported.ptype == a) { }

  if ba := a == mytype{}; ba && a == (mytype{}) { }  // should-diag
  if ba := a == (mytype{}); ba && a == (mytype{}) { }

  // This is fine without parens.
  if a == struct{}{} { }
  if a == struct{}(a) { }
  if a == [4]int{} { }
  if a == [4]int([4]int{}) { }
  if a == [4]int() { }  // should-diag
  if a == map[int]int{} {}
  if a == map[int]int(a) {}
  if a == map[int]int() {}  // should-diag

  if i := (mytype{4}).f; i > 4 {}
  if i := mytype{4}.f; i > 4 {}  // should-diag


  for a == mytype{} { }  // should-diag
  for a == (mytype{}) { }
  for (a == mytype{}) { }

  for a == mytype { }  // should-diag
  for a == (mytype) { }
  for (a == mytype) { }

  for a == imported.ptype{} { }  // should-diag
  for a == (imported.ptype{}) { }
  for (a == imported.ptype{}) { }

  for a == imported.ptype { }  // should-diag
  for a == (imported.ptype) { }
  for (a == imported.ptype) { }

  for ba := a == mytype{}; ba && a == (mytype{}) ; { }  // should-diag
  for ba := a == (mytype{}); ba && a == (mytype{}) ; { }
  for ba := a == (mytype{}); ba && a == (mytype{}) ; ba = a == mytype{} { }  // should-diag
  for ba := a == (mytype{}); ba && a == (mytype{}) ; ba = a == (mytype{}) { }
  for ba := range mytype{} { }  // should-diag
  for ba := range (mytype{}) { }


  switch a == mytype{} { }  // should-diag
  switch a == (mytype{}) { }
  switch (a == mytype{}) { }

  switch a == mytype { }  // should-diag
  switch a == (mytype) { }
  switch (a == mytype) { }

  switch a == imported.ptype{} { }  // should-diag
  switch a == (imported.ptype{}) { }
  switch (a == imported.ptype{}) { }

  switch a == imported.ptype { }  // should-diag
  switch a == (imported.ptype) { }
  switch (a == imported.ptype) { }

  switch ba := a == mytype{}; ba && a == (mytype{}) { }  // should-diag
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
  switch { b := a == mytype{} }

  // gc doesn't warn about those, but the spec isn't very clear on these
  // (golang issue 4482):
  for a[mytype{4}.f] = range "foo" {}
  if 4 == a[mytype{4}.f] {}
  
}
