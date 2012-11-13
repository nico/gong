// RUN: %gong_cc1 -dump-tokens %s 2>&1 | FileCheck %s

// FIXME: check that '078' gives an error, but '078.' doesn't.

// http://golang.org/ref/spec#Integer_literals

// CHECK: numeric_literal '42'
42
// CHECK: numeric_literal '0600'
0600
// CHECK: numeric_literal '0xBadFace'
0xBadFace
// CHECK: numeric_literal '170141183460469231731687303715884105727'
170141183460469231731687303715884105727


// http://golang.org/ref/spec#Floating-point_literals

// CHECK: numeric_literal '0.'
0.
// CHECK: numeric_literal '72.40'
72.40
// CHECK: numeric_literal '072.40'
072.40  // == 72.40
// CHECK: numeric_literal '2.71828'
2.71828
// CHECK: numeric_literal '1.e+0'
1.e+0
// CHECK: numeric_literal '6.67428e-11'
6.67428e-11
// CHECK: numeric_literal '1E6'
1E6
// CHECK: numeric_literal '.25'
.25
// CHECK: numeric_literal '.12345E+5'
.12345E+5

// CHECK: numeric_literal '0x4'
// CHECK-NEXT: numeric_literal '.5'
0x4.5

// CHECK: numeric_literal '0x1234567e'
// CHECK-NEXT: plus '+'
// CHECK-NEXT: numeric_literal '1'
0x1234567e+1

// http://golang.org/ref/spec#Imaginary_literals
// CHECK: numeric_literal '0i'
0i
// CHECK: numeric_literal '011i'
011i  // == 11i
// CHECK: numeric_literal '0.i'
0.i
// CHECK: numeric_literal '2.71828i'
2.71828i
// CHECK: numeric_literal '1.e+0i'
1.e+0i
// CHECK: numeric_literal '6.67428e-11i'
6.67428e-11i
// CHECK: numeric_literal '1E6i'
1E6i
// CHECK: numeric_literal '.25i'
.25i
// CHECK: numeric_literal '.12345E+5i'
.12345E+5i

// CHECK: numeric_literal '42'
// CHECK-NEXT: identifier '_32'
42_32
