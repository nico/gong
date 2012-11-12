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

#include "gong/Basic/Diagnostic.h"
#include "gong/Basic/DiagnosticIDs.h"
#include "gong/Basic/LLVM.h"
#include "gong/Basic/TokenKinds.h"
#include "gong/Frontend/TextDiagnosticPrinter.h"
#include "gong/Frontend/VerifyDiagnosticConsumer.h"
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
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include <cctype>
using namespace gong;

void DumpTokens(Lexer &L) {
  Token Tok;
  do {
    L.Lex(Tok);
    L.DumpToken(Tok, true);
    llvm::errs() << "\n";
  } while (Tok.isNot(tok::eof));
}

int main(int argc_, const char **argv_) {
  llvm::sys::PrintStackTraceOnErrorSignal();
  llvm::PrettyStackTraceProgram X(argc_, argv_);

  llvm::InitializeAllTargets();

  bool dumpTokens = false;
  bool verify = false;
  const char* FileName = NULL;
  for (int i = 1; i < argc_; ++i)
    if (argv_[i][0] != '-' || (argv_[i][0] && !argv_[i][1])) {
      FileName = argv_[i];
    } else if (argv_[i] == std::string("-dump-tokens")) {
      dumpTokens = true;
    } else if (argv_[i] == std::string("-verify")) {
      verify = true;
    }

  llvm::SourceMgr SM;

  IntrusiveRefCntPtr<DiagnosticIDs> DiagIDs(new DiagnosticIDs);
  DiagnosticsEngine Diags(DiagIDs, new TextDiagnosticPrinter);
  Diags.setSourceManager(&SM);

  if (FileName) {
    OwningPtr<llvm::MemoryBuffer> NewBuf;
    llvm::MemoryBuffer::getFileOrSTDIN(FileName, NewBuf);
    if (NewBuf) {
      unsigned id = SM.AddNewSourceBuffer(NewBuf.take(), llvm::SMLoc());
      Lexer L(Diags, SM, SM.getMemoryBuffer(id));

      if (verify) {
        VerifyDiagnosticConsumer* verifier =
            new VerifyDiagnosticConsumer(Diags);
        Diags.setClient(verifier);  // Takes ownership.
        L.addCommentHandler(verifier);
      }

      if (dumpTokens)
        DumpTokens(L);
      else {
        Token Tok;
        do {
          L.Lex(Tok);
        } while (Tok.isNot(tok::eof));
      }

      Diags.getClient()->finish();
    }
  }

  llvm::llvm_shutdown();

  int Res = 0;

  if (Diags.getClient()->getNumDiags())
    Res = 1;

#ifdef _WIN32
  // Exit status should not be negative on Win32, unless abnormal termination.
  // Once abnormal termination was caught, negative status should not be
  // propagated.
  if (Res < 0)
    Res = 1;
#endif

  return Res;
}
