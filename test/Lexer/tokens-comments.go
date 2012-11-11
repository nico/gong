// RUN: %gong_cc1 -dump-tokens %s 2>&1 | FileCheck %s

// http://golang.org/ref/spec#Comments

// CHECK: identifier 'foo0'
// \ at end of line doesn't escape a newline\
foo0

// CHECK: identifier 'foo1'
// CHECK: identifier 'bar1'
foo1
//*
/*/
*/
bar1

// CHECK: star '*'
// CHECK: slash '/'
/*
/*/
*/

// \ at end of line doesn't escape newlines for block comments either
// CHECK: identifier 'foo2'
// CHECK-NOT: identifier 'bar2'
foo2
/*
*\
/
bar2
*/
