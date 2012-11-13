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
    // Start position for diagnostics.
    enum {
      DIAG_UPPER_LIMIT         = 1000
    };

    class CustomDiagInfo;

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
private:
  /// \brief Information for uniquing and looking up custom diags.
  diag::CustomDiagInfo *CustomDiagInfo;

public:
  DiagnosticIDs();
  ~DiagnosticIDs();

  /// \brief Return an ID for a diagnostic with the specified message and level.
  ///
  /// If this is the first request for this diagnosic, it is registered and
  /// created, otherwise the existing ID is returned.
  unsigned getCustomDiagID(StringRef Message);

  //===--------------------------------------------------------------------===//
  // Diagnostic classification and reporting interfaces.
  //

  /// \brief Given a diagnostic ID, return a description of the issue.
  static StringRef getDescription(unsigned DiagID);

  /// \brief Get the set of all diagnostic IDs.
  //void getAllDiagnostics(llvm::SmallVectorImpl<diag::kind> &Diags) const;

private:
};

}  // end namespace gong

#endif

