// RUN: %gong_cc1 %s -verify -fsyntax-only

// Test that we recover gracefully from conflict markers left in input files.

package p;

// diff3 style  expected-diag@+1 {{version control conflict marker in file}}
<<<<<<< .mine
var x = 4
|||||||
var x = 123
=======
var x = 17
>>>>>>> .r91107

// normal style  expected-diag@+1 {{version control conflict marker in file}}
<<<<<<< .mine
type y int
=======
type y struct {}
>>>>>>> .r91107

// Perforce style  expected-diag@+1 {{version control conflict marker in file}}
>>>> ORIGINAL conflict-marker.c#6
var z = 1
==== THEIRS conflict-marker.c#7
var z = 0
==== YOURS conflict-marker.c
var z = 2
<<<<

var y b;


func foo() int {
  return x + z
};  // FIXME ugh, remove
