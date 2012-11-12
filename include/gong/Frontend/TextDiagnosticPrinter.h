//===--- TextDiagnosticPrinter.h - Text Diagnostic Client -------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This is a concrete diagnostic client, which prints the diagnostics to
// standard error.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_GONG_FRONTEND_TEXT_DIAGNOSTIC_PRINTER_H_
#define LLVM_GONG_FRONTEND_TEXT_DIAGNOSTIC_PRINTER_H_

#include "gong/Basic/Diagnostic.h"

namespace gong {

class TextDiagnosticPrinter : public DiagnosticConsumer {
public:
  void handleDiagnostic(const Diagnostic &Info);
};

} // end namespace gong

#endif
