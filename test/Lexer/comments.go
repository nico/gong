// RUN: %gong_cc1 %s -verify
//expected-no-diagnostics

package p

// This is from go/src/pkg/runtime/defs_linux.go. It means that '/*' inside an
// existing block comment cannot be diag'd on.
/*
// Linux glibc and Linux kernel define different and conflicting
// definitions for struct sigaction, struct timespec, etc.
// We want the kernel ones, which are in the asm/* headers.
// But then we'd get conflicts when we include the system
// headers for things like ucontext_t, so that happens in
// a separate file, defs1.go.

#include <asm/posix_types.h>
#define size_t __kernel_size_t
#include <asm/signal.h>
#include <asm/siginfo.h>
#include <asm/mman.h>
*/

