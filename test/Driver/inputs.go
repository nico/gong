// RUN: not %gong nonexistent.go 2>&1 | FileCheck -check-prefix=DRV %s
// DRV: gong: error: No such file or directory: nonexistent.go

// Also check that non-driver errors are not prefixed with "gong:"
// RUN: not %gong %s 2>&1 | FileCheck -check-prefix=FE %s
// FE-NOT: gong:
// FE: error:
adsf
