// ZipRegister.cpp

#include "StdAfx.h"

#include "../../Common/RegisterArc.h"

#include "ZipHandler.h"

namespace NArchive {
namespace NZip {

static const Byte k_Signature[] = {
    4, 0x50, 0x4B, 0x03, 0x04,               // Local
    4, 0x50, 0x4B, 0x05, 0x06,               // Ecd
    4, 0x50, 0x4B, 0x06, 0x06,               // Ecd64
    6, 0x50, 0x4B, 0x07, 0x08, 0x50, 0x4B,   // Span / Descriptor
    6, 0x50, 0x4B, 0x30, 0x30, 0x50, 0x4B }; // NoSpan

REGISTER_ARC_IO(
  "zip", "zip z01 zipx jar xpi odt ods docx xlsx epub ipa apk appx", 0, 1,
  k_Signature,
  0,
  NArcInfoFlags::kFindSignature |
  NArcInfoFlags::kMultiSignature |
  NArcInfoFlags::kUseGlobalOffset,
  IsArc_Zip)
 
}}
