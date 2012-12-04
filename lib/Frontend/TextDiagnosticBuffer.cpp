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
void TextDiagnosticBuffer::handleDiagnostic(DiagnosticsEngine::Level Level,
                                            const Diagnostic &Info) {
  // Default implementation (Warnings/errors count).
  DiagnosticConsumer::handleDiagnostic(Level, Info);

  SmallString<100> Buf;
  Info.FormatDiagnostic(Buf);
  switch (Level) {
  case DiagnosticsEngine::Note:
    Notes.push_back(std::make_pair(Info.getLocation(), Buf.str()));
    break;
  case DiagnosticsEngine::Error:
    Errors.push_back(std::make_pair(Info.getLocation(), Buf.str()));
    break;
  }
}

void TextDiagnosticBuffer::FlushDiagnostics(DiagnosticsEngine &Diags) const {
  for (const_iterator it = err_begin(), ie = err_end(); it != ie; ++it)
    Diags.Report(SourceLocation(),
        Diags.getCustomDiagID(DiagnosticsEngine::Error, it->second.c_str()));
  for (const_iterator it = note_begin(), ie = note_end(); it != ie; ++it)
    Diags.Report(SourceLocation(),
        Diags.getCustomDiagID(DiagnosticsEngine::Note, it->second.c_str()));
}
