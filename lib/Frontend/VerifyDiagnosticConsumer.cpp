//===--- VerifyDiagnosticConsumer.cpp - Diagnostic Printer ----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This diagnostic client checks "// expected-diag {{foo}}" comments
//
//===----------------------------------------------------------------------===//

#include "gong/Frontend/VerifyDiagnosticConsumer.h"

#include "gong/Basic/SourceLocation.h"
#include "gong/Frontend/TextDiagnosticBuffer.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Regex.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include <cctype>

using namespace gong;
typedef VerifyDiagnosticConsumer::Directive Directive;
typedef VerifyDiagnosticConsumer::DirectiveList DirectiveList;
typedef VerifyDiagnosticConsumer::ExpectedData ExpectedData;

VerifyDiagnosticConsumer::VerifyDiagnosticConsumer(DiagnosticsEngine &Diags)
  : Diags(Diags),
    PrimaryClient(Diags.getClient()), OwnsPrimaryClient(Diags.ownsClient()),
    Buffer(new TextDiagnosticBuffer()),
    SrcManager(0), Status(HasNoDirectives)
{
  Diags.takeClient();
  assert(Diags.hasSourceManager());
  setSourceManager(Diags.getSourceManager());
}

VerifyDiagnosticConsumer::~VerifyDiagnosticConsumer() {
  SrcManager = 0;
  Diags.takeClient();
  if (OwnsPrimaryClient)
    delete PrimaryClient;
}

// DiagnosticConsumer interface.

void VerifyDiagnosticConsumer::handleDiagnostic(DiagnosticsEngine::Level Level,
                                                const Diagnostic &Info) {
  if (Info.hasSourceManager())
    setSourceManager(Info.getSourceManager());

  // Send the diagnostic to the buffer, we will check it once we reach the end
  // of the source file (or are destructed).
  Buffer->handleDiagnostic(Level, Info);
}

//===----------------------------------------------------------------------===//
// Checking diagnostics implementation.
//===----------------------------------------------------------------------===//

typedef TextDiagnosticBuffer::DiagList DiagList;
typedef TextDiagnosticBuffer::const_iterator const_diag_iterator;

namespace {

// FIXME: This doesn't belong here.
SourceLocation locForLine(const llvm::SourceMgr& SM, SourceLocation B,
                          unsigned Line) {
  int ID = SM.FindBufferContainingLoc(B);
  if (ID == -1)
    return SourceLocation();

  const llvm::MemoryBuffer *Buf = SM.getMemoryBuffer(ID);
  const char *Cur = Buf->getBufferStart();
  const char *End = Buf->getBufferEnd();

  unsigned CurLine = 1;
  while (CurLine != Line && Cur != End) {
    if (*Cur == '\n')
      ++CurLine;
    ++Cur;
  }
  if (CurLine == Line)
    return SourceLocation::getFromPointer(Cur);
  return SourceLocation();
}

/// StandardDirective - Directive with string matching.
///
class StandardDirective : public Directive {
public:
  StandardDirective(SourceLocation DirectiveLoc, SourceLocation DiagnosticLoc,
                    StringRef Text, unsigned Min, unsigned Max)
    : Directive(DirectiveLoc, DiagnosticLoc, Text, Min, Max) { }

  virtual bool isValid(std::string &Error) {
    // all strings are considered valid; even empty ones
    return true;
  }

  virtual bool match(StringRef S) {
    return S.find(Text) != StringRef::npos;
  }
};

/// RegexDirective - Directive with regular-expression matching.
///
class RegexDirective : public Directive {
public:
  RegexDirective(SourceLocation DirectiveLoc, SourceLocation DiagnosticLoc,
                 StringRef Text, unsigned Min, unsigned Max)
    : Directive(DirectiveLoc, DiagnosticLoc, Text, Min, Max), Regex(Text) { }

  virtual bool isValid(std::string &Error) {
    if (Regex.isValid(Error))
      return true;
    return false;
  }

  virtual bool match(StringRef S) {
    return Regex.match(S);
  }

private:
  llvm::Regex Regex;
};

class ParseHelper
{
public:
  ParseHelper(StringRef S)
    : Begin(S.begin()), End(S.end()), C(Begin), P(Begin), PEnd(NULL) { }

  // Return true if string literal is next.
  bool Next(StringRef S) {
    P = C;
    PEnd = C + S.size();
    if (PEnd > End)
      return false;
    return !memcmp(P, S.data(), S.size());
  }

