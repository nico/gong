//===--- Diagnostic.h - Go Diagnostic Handling ------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Defines the Diagnostic-related interfaces.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_GONG_DIAGNOSTIC_H
#define LLVM_GONG_DIAGNOSTIC_H

#include "gong/Basic/DiagnosticIDs.h"
#include "gong/Basic/SourceLocation.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/Support/type_traits.h"

#include <vector>
#include <list>

namespace llvm {
  class SourceMgr;
}

namespace gong {
class Diagnostic;
class DiagnosticConsumer;
class DiagnosticBuilder;
class IdentifierInfo;

/// \brief Concrete class used by the front-end to report problems and issues.
///
/// This massages the diagnostics (e.g. handling things like "report warnings
/// as errors" and passes them off to the DiagnosticConsumer for reporting to
/// the user. DiagnosticsEngine is tied to one translation unit and one
/// SourceMgr.
class DiagnosticsEngine : public RefCountedBase<DiagnosticsEngine> {
private:
  bool ShowColors;               // Color printing is enabled.
  unsigned ErrorLimit;           // Cap of # errors emitted, 0 -> no limit.
  IntrusiveRefCntPtr<DiagnosticIDs> Diags;
  DiagnosticConsumer *Client;
  bool OwnsDiagClient;
  llvm::SourceMgr *SourceMgr;

  /// \brief Sticky flag set to \c true when an error is emitted.
  bool ErrorOccurred;

  /// \brief Sticky flag set to \c true when a fatal error is emitted.
  bool FatalErrorOccurred;

  /// \brief Indicates that an unrecoverable error has occurred.
  bool UnrecoverableErrorOccurred;
  
  /// \brief Counts for DiagnosticErrorTrap to check whether an error occurred
  /// during a parsing section, e.g. during parsing a function.
  unsigned TrapNumErrorsOccurred;
  unsigned TrapNumUnrecoverableErrorsOccurred;

  unsigned NumDiags;         ///< Number of diags reported

public:
  explicit DiagnosticsEngine(
                       const IntrusiveRefCntPtr<DiagnosticIDs> &diags,
                       DiagnosticConsumer *client = 0,
                       bool ShouldOwnClient = true);
  ~DiagnosticsEngine();

  const IntrusiveRefCntPtr<DiagnosticIDs> &getDiagnosticIDs() const {
    return Diags;
  }

  DiagnosticConsumer *getClient() { return Client; }
  const DiagnosticConsumer *getClient() const { return Client; }

  /// \brief Determine whether this \c DiagnosticsEngine object own its client.
  bool ownsClient() const { return OwnsDiagClient; }
  
  /// \brief Return the current diagnostic client along with ownership of that
  /// client.
  DiagnosticConsumer *takeClient() {
    OwnsDiagClient = false;
    return Client;
  }

  bool hasSourceManager() const { return SourceMgr != 0; }
  llvm::SourceMgr &getSourceManager() const {
    assert(SourceMgr && "SourceManager not set!");
    return *SourceMgr;
  }
  void setSourceManager(llvm::SourceMgr *SrcMgr) { SourceMgr = SrcMgr; }

  //===--------------------------------------------------------------------===//
  //  DiagnosticsEngine characterization methods, used by a client to customize
  //  how diagnostics are emitted.
  //

  /// \brief Set the diagnostic client associated with this diagnostic object.
  ///
  /// \param ShouldOwnClient true if the diagnostic object should take
  /// ownership of \c client.
  void setClient(DiagnosticConsumer *client, bool ShouldOwnClient = true);

  /// \brief Specify a limit for the number of errors we should
  /// emit before giving up.
  ///
  /// Zero disables the limit.
  void setErrorLimit(unsigned Limit) { ErrorLimit = Limit; }
  
  /// \brief Set color printing, so the type diffing will inject color markers
  /// into the output.
  void setShowColors(bool Val = false) { ShowColors = Val; }
  bool getShowColors() { return ShowColors; }

  bool hasErrorOccurred() const { return ErrorOccurred; }
  bool hasFatalErrorOccurred() const { return FatalErrorOccurred; }
  
  /// \brief Determine whether any kind of unrecoverable error has occurred.
  bool hasUnrecoverableErrorOccurred() const {
    return FatalErrorOccurred || UnrecoverableErrorOccurred;
  }
  
