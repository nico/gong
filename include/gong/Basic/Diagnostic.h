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
#include "llvm/Support/type_traits.h"

#include <vector>
#include <list>

namespace llvm {
  class SourceMgr;
}

namespace gong {
class Diagnostic;
class DiagnosticBuilder;
class DiagnosticConsumer;
class IdentifierInfo;

class DeclContext;

/// \brief Annotates a diagnostic with some code that should be
/// inserted, removed, or replaced to fix the problem.
///
/// This kind of hint should be used when we are certain that the
/// introduction, removal, or modification of a particular (small!)
/// amount of code will correct a compilation error. The compiler
/// should also provide full recovery from such errors, such that
/// suppressing the diagnostic output can still result in successful
/// compilation.
class FixItHint {
public:
  /// \brief Code that should be replaced to correct the error. Empty for an
  /// insertion hint.
  CharSourceRange RemoveRange;

  /// \brief Code in the specific range that should be inserted in the insertion
  /// location.
  CharSourceRange InsertFromRange;

  /// \brief The actual code to insert at the insertion location, as a
  /// string.
  std::string CodeToInsert;

  bool BeforePreviousInsertions;

  /// \brief Empty code modification hint, indicating that no code
  /// modification is known.
  FixItHint() : BeforePreviousInsertions(false) { }

  bool isNull() const {
    return !RemoveRange.isValid();
  }
  
  /// \brief Create a code modification hint that inserts the given
  /// code string at a specific location.
  static FixItHint CreateInsertion(SourceLocation InsertionLoc,
                                   StringRef Code,
                                   bool BeforePreviousInsertions = false) {
    FixItHint Hint;
    Hint.RemoveRange =
        CharSourceRange::getCharRange(InsertionLoc, InsertionLoc);
    Hint.CodeToInsert = Code;
    Hint.BeforePreviousInsertions = BeforePreviousInsertions;
    return Hint;
  }
  
  /// \brief Create a code modification hint that inserts the given
  /// code from \p FromRange at a specific location.
  static FixItHint CreateInsertionFromRange(SourceLocation InsertionLoc,
                                            CharSourceRange FromRange,
                                        bool BeforePreviousInsertions = false) {
    FixItHint Hint;
    Hint.RemoveRange =
      CharSourceRange::getCharRange(InsertionLoc, InsertionLoc);
    Hint.InsertFromRange = FromRange;
    Hint.BeforePreviousInsertions = BeforePreviousInsertions;
    return Hint;
  }

  /// \brief Create a code modification hint that removes the given
  /// source range.
  static FixItHint CreateRemoval(CharSourceRange RemoveRange) {
    FixItHint Hint;
    Hint.RemoveRange = RemoveRange;
    return Hint;
  }
  static FixItHint CreateRemoval(SourceRange RemoveRange) {
    return CreateRemoval(CharSourceRange::getTokenRange(RemoveRange));
  }
  
  /// \brief Create a code modification hint that replaces the given
  /// source range with the given code string.
  static FixItHint CreateReplacement(CharSourceRange RemoveRange,
                                     StringRef Code) {
    FixItHint Hint;
    Hint.RemoveRange = RemoveRange;
    Hint.CodeToInsert = Code;
    return Hint;
  }
  
  static FixItHint CreateReplacement(SourceRange RemoveRange,
                                     StringRef Code) {
    return CreateReplacement(CharSourceRange::getTokenRange(RemoveRange), Code);
  }
};


/// \brief Concrete class used by the front-end to report problems and issues.
///
/// This massages the diagnostics (e.g. handling things like "report warnings
/// as errors" and passes them off to the DiagnosticConsumer for reporting to
/// the user. DiagnosticsEngine is tied to one translation unit and one
/// SourceMgr.
class DiagnosticsEngine : public RefCountedBase<DiagnosticsEngine> {
public:
  /// \brief The level of the diagnostic.
  enum Level {
    Note = DiagnosticIDs::Note,
    Error = DiagnosticIDs::Error
  };

