include ../../LzmaDec_gcc.mak

LOCAL_FLAGS_ST =
MT_OBJS =


ifdef ST_MODE

LOCAL_FLAGS_ST = -D_7ZIP_ST

ifdef SystemDrive
MT_OBJS = \
  $O/Threads.o \

endif

else

MT_OBJS = \
  $O/LzFindMt.o \
  $O/LzFindOpt.o \
  $O/StreamBinder.o \
  $O/Synchronization.o \
  $O/VirtThread.o \
  $O/MemBlocks.o \
  $O/OutMemStream.o \
  $O/ProgressMt.o \
  $O/Threads.o \

endif



COMMON_OBJS = \
  $O/CRC.o \
  $O/CrcReg.o \
  $O/DynLimBuf.o \
  $O/IntToString.o \
  $O/LzFindPrepare.o \
  $O/MyMap.o \
  $O/MyString.o \
  $O/MyVector.o \
  $O/MyXml.o \
  $O/NewHandler.o \
  $O/Sha1Prepare.o \
  $O/Sha1Reg.o \
  $O/Sha256Prepare.o \
  $O/Sha256Reg.o \
  $O/StringConvert.o \
  $O/StringToInt.o \
  $O/UTFConvert.o \
  $O/Wildcard.o \
  $O/XzCrc64Init.o \
  $O/XzCrc64Reg.o \

WIN_OBJS = \
  $O/FileDir.o \
  $O/FileFind.o \
  $O/FileIO.o \
  $O/FileName.o \
  $O/PropVariant.o \
  $O/PropVariantUtils.o \
  $O/System.o \
  $O/TimeUtils.o \

7ZIP_COMMON_OBJS = \
  $O/CreateCoder.o \
  $O/CWrappers.o \
  $O/InBuffer.o \
  $O/InOutTempBuffer.o \
  $O/FilterCoder.o \
  $O/LimitedStreams.o \
  $O/LockedStream.o \
  $O/MethodId.o \
  $O/MethodProps.o \
  $O/OffsetStream.o \
  $O/OutBuffer.o \
  $O/ProgressUtils.o \
  $O/PropId.o \
  $O/StreamObjects.o \
  $O/StreamUtils.o \
  $O/UniqBlocks.o \

AR_OBJS = \
  $O/ApmHandler.o \
  $O/ArHandler.o \
  $O/ArjHandler.o \
  $O/Base64Handler.o \
  $O/Bz2Handler.o \
  $O/ComHandler.o \
  $O/CpioHandler.o \
  $O/CramfsHandler.o \
  $O/DeflateProps.o \
  $O/DmgHandler.o \
  $O/ElfHandler.o \
  $O/ExtHandler.o \
  $O/FatHandler.o \
  $O/FlvHandler.o \
  $O/GzHandler.o \
  $O/GptHandler.o \
  $O/HandlerCont.o \
  $O/HfsHandler.o \
  $O/IhexHandler.o \
  $O/LzhHandler.o \
  $O/LzmaHandler.o \
  $O/MachoHandler.o \
  $O/MbrHandler.o \
  $O/MslzHandler.o \
  $O/MubHandler.o \
  $O/NtfsHandler.o \
  $O/PeHandler.o \
  $O/PpmdHandler.o \
  $O/QcowHandler.o \
  $O/RpmHandler.o \
  $O/SplitHandler.o \
  $O/SquashfsHandler.o \
  $O/SwfHandler.o \
  $O/UefiHandler.o \
  $O/VdiHandler.o \
  $O/VhdHandler.o \
  $O/VmdkHandler.o \
  $O/XarHandler.o \
  $O/XzHandler.o \
  $O/ZHandler.o \

AR_COMMON_OBJS = \
  $O/CoderMixer2.o \
  $O/DummyOutStream.o \
  $O/FindSignature.o \
  $O/InStreamWithCRC.o \
  $O/ItemNameUtils.o \
  $O/MultiStream.o \
  $O/OutStreamWithCRC.o \
  $O/OutStreamWithSha1.o \
  $O/HandlerOut.o \
  $O/ParseProperties.o \


7Z_OBJS = \
  $O/7zCompressionMode.o \
  $O/7zDecode.o \
  $O/7zEncode.o \
  $O/7zExtract.o \
  $O/7zFolderInStream.o \
  $O/7zHandler.o \
  $O/7zHandlerOut.o \
  $O/7zHeader.o \
  $O/7zIn.o \
  $O/7zOut.o \
  $O/7zProperties.o \
  $O/7zSpecStream.o \
  $O/7zUpdate.o \
  $O/7zRegister.o \

CAB_OBJS = \
  $O/CabBlockInStream.o \
  $O/CabHandler.o \
  $O/CabHeader.o \
  $O/CabIn.o \
  $O/CabRegister.o \

CHM_OBJS = \
  $O/ChmHandler.o \
  $O/ChmIn.o \

ISO_OBJS = \
  $O/IsoHandler.o \
  $O/IsoHeader.o \
  $O/IsoIn.o \
  $O/IsoRegister.o \

NSIS_OBJS = \
  $O/NsisDecode.o \
  $O/NsisHandler.o \
  $O/NsisIn.o \
  $O/NsisRegister.o \

RAR_OBJS = \
  $O/RarHandler.o \
  $O/Rar5Handler.o \

TAR_OBJS = \
  $O/TarHandler.o \
  $O/TarHandlerOut.o \
  $O/TarHeader.o \
  $O/TarIn.o \
  $O/TarOut.o \
  $O/TarUpdate.o \
  $O/TarRegister.o \

