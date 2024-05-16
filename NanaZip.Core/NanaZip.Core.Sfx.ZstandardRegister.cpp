/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Core.Sfx.ZstandardRegister.cpp
 * PURPOSE:   Implementation for Zstandard decoder register
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "SevenZip/CPP/7zip/Common/RegisterCodec.h"
#include "SevenZip/CPP/7zip/Compress/ZstdDecoder.h"

REGISTER_CODEC_E(
    Zstd,
    NCompress::NZstd::CDecoder(),
    nullptr,
    0x4F71101,
    "Zstd")