  // Return true if number is next.
  // Output N only if number is next.
  bool Next(unsigned &N) {
    unsigned TMP = 0;
    P = C;
    for (; P < End && P[0] >= '0' && P[0] <= '9'; ++P) {
      TMP *= 10;
      TMP += P[0] - '0';
    }
    if (P == C)
      return false;
    PEnd = P;
    N = TMP;
    return true;
  }

  // Return true if string literal is found.
  // When true, P marks begin-position of S in content.
  bool Search(StringRef S, bool EnsureStartOfWord = false) {
    do {
      P = std::search(C, End, S.begin(), S.end());
      PEnd = P + S.size();
      if (P == End)
        break;
      if (!EnsureStartOfWord
            // Check if string literal starts a new word.
            || P == Begin || isspace(P[-1])
            // Or it could be preceeded by the start of a comment.
            || (P > (Begin + 1) && (P[-1] == '/' || P[-1] == '*')
                                &&  P[-2] == '/'))
        return true;
      // Otherwise, skip and search again.
    } while (Advance());
    return false;
  }

  // Advance 1-past previous next/search.
  // Behavior is undefined if previous next/search failed.
  bool Advance() {
    C = PEnd;
    return C < End;
  }

  // Skip zero or more whitespace.
  void SkipWhitespace() {
    for (; C < End && isspace(*C); ++C)
      ;
  }

  // Return true if EOF reached.
  bool Done() {
    return !(C < End);
  }

  const char * const Begin; // beginning of expected content
  const char * const End;   // end of expected content (1-past)
  const char *C;            // position of next char in content
  const char *P;

private:
  const char *PEnd; // previous next/search subject end (1-past)
};

} // namespace anonymous

/// Go through the comment and see if it indicates expected
/// diagnostics. If so, then put them in the appropriate directive list.
///
/// Returns true if any valid directives were found.
static bool ParseDirective(StringRef S, ExpectedData *ED,
                           const llvm::SourceMgr &SM,
                           SourceLocation Pos, DiagnosticsEngine &Diags,
                           VerifyDiagnosticConsumer::DirectiveStatus &Status) {
  // A single comment may contain multiple directives.
  bool FoundDirective = false;
  for (ParseHelper PH(S); !PH.Done();) {
    // Search for token: expected
    if (!PH.Search("expected", true))
      break;
    PH.Advance();

    // Next token: -
    if (!PH.Next("-"))
      continue;
    PH.Advance();

    // Next token: { error | warning | note }
    DirectiveList* DL = NULL;
    if (PH.Next("diag"))
      DL = ED ? &ED->Errors : NULL;
    else if (PH.Next("note"))
      DL = ED ? &ED->Notes : NULL;
    else if (PH.Next("no-diagnostics")) {
      if (Status == VerifyDiagnosticConsumer::HasOtherExpectedDirectives)
        Diags.Report(Pos, diag::verify_invalid_no_diags)
          << /*IsExpectedNoDiagnostics=*/true;
      else
        Status = VerifyDiagnosticConsumer::HasExpectedNoDiagnostics;
      continue;
    } else
      continue;
    PH.Advance();

    if (Status == VerifyDiagnosticConsumer::HasExpectedNoDiagnostics) {
      Diags.Report(Pos, diag::verify_invalid_no_diags)
          << /*IsExpectedNoDiagnostics=*/false;
      continue;
    }
    Status = VerifyDiagnosticConsumer::HasOtherExpectedDirectives;

    // If a directive has been found but we're not interested
    // in storing the directive information, return now.
    if (!DL)
      return true;

    // Default directive kind.
    bool RegexKind = false;
    const char* KindStr = "string";

    // Next optional token: -
    if (PH.Next("-re")) {
      PH.Advance();
      RegexKind = true;
      KindStr = "regex";
    }

    // Next optional token: @
    SourceLocation ExpectedLoc;
    if (!PH.Next("@")) {
      ExpectedLoc = Pos;
    } else {
      PH.Advance();
      unsigned Line = 0;
      bool FoundPlus = PH.Next("+");
      if (FoundPlus || PH.Next("-")) {
        // Relative to current line.
        PH.Advance();
        unsigned ExpectedLine = SM.getLineAndColumn(Pos).first;
        if (PH.Next(Line) && (FoundPlus || Line < ExpectedLine)) {
          if (FoundPlus) ExpectedLine += Line;
          else ExpectedLine -= Line;
          ExpectedLoc = locForLine(SM, Pos, ExpectedLine);
        }
      } else {
        // Absolute line number.
        if (PH.Next(Line) && Line > 0)
          ExpectedLoc = locForLine(SM, Pos, Line);
      }

      if (!ExpectedLoc.isValid()) {
        Diags.Report(Pos, diag::verify_missing_line) << KindStr;
        continue;
      }
      PH.Advance();
    }

    // Skip optional whitespace.
    PH.SkipWhitespace();

    // Next optional token: positive integer or a '+'.
    unsigned Min = 1;
    unsigned Max = 1;
    if (PH.Next(Min)) {
      PH.Advance();
      // A positive integer can be followed by a '+' meaning min
      // or more, or by a '-' meaning a range from min to max.
      if (PH.Next("+")) {
        Max = Directive::MaxCount;
        PH.Advance();
      } else if (PH.Next("-")) {
        PH.Advance();
        if (!PH.Next(Max) || Max < Min) {
          Diags.Report(Pos, diag::verify_invalid_range) << KindStr;
          continue;
        }
        PH.Advance();
      } else {
        Max = Min;
      }
    } else if (PH.Next("+")) {
      // '+' on its own means "1 or more".
      Max = Directive::MaxCount;
      PH.Advance();
    }

    // Skip optional whitespace.
    PH.SkipWhitespace();

    // Next token: {{
    if (!PH.Next("{{")) {
      Diags.Report(Pos, diag::verify_missing_start) << KindStr;
      continue;
    }
    PH.Advance();
    const char* const ContentBegin = PH.C; // mark content begin

    // Search for token: }}
    if (!PH.Search("}}")) {
      Diags.Report(Pos, diag::verify_missing_end) << KindStr;
      continue;
    }
    const char* const ContentEnd = PH.P; // mark content end
    PH.Advance();

    // Build directive text; convert \n to newlines.
    std::string Text;
    StringRef NewlineStr = "\\n";
    StringRef Content(ContentBegin, ContentEnd-ContentBegin);
    size_t CPos = 0;
    size_t FPos;
    while ((FPos = Content.find(NewlineStr, CPos)) != StringRef::npos) {
      Text += Content.substr(CPos, FPos-CPos);
      Text += '\n';
      CPos = FPos + NewlineStr.size();
    }
    if (Text.empty())
      Text.assign(ContentBegin, ContentEnd);

    // Construct new directive.
    Directive *D = Directive::create(RegexKind, Pos, ExpectedLoc, Text,
                                     Min, Max);
    std::string Error;
    if (D->isValid(Error)) {
      DL->push_back(D);
      FoundDirective = true;
    } else {
      Diags.Report(Pos, diag::verify_invalid_content) << KindStr << Error;
    }
  }

  return FoundDirective;
}

