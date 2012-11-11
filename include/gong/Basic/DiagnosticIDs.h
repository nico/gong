//===--- DiagnosticIDs.h - Diagnostic IDs Handling --------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Defines the Diagnostic IDs-related interfaces.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_GONG_DIAGNOSTICIDS_H
#define LLVM_GONG_DIAGNOSTICIDS_H

#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/ADT/StringRef.h"
#include "gong/Basic/LLVM.h"

namespace gong {

  // Import the diagnostic enums themselves.
  namespace diag {
    /// \brief All of the diagnostics that can be emitted by the frontend.
    typedef unsigned kind;

    // Get typedefs for common diagnostics.
    enum {
#define DIAG(ENUM,DESC) ENUM,
#include "gong/Basic/DiagnosticLexKinds.def"
      NUM_BUILTIN_DIAGNOSTICS
#undef DIAG
    };
  }

/// \brief Used for handling and querying diagnostic IDs. Can be used and shared
/// by multiple Diagnostics for multiple translation units.
class DiagnosticIDs : public RefCountedBase<DiagnosticIDs> {
public:
public:
  DiagnosticIDs();
  ~DiagnosticIDs();

  //===--------------------------------------------------------------------===//
  // Diagnostic classification and reporting interfaces.
  //

  /// \brief Given a diagnostic ID, return a description of the issue.
  StringRef getDescription(unsigned DiagID) const;

  /// \brief Get the set of all diagnostic IDs.
  //void getAllDiagnostics(llvm::SmallVectorImpl<diag::kind> &Diags) const;

private:
};

}  // end namespace gong

#endif

