//===--- TextDiagnosticPrinter.h - Text Diagnostic Client -------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This diagnositc client verifies diagnostics agains special comments.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_GONG_FRONTEND_TEXT_DIAGNOSTIC_PRINTER_H_
#define LLVM_GONG_FRONTEND_TEXT_DIAGNOSTIC_PRINTER_H_

#include "gong/Basic/Diagnostic.h"
#include "gong/Basic/DiagnosticOptions.h"

namespace gong {

class TextDiagnosticPrinter : public DiagnosticConsumer {
  raw_ostream &OS;
  llvm::IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts;

  /// A string to prefix to error messages.
  std::string Prefix;

  unsigned OwnsOutputStream : 1;

public:
  TextDiagnosticPrinter(raw_ostream &os, DiagnosticOptions *diags,
                        bool OwnsOutputStream = false);
  virtual ~TextDiagnosticPrinter();

  void handleDiagnostic(DiagnosticsEngine::Level Level, const Diagnostic &Info);
};

} // end namespace gong

#endif