/// Hook into the preprocessor and extract comments containing expected
/// errors and warnings.
void VerifyDiagnosticConsumer::handleComment(Lexer &L, SourceRange Comment) {
  const llvm::SourceMgr &SM = L.getSourceManager();
  SourceLocation CommentBegin = Comment.Start;

  const char *CommentRaw = CommentBegin.getPointer();
  StringRef C(CommentRaw, Comment.End.getPointer() - CommentRaw);

  if (C.empty())
    return;

  ParseDirective(C, &ED, SM, CommentBegin, L.getDiagnostics(), Status);
}

void VerifyDiagnosticConsumer::finish() {
  CheckDiagnostics();  
}

/// \brief Takes a list of diagnostics that have been generated but not matched
/// by an expected-* directive and produces a diagnostic to the user from this.
static unsigned PrintUnexpected(DiagnosticsEngine &Diags,
                                llvm::SourceMgr *SourceMgr,
                                const_diag_iterator diag_begin,
                                const_diag_iterator diag_end,
                                const char *Kind) {
  if (diag_begin == diag_end) return 0;

  SmallString<256> Fmt;
  llvm::raw_svector_ostream OS(Fmt);
  for (const_diag_iterator I = diag_begin, E = diag_end; I != E; ++I) {
    if (!I->first.isValid() || !SourceMgr)
      OS << "\n  (frontend)";
    else
      OS << "\n  Line " << SourceMgr->getLineAndColumn(I->first).first;
    OS << ": " << I->second;
  }

  Diags.Report(SourceLocation(), diag::verify_inconsistent_diags)
    << Kind << /*Unexpected=*/true << OS.str();
  return std::distance(diag_begin, diag_end);
}

