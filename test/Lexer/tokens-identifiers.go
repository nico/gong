// RUN: %gong_cc1 -dump-tokens %s 2>&1 | FileCheck %s

// http://golang.org/ref/spec#Identifiers

// CHECK: identifier 'a'
a
// CHECK: identifier '_x9'
_x9
// CHECK: identifier 'ThisVariableIsExported'
ThisVariableIsExported

// FIXME: utf8
//αβ

// CHECK: identifier '_00'
_00

// CHECK: numeric_literal '00'
00
