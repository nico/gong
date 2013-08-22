// RUN: not %gong_cc1 -verify %s 2>&1 | FileCheck %s

// expected-diag malformed
// expected-diag {{malformed
// expected-diag 1- {{malformed
// expected-diag-re {{*}}
// expected-no-diagnostics

// CHECK: Line 3: cannot find start ('{{[{][{]}}') of expected string
// CHECK: Line 4: cannot find end ('{{[}][}]}}') of expected string
// CHECK: Line 5: invalid range following '-' in expected string
// CHECK: Line 6: invalid expected regex: repetition-operator operand invalid
// CHECK: Line 7: 'expected-no-diagnostics' directive cannot follow other expected directives