UDF_OBJS = \
  $O/UdfHandler.o \
  $O/UdfIn.o \

WIM_OBJS = \
  $O/WimHandler.o \
  $O/WimHandlerOut.o \
  $O/WimIn.o \
  $O/WimRegister.o \

ZIP_OBJS = \
  $O/ZipAddCommon.o \
  $O/ZipHandler.o \
  $O/ZipHandlerOut.o \
  $O/ZipIn.o \
  $O/ZipItem.o \
  $O/ZipOut.o \
  $O/ZipUpdate.o \
  $O/ZipRegister.o \

COMPRESS_OBJS = \
  $O/Bcj2Coder.o \
  $O/Bcj2Register.o \
  $O/BcjCoder.o \
  $O/BcjRegister.o \
  $O/BitlDecoder.o \
  $O/BranchMisc.o \
  $O/BranchRegister.o \
  $O/ByteSwap.o \
  $O/BZip2Crc.o \
  $O/BZip2Decoder.o \
  $O/BZip2Encoder.o \
  $O/BZip2Register.o \
  $O/CopyCoder.o \
  $O/CopyRegister.o \
  $O/Deflate64Register.o \
  $O/DeflateDecoder.o \
  $O/DeflateEncoder.o \
  $O/DeflateRegister.o \
  $O/DeltaFilter.o \
  $O/ImplodeDecoder.o \
  $O/LzfseDecoder.o \
  $O/LzhDecoder.o \
  $O/Lzma2Decoder.o \
  $O/Lzma2Encoder.o \
  $O/Lzma2Register.o \
  $O/LzmaDecoder.o \
  $O/LzmaEncoder.o \
  $O/LzmaRegister.o \
  $O/LzmsDecoder.o \
  $O/LzOutWindow.o \
  $O/LzxDecoder.o \
  $O/PpmdDecoder.o \
  $O/PpmdEncoder.o \
  $O/PpmdRegister.o \
  $O/PpmdZip.o \
  $O/QuantumDecoder.o \
  $O/Rar1Decoder.o \
  $O/Rar2Decoder.o \
  $O/Rar3Decoder.o \
  $O/Rar3Vm.o \
  $O/Rar5Decoder.o \
  $O/RarCodecsRegister.o \
  $O/ShrinkDecoder.o \
  $O/XpressDecoder.o \
  $O/XzDecoder.o \
  $O/XzEncoder.o \
  $O/ZlibDecoder.o \
  $O/ZlibEncoder.o \
  $O/ZDecoder.o \


CRYPTO_OBJS = \
  $O/7zAes.o \
  $O/7zAesRegister.o \
  $O/HmacSha1.o \
  $O/HmacSha256.o \
  $O/MyAes.o \
  $O/MyAesReg.o \
  $O/Pbkdf2HmacSha1.o \
  $O/RandGen.o \
  $O/Rar20Crypto.o \
  $O/Rar5Aes.o \
  $O/RarAes.o \
  $O/WzAes.o \
  $O/ZipCrypto.o \
  $O/ZipStrong.o \


C_OBJS = \
  $O/7zBuf2.o \
  $O/7zStream.o \
  $O/Alloc.o \
  $O/Bcj2.o \
  $O/Bcj2Enc.o \
  $O/Blake2s.o \
  $O/Bra.o \
  $O/Bra86.o \
  $O/BraIA64.o \
  $O/BwtSort.o \
  $O/CpuArch.o \
  $O/Delta.o \
  $O/HuffEnc.o \
  $O/LzFind.o \
  $O/Lzma2Dec.o \
  $O/Lzma2DecMt.o \
  $O/Lzma2Enc.o \
  $O/LzmaDec.o \
  $O/LzmaEnc.o \
  $O/MtCoder.o \
  $O/MtDec.o \
  $O/Ppmd7.o \
  $O/Ppmd7Dec.o \
  $O/Ppmd7aDec.o \
  $O/Ppmd7Enc.o \
  $O/Ppmd8.o \
  $O/Ppmd8Dec.o \
  $O/Ppmd8Enc.o \
  $O/Sort.o \
  $O/Xz.o \
  $O/XzDec.o \
  $O/XzEnc.o \
  $O/XzIn.o \
  $O/XzCrc64.o \
  $O/XzCrc64Opt.o \
  $O/7zCrc.o \
  $O/7zCrcOpt.o \
  $O/Aes.o \
  $O/AesOpt.o \
  $O/Sha256.o \
  $O/Sha256Opt.o \
  $O/Sha1.o \
  $O/Sha1Opt.o \

ARC_OBJS = \
	$(LZMA_DEC_OPT_OBJS) \
  $(C_OBJS) \
  $(MT_OBJS) \
  $(COMMON_OBJS) \
  $(WIN_OBJS) \
  $(AR_OBJS) \
  $(AR_COMMON_OBJS) \
  $(7Z_OBJS) \
  $(CAB_OBJS) \
  $(CHM_OBJS) \
  $(COM_OBJS) \
  $(ISO_OBJS) \
  $(NSIS_OBJS) \
  $(RAR_OBJS) \
  $(TAR_OBJS) \
  $(UDF_OBJS) \
  $(WIM_OBJS) \
  $(ZIP_OBJS) \
  $(COMPRESS_OBJS) \
  $(CRYPTO_OBJS) \
  $(7ZIP_COMMON_OBJS) \

