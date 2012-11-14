// RUN: %gong_cc1 -dump-tokens %s 2>&1 | FileCheck %s

// http://golang.org/ref/spec#Identifiers

// CHECK: identifier 'a'
a
// CHECK: identifier '_x9'
_x9
// CHECK: identifier 'ThisVariableIsExported'
ThisVariableIsExported

// CHECK: identifier 'αβ'
αβ

// CHECK: identifier '_00'
_00

// CHECK: numeric_literal '00'
00

// CHECK: identifier 'αβɣ'
αβɣ

// CHECK: identifier '_βɣ'
_βɣ

// CHECK: identifier '_βɣ0'
_βɣ0

// CHECK: numeric_literal '0'
// CHECK-NEXT: identifier '_βɣ'
0_βɣ

// CHECK: identifier 'ab੩'
ab੩

// CHECK: identifier 'aβ੩'
aβ੩

// CHECK: unknown '੩'
// CHECK: identifier 'aβ'
੩aβ

// CHECK: identifier 'α_β'
α_β

// CHECK: identifier 'a_b'
a_b
