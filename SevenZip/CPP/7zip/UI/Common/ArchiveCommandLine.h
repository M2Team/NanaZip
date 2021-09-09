// ArchiveCommandLine.h

#ifndef __ARCHIVE_COMMAND_LINE_H
#define __ARCHIVE_COMMAND_LINE_H

#include "../../../Common/CommandLineParser.h"
#include "../../../Common/Wildcard.h"

#include "EnumDirItems.h"

#include "Extract.h"
#include "HashCalc.h"
#include "Update.h"

typedef CMessagePathException CArcCmdLineException;

namespace NCommandType { enum EEnum
{
  kAdd = 0,
  kUpdate,
  kDelete,
  kTest,
  kExtract,
  kExtractFull,
  kList,
  kBenchmark,
  kInfo,
  kHash,
  kRename
};}

struct CArcCommand
{
  NCommandType::EEnum CommandType;

  bool IsFromExtractGroup() const;
  bool IsFromUpdateGroup() const;
  bool IsTestCommand() const { return CommandType == NCommandType::kTest; }
  NExtract::NPathMode::EEnum GetPathMode() const;
};

enum
{
  k_OutStream_disabled = 0,
  k_OutStream_stdout = 1,
  k_OutStream_stderr = 2
};

struct CArcCmdLineOptions
{
  bool HelpMode;

  // bool LargePages;
  bool CaseSensitiveChange;
  bool CaseSensitive;

  bool IsInTerminal;
  bool IsStdOutTerminal;
  bool IsStdErrTerminal;
  bool StdInMode;
  bool StdOutMode;
  bool EnableHeaders;

  bool YesToAll;
  bool ShowDialog;
  bool TechMode;
  bool ShowTime;

  int ConsoleCodePage;

  NWildcard::CCensor Censor;

  CArcCommand Command;
  UString ArchiveName;

  #ifndef _NO_CRYPTO
  bool PasswordEnabled;
  UString Password;
  #endif

  UStringVector HashMethods;

  bool AppendName;
  // UStringVector ArchivePathsSorted;
  // UStringVector ArchivePathsFullSorted;
  NWildcard::CCensor arcCensor;
  UString ArcName_for_StdInMode;

  CObjectVector<CProperty> Properties;

  CExtractOptionsBase ExtractOptions;

  CBoolPair NtSecurity;
  CBoolPair AltStreams;
  CBoolPair HardLinks;
  CBoolPair SymLinks;

  CUpdateOptions UpdateOptions;
  CHashOptions HashOptions;
  UString ArcType;
  UStringVector ExcludedArcTypes;
  
  unsigned Number_for_Out;
  unsigned Number_for_Errors;
  unsigned Number_for_Percents;
  unsigned LogLevel;

  // bool IsOutAllowed() const { return Number_for_Out != k_OutStream_disabled; }

  // Benchmark
  UInt32 NumIterations;
  bool NumIterations_Defined;

  CArcCmdLineOptions():
      HelpMode(false),
      // LargePages(false),
      CaseSensitiveChange(false),
      CaseSensitive(false),

      IsInTerminal(false),
      IsStdOutTerminal(false),
      IsStdErrTerminal(false),

      StdInMode(false),
      StdOutMode(false),

      EnableHeaders(false),
      
      YesToAll(false),
      ShowDialog(false),
      TechMode(false),
      ShowTime(false),

      ConsoleCodePage(-1),

      Number_for_Out(k_OutStream_stdout),
      Number_for_Errors(k_OutStream_stderr),
      Number_for_Percents(k_OutStream_stdout),

      LogLevel(0)
  {
  };
};

class CArcCmdLineParser
{
  NCommandLineParser::CParser parser;
public:
  UString Parse1Log;
  void Parse1(const UStringVector &commandStrings, CArcCmdLineOptions &options);
  void Parse2(CArcCmdLineOptions &options);
};

#endif
