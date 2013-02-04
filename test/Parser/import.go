// RUN: %gong_cc1 -verify %s
// RUN: %gong_cc1 -fdiagnostics-parseable-fixits %s 2>&1 | FileCheck %s

package mypackage;

import "import1";

import . "import2";

import foo "import3";

import foo bar;  // expected-diag {{expected string literal}}

import "foo" bar;  // expected-diag {{expected ';' after import line}}

import ();

import (
  "import1";
  . "import1";
  foo "import1";
  _ "import1";
);

import (
  foo bar;  // expected-diag {{expected string literal}}
  "foo" bar;  // expected-diag {{expected ';' after import line}}
);

import ( foo "foo" )

import (
  package  // expected-diag {{expected '.' or identifier or string literal}}
)
import ( package )  // expected-diag {{expected '.' or identifier or string literal}}

// CHECK: fix-it:"{{.*}}":{[[@LINE+2]]:8-[[@LINE+2]]:8}:"\""
// CHECK: fix-it:"{{.*}}":{[[@LINE+1]]:11-[[@LINE+1]]:11}:"\""
import fmt  // expected-diag {{import path must be string literal}}

// This checks for handling of eof. Don't put anything after it.
// expected-diag@+2 {{expected ')'}} expected-note@+1 {{to match this '('}} expected-diag@+2 {{expected ';' after import line}}
import (
