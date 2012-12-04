//===--- TextDiagnosticPrinter.cpp - Diagnostic Printer -------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This diagnostic client prints out their diagnostic messages.
//
//===----------------------------------------------------------------------===//

#include "gong/Frontend/TextDiagnosticPrinter.h"

#include "gong/Basic/SourceLocation.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/ADT/SmallString.h"
using namespace gong;

void TextDiagnosticPrinter::handleDiagnostic(DiagnosticsEngine::Level Level,
                                             const Diagnostic &Info) {
  // Default implementation (Diags count).
  DiagnosticConsumer::handleDiagnostic(Level, Info);

  // Render the diagnostic message into a temporary buffer eagerly. We'll use
  // this later as we print out the diagnostic to the terminal.
  SmallString<100> OutStr;
  Info.FormatDiagnostic(OutStr);

  SourceLocation Loc = Info.getLocation();
  llvm::SourceMgr &SM = Info.getSourceManager();
  llvm::SourceMgr::DiagKind Kind;
  switch (Level) {
  case DiagnosticsEngine::Note: Kind = llvm::SourceMgr::DK_Note; break;
  case DiagnosticsEngine::Error: Kind = llvm::SourceMgr::DK_Error; break;
  }
  SM.PrintMessage(Loc, Kind, OutStr.str());
}