  enum ArgumentKind {
    ak_std_string,      ///< std::string
    ak_c_string,        ///< const char *
    ak_sint,            ///< int
    ak_uint,            ///< unsigned
    ak_identifierinfo,  ///< IdentifierInfo

    ak_type,            ///< Type *
    ak_declcontext      ///< DeclContext *
  };

  /// \brief Represents on argument value, which is a union discriminated
  /// by ArgumentKind, with a value.
  typedef std::pair<ArgumentKind, intptr_t> ArgumentValue;

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
  //unsigned TrapNumErrorsOccurred;
  //unsigned TrapNumUnrecoverableErrorsOccurred;

  unsigned NumDiags;         ///< Number of diags reported

  /// \brief A function pointer that converts an opaque diagnostic
  /// argument to a strings.
  ///
  /// This takes the modifiers and argument that was present in the diagnostic.
  ///
  /// The PrevArgs array (whose length is NumPrevArgs) indicates the previous
  /// arguments formatted for this diagnostic.  Implementations of this function
  /// can use this information to avoid redundancy across arguments.
  ///
  /// This is a hack to avoid a layering violation between libbasic and libsema.
  typedef void (*ArgToStringFnTy)(
      ArgumentKind Kind, intptr_t Val,
      const char *Modifier, unsigned ModifierLen,
      const char *Argument, unsigned ArgumentLen,
      const ArgumentValue *PrevArgs,
      unsigned NumPrevArgs,
      SmallVectorImpl<char> &Output,
      void *Cookie,
      ArrayRef<intptr_t> QualTypeVals);
  void *ArgToStringCookie;
  ArgToStringFnTy ArgToStringFn;

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
  unsigned getCustomDiagID(Level L, StringRef Message) {
    return Diags->getCustomDiagID((DiagnosticIDs::Level)L, Message);
  }

  /// \brief Converts a diagnostic argument (as an intptr_t) into the string
  /// that represents it.
  void ConvertArgToString(ArgumentKind Kind, intptr_t Val,
                          const char *Modifier, unsigned ModLen,
                          const char *Argument, unsigned ArgLen,
                          const ArgumentValue *PrevArgs, unsigned NumPrevArgs,
                          SmallVectorImpl<char> &Output,
                          ArrayRef<intptr_t> QualTypeVals) const {
    ArgToStringFn(Kind, Val, Modifier, ModLen, Argument, ArgLen,
                  PrevArgs, NumPrevArgs, Output, ArgToStringCookie,
                  QualTypeVals);
  }

  void SetArgToStringFn(ArgToStringFnTy Fn, void *Cookie) {
    ArgToStringFn = Fn;
    ArgToStringCookie = Cookie;
  }

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
  inline DiagnosticBuilder Report(SourceLocation Loc, unsigned DiagID);

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

  enum {
    /// \brief The maximum number of arguments we can hold.
    ///
    /// We currently only support up to 10 arguments (%0-%9).  A single
    /// diagnostic with more than that almost certainly has to be simplified
    /// anyway.
    MaxArguments = 10,

    /// \brief The maximum number of ranges we can hold.
    MaxRanges = 10,

    /// \brief The maximum number of ranges we can hold.
    MaxFixItHints = 10
  };

  /// \brief The number of entries in Arguments.
  signed char NumDiagArgs;
  /// \brief The number of ranges in the DiagRanges array.
  unsigned char NumDiagRanges;
  /// \brief The number of hints in the DiagFixItHints array.
  unsigned char NumDiagFixItHints;

  /// \brief Specifies whether an argument is in DiagArgumentsStr or
  /// in DiagArguments.
  ///
  /// This is an array of ArgumentKind::ArgumentKind enum values, one for each
  /// argument.
  unsigned char DiagArgumentsKind[MaxArguments];

