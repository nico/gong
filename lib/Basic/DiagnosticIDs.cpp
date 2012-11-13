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

#include <map>
#include <stdint.h>
#include <vector>
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
// Custom Diagnostic information
//===----------------------------------------------------------------------===//

namespace gong {
  namespace diag {
    class CustomDiagInfo {
      typedef std::string DiagDesc;
      std::vector<DiagDesc> DiagInfo;
      std::map<DiagDesc, unsigned> DiagIDs;
    public:

      /// getDescription - Return the description of the specified custom
      /// diagnostic.
      StringRef getDescription(unsigned DiagID) const {
        assert(this && DiagID-DIAG_UPPER_LIMIT < DiagInfo.size() &&
               "Invalid diagnosic ID");
        return DiagInfo[DiagID-DIAG_UPPER_LIMIT];
      }

      unsigned getOrCreateDiagID(StringRef Message, DiagnosticIDs &Diags) {
        DiagDesc D(Message);
        // Check to see if it already exists.
        std::map<DiagDesc, unsigned>::iterator I = DiagIDs.lower_bound(D);
        if (I != DiagIDs.end() && I->first == D)
          return I->second;

        // If not, assign a new ID.
        unsigned ID = DiagInfo.size()+DIAG_UPPER_LIMIT;
        DiagIDs.insert(std::make_pair(D, ID));
        DiagInfo.push_back(D);
        return ID;
      }
    };

  } // end diag namespace
} // end gong namespace

//===----------------------------------------------------------------------===//
// Common Diagnostic implementation
//===----------------------------------------------------------------------===//

DiagnosticIDs::DiagnosticIDs() {
  CustomDiagInfo = 0;
}

DiagnosticIDs::~DiagnosticIDs() {
  delete CustomDiagInfo;
}

/// getCustomDiagID - Return an ID for a diagnostic with the specified message
/// and level.  If this is the first request for this diagnosic, it is
/// registered and created, otherwise the existing ID is returned.
unsigned DiagnosticIDs::getCustomDiagID(StringRef Message) {
  if (CustomDiagInfo == 0)
    CustomDiagInfo = new diag::CustomDiagInfo();
  return CustomDiagInfo->getOrCreateDiagID(Message, *this);
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
