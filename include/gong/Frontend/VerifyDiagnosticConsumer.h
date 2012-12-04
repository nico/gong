//===--- VerifyDiagnosticConsumer.h - Text Diagnostic Client ----*- C++ -*-===//
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

#ifndef LLVM_GONG_FRONTEND_VERIFY_DIAGNOSTIC_PRINTER_
#define LLVM_GONG_FRONTEND_VERIFY_DIAGNOSTIC_PRINTER_

#include "gong/Basic/Diagnostic.h"
#include "gong/Lex/Lexer.h"
#include "llvm/ADT/STLExtras.h"

namespace llvm {
  class SourceMgr;
}

namespace gong {
class TextDiagnosticBuffer;

class VerifyDiagnosticConsumer : public DiagnosticConsumer,
                                 public CommentHandler {
public:
  /// Abstract class representing a parsed verify directive.
  class Directive {
  public:
    static Directive *create(bool RegexKind, SourceLocation DirectiveLoc,
                             SourceLocation DiagnosticLoc,
                             StringRef Text, unsigned Min, unsigned Max);
  public:
    /// Constant representing n or more matches.
    static const unsigned MaxCount = UINT_MAX;

    SourceLocation DirectiveLoc;
    SourceLocation DiagnosticLoc;
    const std::string Text;
    unsigned Min, Max;

    virtual ~Directive() { }

    // Returns true if directive text is valid.
    // Otherwise returns false and populates E.
    virtual bool isValid(std::string &Error) = 0;

    // Returns true on match.
    virtual bool match(StringRef S) = 0;

  protected:
    Directive(SourceLocation DirectiveLoc, SourceLocation DiagnosticLoc,
              StringRef Text, unsigned Min, unsigned Max)
      : DirectiveLoc(DirectiveLoc), DiagnosticLoc(DiagnosticLoc),
        Text(Text), Min(Min), Max(Max) {
    assert(DirectiveLoc.isValid() && "DirectiveLoc is invalid!");
    assert(DiagnosticLoc.isValid() && "DiagnosticLoc is invalid!");
    }

  private:
    Directive(const Directive &) LLVM_DELETED_FUNCTION;
    void operator=(const Directive &) LLVM_DELETED_FUNCTION;
  };

  typedef std::vector<Directive*> DirectiveList;

  /// ExpectedData - owns directive objects and deletes on destructor.
  ///
  struct ExpectedData {
    DirectiveList Errors;
    DirectiveList Notes;

    ~ExpectedData() {
      llvm::DeleteContainerPointers(Errors);
      llvm::DeleteContainerPointers(Notes);
    }
  };

  enum DirectiveStatus {
    HasNoDirectives,
    HasNoDirectivesReported,
    HasExpectedNoDiagnostics,
    HasOtherExpectedDirectives
  };

private:
  DiagnosticsEngine &Diags;
  DiagnosticConsumer *PrimaryClient;
  bool OwnsPrimaryClient;
  OwningPtr<TextDiagnosticBuffer> Buffer;
  llvm::SourceMgr *SrcManager;
  DirectiveStatus Status;
  ExpectedData ED;

  void CheckDiagnostics();
  void setSourceManager(llvm::SourceMgr &SM) {
    assert((!SrcManager || SrcManager == &SM) && "SourceManager changed!");
    SrcManager = &SM;
  }

public:
  VerifyDiagnosticConsumer(DiagnosticsEngine &Diags);
  ~VerifyDiagnosticConsumer();

  // Implement DiagnosticConsumer
  void handleDiagnostic(DiagnosticsEngine::Level Level, const Diagnostic &Info);

  // Implement CommentHandler
  void handleComment(Lexer &L, SourceRange Range);
  void finish();
};

} // end namespace gong

#endif