  /// \brief Holds the values of each string argument for the current
  /// diagnostic.
  ///
  /// This is only used when the corresponding ArgumentKind is ak_std_string.
  std::string DiagArgumentsStr[MaxArguments];

  /// \brief The values for the various substitution positions.
  ///
  /// This is used when the argument is not an std::string.  The specific
  /// value is mangled into an intptr_t and the interpretation depends on
  /// exactly what sort of argument kind it is.
  intptr_t DiagArgumentsVal[MaxArguments];

  /// \brief The list of ranges added to this diagnostic.
  CharSourceRange DiagRanges[MaxRanges];

  /// \brief If valid, provides a hint with some code to insert, remove,
  /// or modify at a particular position.
  FixItHint DiagFixItHints[MaxFixItHints];


  friend class Diagnostic;
  friend class DiagnosticBuilder;
protected:
  /// \brief Emit the current diagnostic and clear the diagnostic state.
  ///
  /// \param Force Emit the diagnostic regardless of suppression settings.
  bool EmitCurrentDiagnostic();

  unsigned getCurrentDiagID() const { return CurDiagID; }

  SourceLocation getCurrentDiagLoc() const { return CurDiagLoc; }
};

//===----------------------------------------------------------------------===//
// DiagnosticBuilder
//===----------------------------------------------------------------------===//

/// \brief A little helper class used to produce diagnostics.
///
/// This is constructed by the DiagnosticsEngine::Report method, and
/// allows insertion of extra information (arguments and source ranges) into
/// the currently "in flight" diagnostic.  When the temporary for the builder
/// is destroyed, the diagnostic is issued.
///
/// Note that many of these will be created as temporary objects (many call
/// sites), so we want them to be small and we never want their address taken.
/// This ensures that compilers with somewhat reasonable optimizers will promote
/// the common fields to registers, eliminating increments of the NumArgs field,
/// for example.
class DiagnosticBuilder {
  mutable DiagnosticsEngine *DiagObj;
  mutable unsigned NumArgs, NumRanges, NumFixits;

  /// \brief Status variable indicating if this diagnostic is still active.
  ///
  // NOTE: This field is redundant with DiagObj (IsActive iff (DiagObj == 0)),
  // but LLVM is not currently smart enough to eliminate the null check that
  // Emit() would end up with if we used that as our status variable.
  mutable bool IsActive;

  void operator=(const DiagnosticBuilder &) = delete;
  friend class DiagnosticsEngine;
  
  DiagnosticBuilder()
    : DiagObj(0), NumArgs(0), NumRanges(0), NumFixits(0), IsActive(false) { }

  explicit DiagnosticBuilder(DiagnosticsEngine *diagObj)
    : DiagObj(diagObj), NumArgs(0), NumRanges(0), NumFixits(0), IsActive(true)
      {
    assert(diagObj && "DiagnosticBuilder requires a valid DiagnosticsEngine!");
  }

  friend class PartialDiagnostic;
  
protected:
  void FlushCounts() {
    DiagObj->NumDiagArgs = NumArgs;
    DiagObj->NumDiagRanges = NumRanges;
    DiagObj->NumDiagFixItHints = NumFixits;
  }

  /// \brief Clear out the current diagnostic.
  void Clear() const {
    DiagObj = 0;
    IsActive = false;
  }

  /// \brief Determine whether this diagnostic is still active.
  bool isActive() const { return IsActive; }

  /// \brief Force the diagnostic builder to emit the diagnostic now.
  ///
  /// Once this function has been called, the DiagnosticBuilder object
  /// should not be used again before it is destroyed.
  ///
  /// \returns true if a diagnostic was emitted, false if the
  /// diagnostic was suppressed.
  bool Emit() {
    // If this diagnostic is inactive, then its soul was stolen by the copy ctor
    // (or by a subclass, as in SemaDiagnosticBuilder).
    if (!isActive()) return false;

    // When emitting diagnostics, we set the final argument count into
    // the DiagnosticsEngine object.
    FlushCounts();

    // Process the diagnostic.
    bool Result = DiagObj->EmitCurrentDiagnostic();

    // This diagnostic is dead.
    Clear();

    return Result;
  }
  
public:
  /// Copy constructor.  When copied, this "takes" the diagnostic info from the
  /// input and neuters it.
  DiagnosticBuilder(const DiagnosticBuilder &D) {
    DiagObj = D.DiagObj;
    IsActive = D.IsActive;
    D.Clear();
    NumArgs = D.NumArgs;
    NumRanges = D.NumRanges;
    NumFixits = D.NumFixits;
  }

