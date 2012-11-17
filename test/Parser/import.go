// RUN: %gong_cc1 -verify %s

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

// This checks for handling of eof. Don't put anything after it.
// expected-diag@+2 {{expected ';' after import line}}
import (
