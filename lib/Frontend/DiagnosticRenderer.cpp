//===--- DiagnosticRenderer.cpp - Diagnostic Pretty-Printing --------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "gong/Frontend/DiagnosticRenderer.h"
//#include "gong/Basic/DiagnosticOptions.h"
#include "gong/Basic/Diagnostic.h"
//#include "gong/Basic/FileManager.h"
//#include "gong/Basic/SourceManager.h"
//#include "gong/Edit/Commit.h"
//#include "gong/Edit/EditedSource.h"
//#include "gong/Edit/EditsReceiver.h"
#include "gong/Lex/Lexer.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
using namespace gong;

DiagnosticRenderer::DiagnosticRenderer(const LangOptions &LangOpts,
                                       DiagnosticOptions *DiagOpts)
  : LangOpts(LangOpts), /*DiagOpts(DiagOpts),*/ LastLevel() {}

DiagnosticRenderer::~DiagnosticRenderer() {}

void DiagnosticRenderer::emitDiagnostic(SourceLocation Loc,
                                        DiagnosticsEngine::Level Level,
                                        StringRef Message,
                                        ArrayRef<CharSourceRange> Ranges,
                                        ArrayRef<FixItHint> FixItHints,
                                        const SourceManager *SM,
                                        DiagOrStoredDiag D) {
  assert(SM || !Loc.isValid());

  beginDiagnostic(D, Level);

  if (!Loc.isValid())
    // If we have no source location, just emit the diagnostic message.
    emitDiagnosticMessage(Loc, PresumedLoc(), Level, Message, Ranges, SM, D);
  else {
    // Get the ranges into a local array we can hack on.
    SmallVector<CharSourceRange, 20> MutableRanges(Ranges.begin(),
                                                   Ranges.end());

    for (ArrayRef<FixItHint>::const_iterator I = FixItHints.begin(),
         E = FixItHints.end();
         I != E; ++I)
      if (I->RemoveRange.isValid())
        MutableRanges.push_back(I->RemoveRange);

    PresumedLoc PLoc; // FIXME = SM->getPresumedLoc(Loc);

    // Next, emit the actual diagnostic message and caret.
    emitDiagnosticMessage(Loc, PLoc, Level, Message, Ranges, SM, D);
    emitCaret(Loc, Level, MutableRanges, FixItHints, *SM);
  }

  LastLoc = Loc;
  LastLevel = Level;

  endDiagnostic(D, Level);
}

#if 0
void DiagnosticRenderer::emitStoredDiagnostic(StoredDiagnostic &Diag) {
  emitDiagnostic(Diag.getLocation(), Diag.getLevel(), Diag.getMessage(),
                 Diag.getRanges(), Diag.getFixIts(),
                 Diag.getLocation().isValid() ? &Diag.getLocation().getManager()
                                              : 0,
                 &Diag);
}

/// \brief Emit the module import stack associated with the current location.
void DiagnosticRenderer::emitImportStack(SourceLocation Loc,
                                         const SourceManager &SM) {
  if (Loc.isInvalid()) {
    //emitModuleBuildStack(SM);
    return;
  }

  std::pair<SourceLocation, StringRef> NextImportLoc
    = SM.getModuleImportLoc(Loc);
  emitImportStackRecursively(NextImportLoc.first, NextImportLoc.second, SM);
}

/// \brief Helper to recursivly walk up the import stack and print each layer
/// on the way back down.
void DiagnosticRenderer::emitImportStackRecursively(SourceLocation Loc,
                                                    StringRef ModuleName,
                                                    const SourceManager &SM) {
  if (Loc.isInvalid()) {
    return;
  }

  PresumedLoc PLoc = SM.getPresumedLoc(Loc);
  if (PLoc.isInvalid())
    return;

  // Emit the other import frames first.
  std::pair<SourceLocation, StringRef> NextImportLoc
    = SM.getModuleImportLoc(Loc);
  emitImportStackRecursively(NextImportLoc.first, NextImportLoc.second, SM);

  // Emit the inclusion text/note.
  emitImportLocation(Loc, PLoc, ModuleName, SM);
}

/// \brief Emit the module build stack, for cases where a module is (re-)built
/// on demand.
void DiagnosticRenderer::emitModuleBuildStack(const SourceManager &SM) {
  ModuleBuildStack Stack = SM.getModuleBuildStack();
  for (unsigned I = 0, N = Stack.size(); I != N; ++I) {
    const SourceManager &CurSM = Stack[I].second.getManager();
    SourceLocation CurLoc = Stack[I].second;
    emitBuildingModuleLocation(CurLoc,
                               CurSM.getPresumedLoc(CurLoc),
                               Stack[I].first,
                               CurSM);
  }
}
#endif

void DiagnosticRenderer::emitCaret(SourceLocation Loc,
                                   DiagnosticsEngine::Level Level,
                                   ArrayRef<CharSourceRange> Ranges,
                                   ArrayRef<FixItHint> Hints,
                                   const SourceManager &SM) {
  SmallVector<CharSourceRange, 4> SpellingRanges(Ranges.begin(), Ranges.end());
  emitCodeContext(Loc, Level, SpellingRanges, Hints, SM);
}

DiagnosticNoteRenderer::~DiagnosticNoteRenderer() {}

void DiagnosticNoteRenderer::emitIncludeLocation(SourceLocation Loc,
                                                 PresumedLoc PLoc,
                                                 const SourceManager &SM) {
  // Generate a note indicating the include location.
  SmallString<200> MessageStorage;
  llvm::raw_svector_ostream Message(MessageStorage);
  Message << "in file included from " << PLoc.getFilename() << ':'
          << PLoc.getLine() << ":";
  emitNote(Loc, Message.str(), &SM);
}

void DiagnosticNoteRenderer::emitImportLocation(SourceLocation Loc,
                                                PresumedLoc PLoc,
                                                StringRef ModuleName,
                                                const SourceManager &SM) {
  // Generate a note indicating the include location.
  SmallString<200> MessageStorage;
  llvm::raw_svector_ostream Message(MessageStorage);
  Message << "in module '" << ModuleName << "' imported from "
          << PLoc.getFilename() << ':' << PLoc.getLine() << ":";
  emitNote(Loc, Message.str(), &SM);
}

void
DiagnosticNoteRenderer::emitBuildingModuleLocation(SourceLocation Loc,
                                                   PresumedLoc PLoc,
                                                   StringRef ModuleName,
                                                   const SourceManager &SM) {
  // Generate a note indicating the include location.
  SmallString<200> MessageStorage;
  llvm::raw_svector_ostream Message(MessageStorage);
  Message << "while building module '" << ModuleName << "' imported from "
          << PLoc.getFilename() << ':' << PLoc.getLine() << ":";
  emitNote(Loc, Message.str(), &SM);
}


void DiagnosticNoteRenderer::emitBasicNote(StringRef Message) {
  emitNote(SourceLocation(), Message, 0);  
}
