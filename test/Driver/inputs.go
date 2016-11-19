// RUN: not %gong nonexistent.go 2>&1 | FileCheck %s
// CHECK: error: No such file or directory: nonexistent.go
