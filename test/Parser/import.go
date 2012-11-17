// RUN: %gong_cc1 -verify %s

package mypackage;

import "import1";

import . "import2";

import foo "import3";

import foo bar;  // expected-diag {{expected string literal}}

import "foo" bar;  // expected-diag {{expected ';' after import line}}
