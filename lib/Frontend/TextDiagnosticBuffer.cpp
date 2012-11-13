//===--- TextDiagnosticBuffer.cpp - Buffer Text Diagnostics ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This is a concrete diagnostic client, which buffers the diagnostic messages.
//
//===----------------------------------------------------------------------===//

#include "gong/Frontend/TextDiagnosticBuffer.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/ErrorHandling.h"
using namespace gong;

/// Store the errors that are reported.
void TextDiagnosticBuffer::handleDiagnostic(const Diagnostic &Info) {
  // Default implementation (Warnings/errors count).
  DiagnosticConsumer::handleDiagnostic(Info);

  Errors.push_back(std::make_pair(Info.getLocation(), Info.getID()));
}

void TextDiagnosticBuffer::FlushDiagnostics(DiagnosticsEngine &Diags) const {
  for (const_iterator it = err_begin(), ie = err_end(); it != ie; ++it)
    Diags.Report(it->first, it->second);
}
