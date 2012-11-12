//===--- Diagnostic.cpp - Go Diagnostic Handling --------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements the Diagnostic-related interfaces.
//
//===----------------------------------------------------------------------===//

#include "gong/Basic/Diagnostic.h"
#include "gong/Basic/IdentifierTable.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CrashRecoveryContext.h"
#include <cctype>

using namespace gong;

DiagnosticsEngine::DiagnosticsEngine(
                       const IntrusiveRefCntPtr<DiagnosticIDs> &diags,
                       DiagnosticConsumer *client, bool ShouldOwnClient)
  : Diags(diags), Client(client),
    OwnsDiagClient(ShouldOwnClient), SourceMgr(0) {
  ShowColors = false;
  ErrorLimit = 0;
  Reset();
}

DiagnosticsEngine::~DiagnosticsEngine() {
  if (OwnsDiagClient)
    delete Client;
}

void DiagnosticsEngine::setClient(DiagnosticConsumer *client,
                                  bool ShouldOwnClient) {
  if (OwnsDiagClient && Client)
    delete Client;
  
  Client = client;
  OwnsDiagClient = ShouldOwnClient;
}

void DiagnosticsEngine::Reset() {
  ErrorOccurred = false;
  FatalErrorOccurred = false;
  UnrecoverableErrorOccurred = false;
  NumDiags = 0;
  CurDiagID = ~0U;
}

DiagnosticConsumer::~DiagnosticConsumer() {}

void DiagnosticConsumer::handleDiagnostic(const Diagnostic &Info) {
  ++NumDiags;
}
