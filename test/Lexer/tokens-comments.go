// RUN: %gong_cc1 -dump-tokens %s 2>&1 | FileCheck %s

// http://golang.org/ref/spec#Comments

// CHECK: identifier 'foo'
// \ at end of line doesn't escape a newline\
foo

// CHECK: identifier 'foo'
// CHECK: identifier 'bar'
foo
//*
/*/
*/
bar

// CHECK: star '*'
// CHECK: slash '/'
/*
/*/
*/