  /// \brief Retrieve an empty diagnostic builder.
  static DiagnosticBuilder getEmpty() {
    return DiagnosticBuilder();
  }

  /// \brief Emits the diagnostic.
  ~DiagnosticBuilder() {
    Emit();
  }
  
  /// \brief Conversion of DiagnosticBuilder to bool always returns \c true.
  ///
  /// This allows is to be used in boolean error contexts (where \c true is
  /// used to indicate that an error has occurred), like:
  /// \code
  /// return Diag(...);
  /// \endcode
  operator bool() const { return true; }

  void AddString(StringRef S) const {
    assert(isActive() && "Clients must not add to cleared diagnostic!");
    assert(NumArgs < DiagnosticsEngine::MaxArguments &&
           "Too many arguments to diagnostic!");
    DiagObj->DiagArgumentsKind[NumArgs] = DiagnosticsEngine::ak_std_string;
    DiagObj->DiagArgumentsStr[NumArgs++] = S;
  }

  void AddTaggedVal(intptr_t V, DiagnosticsEngine::ArgumentKind Kind) const {
    assert(isActive() && "Clients must not add to cleared diagnostic!");
    assert(NumArgs < DiagnosticsEngine::MaxArguments &&
           "Too many arguments to diagnostic!");
    DiagObj->DiagArgumentsKind[NumArgs] = Kind;
    DiagObj->DiagArgumentsVal[NumArgs++] = V;
  }

  void AddSourceRange(const CharSourceRange &R) const {
    assert(isActive() && "Clients must not add to cleared diagnostic!");
    assert(NumRanges < DiagnosticsEngine::MaxRanges &&
           "Too many arguments to diagnostic!");
    DiagObj->DiagRanges[NumRanges++] = R;
  }

  void AddFixItHint(const FixItHint &Hint) const {
    assert(isActive() && "Clients must not add to cleared diagnostic!");
    assert(NumFixits < DiagnosticsEngine::MaxFixItHints &&
           "Too many arguments to diagnostic!");
    DiagObj->DiagFixItHints[NumFixits++] = Hint;
  }

