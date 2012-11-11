//===-- driver.cpp - Gong Driver ------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This is the entry point to the gong driver; it is a thin wrapper
// for functionality in the Driver gong library.
//
//===----------------------------------------------------------------------===//

#include "gong/Basic/LLVM.h"
#include "gong/Basic/TokenKinds.h"
#include "gong/Lex/Lexer.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Regex.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/system_error.h"
#include <cctype>
using namespace gong;

int main(int argc_, const char **argv_) {
  llvm::sys::PrintStackTraceOnErrorSignal();
  llvm::PrettyStackTraceProgram X(argc_, argv_);

  llvm::InitializeAllTargets();

  if (argc_ < 2)
    return -1;

  OwningPtr<llvm::MemoryBuffer> NewBuf;
  llvm::MemoryBuffer::getFile(argv_[1], NewBuf);
  if (NewBuf) {
    Lexer L(NewBuf.get());
    Token Tok;
    while (Tok.isNot(tok::eof)) {
      L.Lex(Tok);
      printf("tok: %s\n", Tok.getName());
    }
  }

  llvm::llvm_shutdown();

  int Res = 0;

#ifdef _WIN32
  // Exit status should not be negative on Win32, unless abnormal termination.
  // Once abnormal termination was caught, negative status should not be
  // propagated.
  if (Res < 0)
    Res = 1;
#endif

  return Res;
}