  /// \brief Return an ID for a diagnostic with the specified message and level.
  ///
  /// If this is the first request for this diagnosic, it is registered and
  /// created, otherwise the existing ID is returned.
  //unsigned getCustomDiagID(Level L, StringRef Message) {
    //return Diags->getCustomDiagID((DiagnosticIDs::Level)L, Message);
  //}

  /// \brief Reset the state of the diagnostic object to its initial 
  /// configuration.
  void Reset();
  
  //===--------------------------------------------------------------------===//
  // DiagnosticsEngine classification and reporting interfaces.
  //

  /// \brief Issue the message to the client.
  ///
  /// This actually returns an instance of DiagnosticBuilder which emits the
  /// diagnostics (through @c ProcessDiag) when it is destroyed.
  ///
  /// \param DiagID A member of the @c diag::kind enum.
  /// \param Loc Represents the source location associated with the diagnostic,
  /// which can be an invalid location if no position information is available.
  inline void Report(SourceLocation Loc, unsigned DiagID);

  /// \brief Determine whethere there is already a diagnostic in flight.
  bool isDiagnosticInFlight() const { return CurDiagID != ~0U; }

  /// \brief Clear out the current diagnostic.
  void Clear() { CurDiagID = ~0U; }

private:
  /// \brief The location of the current diagnostic that is in flight.
  SourceLocation CurDiagLoc;

  /// \brief The ID of the current diagnostic that is in flight.
  ///
  /// This is set to ~0U when there is no diagnostic in flight.
  unsigned CurDiagID;

  friend class Diagnostic;
protected:
  /// \brief Emit the current diagnostic and clear the diagnostic state.
  ///
  /// \param Force Emit the diagnostic regardless of suppression settings.
  bool EmitCurrentDiagnostic(bool Force = false);

  unsigned getCurrentDiagID() const { return CurDiagID; }

  SourceLocation getCurrentDiagLoc() const { return CurDiagLoc; }
};

//===----------------------------------------------------------------------===//
// Diagnostic
//===----------------------------------------------------------------------===//

/// A little helper class (which is basically a smart pointer that forwards
/// info from DiagnosticsEngine) that allows clients to enquire about the
/// currently in-flight diagnostic.
class Diagnostic {
  const DiagnosticsEngine *DiagObj;
  StringRef StoredDiagMessage;
public:
  explicit Diagnostic(const DiagnosticsEngine *DO) : DiagObj(DO) {}
  Diagnostic(const DiagnosticsEngine *DO, StringRef storedDiagMessage)
    : DiagObj(DO), StoredDiagMessage(storedDiagMessage) {}

  const DiagnosticsEngine *getDiags() const { return DiagObj; }
  unsigned getID() const { return DiagObj->CurDiagID; }
  const SourceLocation &getLocation() const { return DiagObj->CurDiagLoc; }
  bool hasSourceManager() const { return DiagObj->hasSourceManager(); }
  llvm::SourceMgr &getSourceManager() const {
    return DiagObj->getSourceManager();
  }
  StringRef getMessage() const { return StoredDiagMessage; }
};

/// \brief Abstract interface, implemented by clients of the front-end, which
/// formats and prints fully processed diagnostics.
class DiagnosticConsumer {
protected:
  unsigned NumDiags;       ///< Number of diags reported
  
public:
  DiagnosticConsumer() : NumDiags(0) { }

  unsigned getNumDiags() const { return NumDiags; }
  virtual void clear() { NumDiags = 0; }

  virtual ~DiagnosticConsumer();

  /// \brief Callback to inform the diagnostic client that processing of all
  /// source files has ended.
  virtual void finish() {}

  /// \brief Handle this diagnostic, reporting it to the user or
  /// capturing it to a log as needed.
  ///
  /// The default implementation just keeps track of the total number of
  /// warnings and errors.
  virtual void handleDiagnostic(const Diagnostic &Info);
};

inline void DiagnosticsEngine::Report(SourceLocation Loc, unsigned DiagID) {
  assert(CurDiagID == ~0U && "Multiple diagnostics in flight at once!");
  CurDiagLoc = Loc;
  CurDiagID = DiagID;

  assert(Client && "DiagnosticConsumer not set!");
  Diagnostic Info(this, Diags->getDescription(DiagID));
  Client->handleDiagnostic(Info);

  CurDiagID = ~0U;
}

}  // end namespace gong

#endif

