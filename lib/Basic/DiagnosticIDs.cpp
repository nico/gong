//===--- DiagnosticIDs.cpp - Diagnostic IDs Handling ----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements the Diagnostic IDs-related interfaces.
//
//===----------------------------------------------------------------------===//

#include "gong/Basic/DiagnosticIDs.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/ErrorHandling.h"

#include <stdint.h>
using namespace gong;

//===----------------------------------------------------------------------===//
// Builtin Diagnostic information
//===----------------------------------------------------------------------===//

namespace {

struct StaticDiagInfoRec {
  unsigned short DiagID;

  uint16_t DescriptionLen;
  const char *DescriptionStr;

  StringRef getDescription() const {
    return StringRef(DescriptionStr, DescriptionLen);
  }

  bool operator<(const StaticDiagInfoRec &RHS) const {
    return DiagID < RHS.DiagID;
  }
};


template <size_t SizeOfStr, typename FieldType>
class StringSizerHelper {
  char FIELD_TOO_SMALL[SizeOfStr <= FieldType(~0U) ? 1 : -1];
public:
  enum { Size = SizeOfStr };
};

#define STR_SIZE(str, fieldTy) StringSizerHelper<sizeof(str)-1, fieldTy>::Size 
} // namespace anonymous

static const StaticDiagInfoRec StaticDiagInfo[] = {
#define DIAG(ENUM,DESC) \
  { diag::ENUM, STR_SIZE(DESC, uint16_t), DESC },
#include "gong/Basic/DiagnosticLexKinds.def"
#undef DIAG
  { 0, 0, 0 }
};

static const unsigned StaticDiagInfoSize =
  sizeof(StaticDiagInfo)/sizeof(StaticDiagInfo[0])-1;

/// GetDiagInfo - Return the StaticDiagInfoRec entry for the specified DiagID,
/// or null if the ID is invalid.
static const StaticDiagInfoRec *GetDiagInfo(unsigned DiagID) {
  // If assertions are enabled, verify that the StaticDiagInfo array is sorted.
#ifndef NDEBUG
  static bool IsFirst = true;
  if (IsFirst) {
    for (unsigned i = 1; i != StaticDiagInfoSize; ++i) {
      assert(StaticDiagInfo[i-1].DiagID != StaticDiagInfo[i].DiagID &&
             "Diag ID conflict, the enums at the start of gong::diag (in "
             "DiagnosticIDs.h) probably need to be increased");

      assert(StaticDiagInfo[i-1] < StaticDiagInfo[i] &&
             "Improperly sorted diag info");
    }
    IsFirst = false;
  }
#endif

  // Search the diagnostic table with a binary search.
  StaticDiagInfoRec Find = { static_cast<unsigned short>(DiagID), 0, 0 };

  const StaticDiagInfoRec *Found =
    std::lower_bound(StaticDiagInfo, StaticDiagInfo + StaticDiagInfoSize, Find);
  if (Found == StaticDiagInfo + StaticDiagInfoSize ||
      Found->DiagID != DiagID)
    return 0;

  return Found;
}

//===----------------------------------------------------------------------===//
// Common Diagnostic implementation
//===----------------------------------------------------------------------===//

DiagnosticIDs::DiagnosticIDs() {
}

DiagnosticIDs::~DiagnosticIDs() {
}

/// Given a diagnostic ID, return a description of the
/// issue.
StringRef DiagnosticIDs::getDescription(unsigned DiagID) {
  if (const StaticDiagInfoRec *Info = GetDiagInfo(DiagID))
    return Info->getDescription();
  return StringRef();
}

//void DiagnosticIDs::getAllDiagnostics(
//                               llvm::SmallVectorImpl<diag::kind> &Diags) const {
//  for (unsigned i = 0; i != StaticDiagInfoSize; ++i)
//    Diags.push_back(StaticDiagInfo[i].DiagID);
//}
