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

#include "gong/AST/ASTContext.h"
#include "gong/AST/Decl.h"
#include "gong/AST/Expr.h"
#include "gong/Basic/Diagnostic.h"
#include "gong/Basic/DiagnosticIDs.h"
#include "gong/Basic/LLVM.h"
#include "gong/Basic/SourceManager.h"
#include "gong/Basic/TokenKinds.h"
#include "gong/Frontend/TextDiagnosticPrinter.h"
#include "gong/Frontend/VerifyDiagnosticConsumer.h"
#include "gong/Lex/Lexer.h"
#include "gong/Parse/Parser.h"
#include "gong/Sema/Sema.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
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
#include "llvm/Support/Process.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/TargetSelect.h"
#include <cctype>
using namespace gong;

namespace {

void DumpTokens(Lexer &L) {
  Token Tok;
  do {
    L.Lex(Tok);
    L.DumpToken(Tok, true);
    llvm::errs() << "\n";
  } while (Tok.isNot(tok::eof));
}

void ParseDiagnosticArgs(DiagnosticOptions &DiagOpts, int argc,
                         const char **argv) {
  DiagOpts.ShowColors = llvm::sys::Process::StandardErrHasColors();
  for (int i = 1; i < argc; ++i) {
    if (argv[i] == std::string("-fshow-column"))
      DiagOpts.ShowColumn = 1;
    else if (argv[i] == std::string("-fno-show-column"))
      DiagOpts.ShowColumn = 0;
    else if (argv[i] == std::string("-fshow-source-location"))
      DiagOpts.ShowLocation = 1;
    else if (argv[i] == std::string("-fno-show-source-location"))
      DiagOpts.ShowLocation = 0;
    else if (argv[i] == std::string("-fcaret-diagnostics"))
      DiagOpts.ShowCarets = 1;
    else if (argv[i] == std::string("-fno-caret-diagnostics"))
      DiagOpts.ShowCarets = 0;
    else if (argv[i] == std::string("-fdiagnostics-fixit-info"))
      DiagOpts.ShowFixits = 1;
    else if (argv[i] == std::string("-fno-diagnostics-fixit-info"))
      DiagOpts.ShowFixits = 0;
    else if (argv[i] == std::string("-fdiagnostics-print-source-range-info"))
      // clang warns about -fno-diagnostics-print-source-range-info, ignore it.
      DiagOpts.ShowSourceRanges = 1;
    else if (argv[i] == std::string("-fdiagnostics-parseable-fixits"))
      // clang warns about -fno-diagnostic-parseable-fixits, ignore it.
      DiagOpts.ShowParseableFixits = 1;
    else if (argv[i] == std::string("-fdiagnostics-format=gong"))
      DiagOpts.setFormat(DiagnosticOptions::Gong);
    else if (argv[i] == std::string("-fdiagnostics-format=msvc"))
      DiagOpts.setFormat(DiagnosticOptions::Msvc);
    else if (argv[i] == std::string("-fdiagnostics-format=vi"))
      DiagOpts.setFormat(DiagnosticOptions::Vi);
    else if (argv[i] == std::string("-fcolor-diagnostics"))
      DiagOpts.ShowColors = 1;
    else if (argv[i] == std::string("-fno-color-diagnostics"))
      DiagOpts.ShowColors = 0;
    else if (argv[i] == std::string("-verify"))
      DiagOpts.VerifyDiagnostics = 1;
    // FIXME: -ferror-limit=, -fmessage-length=, -ftabstop=
  }
}

}

int main(int argc_, const char **argv_) {
  llvm::sys::PrintStackTraceOnErrorSignal();
  llvm::PrettyStackTraceProgram X(argc_, argv_);

  llvm::InitializeAllTargets();

  bool dumpTokens = false;
  bool sema = false;
  bool stats = false;
  const char* FileName = NULL;
  for (int i = 1; i < argc_; ++i)
    if (argv_[i][0] != '-' || !argv_[i][1]) {
      FileName = argv_[i];
    } else if (argv_[i] == std::string("-dump-tokens")) {
      dumpTokens = true;
    } else if (argv_[i] == std::string("-sema")) {
      sema = true;  // This should become opt-out once sema is useful.
    } else if (argv_[i] == std::string("-print-stats")) {
      stats = true;
    }

  SourceManager SM;

  IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts = new DiagnosticOptions;
  ParseDiagnosticArgs(*DiagOpts, argc_, argv_);
  // Now we can create the DiagnosticsEngine with a properly-filled-out
  // DiagnosticOptions instance.
  TextDiagnosticPrinter *DiagClient
    = new TextDiagnosticPrinter(llvm::errs(), &*DiagOpts);

  IntrusiveRefCntPtr<DiagnosticIDs> DiagIDs(new DiagnosticIDs);
  DiagnosticsEngine Diags(DiagIDs, DiagClient);
  Diags.setSourceManager(&SM);

  if (FileName) {
    llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> Buffer =
        llvm::MemoryBuffer::getFileOrSTDIN(FileName);
    if (Buffer) {
      unsigned id = SM.AddNewSourceBuffer(std::move(*Buffer), llvm::SMLoc());
      Lexer L(Diags, SM, SM.getMemoryBuffer(id));

      if (DiagOpts->VerifyDiagnostics) {
        VerifyDiagnosticConsumer* verifier =
            new VerifyDiagnosticConsumer(Diags);
        Diags.setClient(verifier);  // Takes ownership.
        L.addCommentHandler(verifier);
      }

      if (dumpTokens)
        DumpTokens(L);
      else {
        if (sema) {
          if (stats) {
            Decl::EnableStatistics();
            Expr::EnableStatistics();
          }
          ASTContext Context(SM, L.getIdentifierTable());
          Sema ParseActions(L, Context);
          Parser P(L, ParseActions);
          P.ParseSourceFile();
          if (stats) {
            Context.PrintStats();
            Decl::PrintStats();
            Expr::PrintStats();
            L.getIdentifierTable().PrintStats();
            // FIXME: stats for sema, stmt
          }
        } else {
          MinimalAction ParseActions(L);
          Parser P(L, ParseActions);
          P.ParseSourceFile();
        }
        /*Token Tok;
        do {
          L.Lex(Tok);
        } while (Tok.isNot(tok::eof));*/
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