  bool hasMaxRanges() const {
    return NumRanges == DiagnosticsEngine::MaxRanges;
  }
};

inline const DiagnosticBuilder &operator<<(const DiagnosticBuilder &DB,
                                           StringRef S) {
  DB.AddString(S);
  return DB;
}

inline const DiagnosticBuilder &operator<<(const DiagnosticBuilder &DB,
                                           const char *Str) {
  DB.AddTaggedVal(reinterpret_cast<intptr_t>(Str),
                  DiagnosticsEngine::ak_c_string);
  return DB;
}

inline const DiagnosticBuilder &operator<<(const DiagnosticBuilder &DB, int I) {
  DB.AddTaggedVal(I, DiagnosticsEngine::ak_sint);
  return DB;
}

inline const DiagnosticBuilder &operator<<(const DiagnosticBuilder &DB,
                                           bool I) {
  DB.AddTaggedVal(I, DiagnosticsEngine::ak_sint);
  return DB;
}

inline const DiagnosticBuilder &operator<<(const DiagnosticBuilder &DB,
                                           unsigned I) {
  DB.AddTaggedVal(I, DiagnosticsEngine::ak_uint);
  return DB;
}

inline const DiagnosticBuilder &operator<<(const DiagnosticBuilder &DB,
                                           const IdentifierInfo *II) {
  DB.AddTaggedVal(reinterpret_cast<intptr_t>(II),
                  DiagnosticsEngine::ak_identifierinfo);
  return DB;
}

// Adds a DeclContext to the diagnostic. The enable_if template magic is here
// so that we only match those arguments that are (statically) DeclContexts;
// other arguments that derive from DeclContext (e.g., RecordDecls) will not
// match.
template <typename T>
inline typename std::enable_if<std::is_same<T, DeclContext>::value,
                               const DiagnosticBuilder &>::type
operator<<(const DiagnosticBuilder &DB, T *DC) {
  DB.AddTaggedVal(reinterpret_cast<intptr_t>(DC),
                  DiagnosticsEngine::ak_declcontext);
  return DB;
}

inline const DiagnosticBuilder &operator<<(const DiagnosticBuilder &DB,
                                           const SourceRange &R) {
  DB.AddSourceRange(CharSourceRange::getTokenRange(R));
  return DB;
}

inline const DiagnosticBuilder &operator<<(const DiagnosticBuilder &DB,
                                           const CharSourceRange &R) {
  DB.AddSourceRange(R);
  return DB;
}
  
inline const DiagnosticBuilder &operator<<(const DiagnosticBuilder &DB,
                                           const FixItHint &Hint) {
  if (!Hint.isNull())
    DB.AddFixItHint(Hint);
  return DB;
}

inline DiagnosticBuilder DiagnosticsEngine::Report(SourceLocation Loc,
                                            unsigned DiagID){
  assert(CurDiagID == ~0U && "Multiple diagnostics in flight at once!");
  CurDiagLoc = Loc;
  CurDiagID = DiagID;
  return DiagnosticBuilder(this);
}

//===----------------------------------------------------------------------===//
// Diagnostic
//===----------------------------------------------------------------------===//

/// A little helper class (which is basically a smart pointer that forwards
/// info from DiagnosticsEngine) that allows clients to enquire about the
/// currently in-flight diagnostic.
class Diagnostic {
  const DiagnosticsEngine *DiagObj;
public:
  explicit Diagnostic(const DiagnosticsEngine *DO) : DiagObj(DO) {}

  const DiagnosticsEngine *getDiags() const { return DiagObj; }
  unsigned getID() const { return DiagObj->CurDiagID; }
  const SourceLocation &getLocation() const { return DiagObj->CurDiagLoc; }
  bool hasSourceManager() const { return DiagObj->hasSourceManager(); }
  llvm::SourceMgr &getSourceManager() const {
    return DiagObj->getSourceManager();
  }
  unsigned getNumArgs() const { return DiagObj->NumDiagArgs; }

  /// \brief Return the kind of the specified index.
  ///
  /// Based on the kind of argument, the accessors below can be used to get
  /// the value.
  ///
  /// \pre Idx < getNumArgs()
  DiagnosticsEngine::ArgumentKind getArgKind(unsigned Idx) const {
    assert(Idx < getNumArgs() && "Argument index out of range!");
    return (DiagnosticsEngine::ArgumentKind)DiagObj->DiagArgumentsKind[Idx];
  }

  /// \brief Return the provided argument string specified by \p Idx.
  /// \pre getArgKind(Idx) == DiagnosticsEngine::ak_std_string
  const std::string &getArgStdStr(unsigned Idx) const {
    assert(getArgKind(Idx) == DiagnosticsEngine::ak_std_string &&
           "invalid argument accessor!");
    return DiagObj->DiagArgumentsStr[Idx];
  }

  /// \brief Return the specified C string argument.
  /// \pre getArgKind(Idx) == DiagnosticsEngine::ak_c_string
  const char *getArgCStr(unsigned Idx) const {
    assert(getArgKind(Idx) == DiagnosticsEngine::ak_c_string &&
           "invalid argument accessor!");
    return reinterpret_cast<const char*>(DiagObj->DiagArgumentsVal[Idx]);
  }

