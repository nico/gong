// RUN: %gong_cc1 -verify %s 2>&1 | FileCheck %s

package p;

''  // expected-diag {{empty run literal}}

// CHECK: <unknown>:0: error: diagnostic expected but not seen: 
// CHECK:   Line 5: empty run literal
// CHECK: <unknown>:0: error: diagnostic seen but not expected: 
// CHECK:   Line 5: empty rune literal
