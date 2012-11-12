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
using namespace gong;

void TextDiagnosticPrinter::handleDiagnostic(const Diagnostic &Info) {
  // Default implementation (Diags count).
  DiagnosticConsumer::handleDiagnostic(Info);

  SourceLocation Loc = Info.getLocation();
  llvm::SourceMgr &SM = Info.getSourceManager();
  SM.PrintMessage(Loc, llvm::SourceMgr::DK_Error, Info.getMessage());
}