  /// \brief Return the specified signed integer argument.
  /// \pre getArgKind(Idx) == DiagnosticsEngine::ak_sint
  int getArgSInt(unsigned Idx) const {
    assert(getArgKind(Idx) == DiagnosticsEngine::ak_sint &&
           "invalid argument accessor!");
    return (int)DiagObj->DiagArgumentsVal[Idx];
  }

  /// \brief Return the specified unsigned integer argument.
  /// \pre getArgKind(Idx) == DiagnosticsEngine::ak_uint
  unsigned getArgUInt(unsigned Idx) const {
    assert(getArgKind(Idx) == DiagnosticsEngine::ak_uint &&
           "invalid argument accessor!");
    return (unsigned)DiagObj->DiagArgumentsVal[Idx];
  }

  /// \brief Return the specified IdentifierInfo argument.
  /// \pre getArgKind(Idx) == DiagnosticsEngine::ak_identifierinfo
  const IdentifierInfo *getArgIdentifier(unsigned Idx) const {
    assert(getArgKind(Idx) == DiagnosticsEngine::ak_identifierinfo &&
           "invalid argument accessor!");
    return reinterpret_cast<IdentifierInfo*>(DiagObj->DiagArgumentsVal[Idx]);
  }

  /// \brief Return the specified non-string argument in an opaque form.
  /// \pre getArgKind(Idx) != DiagnosticsEngine::ak_std_string
  intptr_t getRawArg(unsigned Idx) const {
    assert(getArgKind(Idx) != DiagnosticsEngine::ak_std_string &&
           "invalid argument accessor!");
    return DiagObj->DiagArgumentsVal[Idx];
  }

  /// \brief Return the number of source ranges associated with this diagnostic.
  unsigned getNumRanges() const {
    return DiagObj->NumDiagRanges;
  }

  /// \pre Idx < getNumRanges()
  const CharSourceRange &getRange(unsigned Idx) const {
    assert(Idx < DiagObj->NumDiagRanges && "Invalid diagnostic range index!");
    return DiagObj->DiagRanges[Idx];
  }

  /// \brief Return an array reference for this diagnostic's ranges.
  ArrayRef<CharSourceRange> getRanges() const {
    return llvm::makeArrayRef(DiagObj->DiagRanges, DiagObj->NumDiagRanges);
  }

  unsigned getNumFixItHints() const {
    return DiagObj->NumDiagFixItHints;
  }

  const FixItHint &getFixItHint(unsigned Idx) const {
    assert(Idx < getNumFixItHints() && "Invalid index!");
    return DiagObj->DiagFixItHints[Idx];
  }

  const FixItHint *getFixItHints() const {
    return getNumFixItHints()? DiagObj->DiagFixItHints : 0;
  }

  /// \brief Format this diagnostic into a string, substituting the
  /// formal arguments into the %0 slots.
  ///
  /// The result is appended onto the \p OutStr array.
  void FormatDiagnostic(SmallVectorImpl<char> &OutStr) const;

  /// \brief Format the given format-string into the output buffer using the
  /// arguments stored in this diagnostic.
  void FormatDiagnostic(const char *DiagStr, const char *DiagEnd,
                        SmallVectorImpl<char> &OutStr) const;
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
  virtual void handleDiagnostic(DiagnosticsEngine::Level DiagLevel,
                                const Diagnostic &Info) = 0;
};

//inline void DiagnosticsEngine::Report(SourceLocation Loc, unsigned DiagID) {
//  assert(CurDiagID == ~0U && "Multiple diagnostics in flight at once!");
//  CurDiagLoc = Loc;
//  CurDiagID = DiagID;
//
//  assert(Client && "DiagnosticConsumer not set!");
//  Diagnostic Info(this, Diags->getDescription(DiagID));
//  Client->handleDiagnostic(Info);
//
//  CurDiagID = ~0U;
//}

}  // end namespace gong

#endif