/// \brief Takes a list of diagnostics that were expected to have been generated
/// but were not and produces a diagnostic to the user from this.
static unsigned PrintExpected(DiagnosticsEngine &Diags, llvm::SourceMgr &SourceMgr,
                              DirectiveList &DL, const char *Kind) {
  if (DL.empty())
    return 0;

  SmallString<256> Fmt;
  llvm::raw_svector_ostream OS(Fmt);
  for (DirectiveList::iterator I = DL.begin(), E = DL.end(); I != E; ++I) {
    Directive &D = **I;
    OS << "\n  Line " << SourceMgr.getLineAndColumn(D.DiagnosticLoc).first;
    if (D.DirectiveLoc != D.DiagnosticLoc) {
      int BufID = SourceMgr.FindBufferContainingLoc(D.DirectiveLoc);
      OS << " (directive at "
         << SourceMgr.getMemoryBuffer(BufID)->getBufferIdentifier()
         << SourceMgr.getLineAndColumn(D.DirectiveLoc).first << ")";
    }
    OS << ": " << D.Text;
  }

  Diags.Report(SourceLocation(), diag::verify_inconsistent_diags)
    << Kind << /*Unexpected=*/false << OS.str();
  return DL.size();
}

/// Compare expected to seen diagnostic lists and return the the difference
/// between them.
static unsigned CheckLists(DiagnosticsEngine &Diags, llvm::SourceMgr &SourceMgr,
                           const char *Label,
                           DirectiveList &Left,
                           const_diag_iterator d2_begin,
                           const_diag_iterator d2_end) {
  DirectiveList LeftOnly;
  DiagList Right(d2_begin, d2_end);

  for (DirectiveList::iterator I = Left.begin(), E = Left.end(); I != E; ++I) {
    Directive& D = **I;
    unsigned LineNo1 = SourceMgr.getLineAndColumn(D.DiagnosticLoc).first;

    for (unsigned i = 0; i < D.Max; ++i) {
      DiagList::iterator II, IE;
      for (II = Right.begin(), IE = Right.end(); II != IE; ++II) {
        unsigned LineNo2 = SourceMgr.getLineAndColumn(II->first).first;
        if (LineNo1 != LineNo2)
          continue;

        const std::string &RightText = II->second;
        if (D.match(RightText))
          break;
      }
      if (II == IE) {
        // Not found.
        if (i >= D.Min) break;
        LeftOnly.push_back(*I);
      } else {
        // Found. The same cannot be found twice.
        Right.erase(II);
      }
    }
  }
  // Now all that's left in Right are those that were not matched.
  unsigned num = PrintExpected(Diags, SourceMgr, LeftOnly, Label);
  num += PrintUnexpected(Diags, &SourceMgr, Right.begin(), Right.end(), Label);
  return num;
}

/// This compares the expected results to those that were actually reported.
/// It emits any discrepencies. Return "true" if there were problems. Return
/// "false" otherwise.
static unsigned CheckResults(DiagnosticsEngine &Diags,
                             llvm::SourceMgr &SourceMgr,
                             const TextDiagnosticBuffer &Buffer,
                             ExpectedData &ED) {
  // We want to capture the delta between what was expected and what was
  // seen.
  //
  //   Expected \ Seen - set expected but not seen
  //   Seen \ Expected - set seen but not expected
  unsigned NumProblems = 0;

  // See if there are error mismatches.
  NumProblems += CheckLists(Diags, SourceMgr, "error", ED.Errors,
                            Buffer.err_begin(), Buffer.err_end());

  // See if there are note mismatches.
  NumProblems += CheckLists(Diags, SourceMgr, "note", ED.Notes,
                            Buffer.note_begin(), Buffer.note_end());

  return NumProblems;
}

void VerifyDiagnosticConsumer::CheckDiagnostics() {
  // Ensure any diagnostics go to the primary client.
  bool OwnsCurClient = Diags.ownsClient();
  DiagnosticConsumer *CurClient = Diags.takeClient();
  Diags.setClient(PrimaryClient, false);

  assert(SrcManager && "SrcManager not set?");
  // Produce an error if no expected-* directives could be found in the
  // source file(s) processed.
  if (Status == HasNoDirectives) {
    Diags.Report(SourceLocation(), diag::verify_no_directives);
    ++NumDiags;
    Status = HasNoDirectivesReported;
  }

  // Check that the expected diagnostics occurred.
  NumDiags += CheckResults(Diags, *SrcManager, *Buffer, ED);

  Diags.takeClient();
  Diags.setClient(CurClient, OwnsCurClient);

  // Reset the buffer, we have processed all the diagnostics in it.
  Buffer.reset(new TextDiagnosticBuffer());
  ED.Errors.clear();
  ED.Notes.clear();
}

Directive *Directive::create(bool RegexKind, SourceLocation DirectiveLoc,
                             SourceLocation DiagnosticLoc, StringRef Text,
                             unsigned Min, unsigned Max) {
  if (RegexKind)
    return new RegexDirective(DirectiveLoc, DiagnosticLoc, Text, Min, Max);
  return new StandardDirective(DirectiveLoc, DiagnosticLoc, Text, Min, Max);
}

