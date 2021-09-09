// FarPlugin.h

// #include "plugin.hpp"

const int kInfoPanelLineSize = 80;

// #define __FAR_PLUGIN_H

#ifdef UNDER_CE
typedef struct _CHAR_INFO {
    union {
        WCHAR UnicodeChar;
        CHAR   AsciiChar;
    } Char;
    WORD Attributes;
} CHAR_INFO, *PCHAR_INFO;
#endif

#ifndef __FAR_PLUGIN_H
#define __FAR_PLUGIN_H

#ifndef _WIN64
#if defined(__BORLANDC__) && (__BORLANDC <= 0x520)
  #pragma option -a1
#elif defined(__GNUC__) || (defined(__WATCOMC__) && (__WATCOMC__ < 1100))
  #pragma pack(1)
#else
  #pragma pack(push,1)
#endif
#endif

  #if _MSC_VER
    #define _export
  #endif

#define NM 260

struct FarFindData
{
  DWORD dwFileAttributes;
  FILETIME ftCreationTime;
  FILETIME ftLastAccessTime;
  FILETIME ftLastWriteTime;
  DWORD nFileSizeHigh;
  DWORD nFileSizeLow;
  DWORD dwReserved0;
  DWORD dwReserved1;
  char cFileName[ MAX_PATH ];
  char cAlternateFileName[ 14 ];
};

struct PluginPanelItem
{
  FarFindData FindData;
  DWORD PackSizeHigh;
  DWORD PackSize;
  DWORD Flags;
  DWORD NumberOfLinks;
  char *Description;
  char *Owner;
  char **CustomColumnData;
  int CustomColumnNumber;
  DWORD_PTR UserData;
  DWORD CRC32;
  DWORD_PTR Reserved[2];
};

#define PPIF_PROCESSDESCR 0x80000000
#define PPIF_SELECTED     0x40000000
#define PPIF_USERDATA     0x20000000

enum {
  FMENU_SHOWAMPERSAND=1,
  FMENU_WRAPMODE=2,
  FMENU_AUTOHIGHLIGHT=4,
  FMENU_REVERSEAUTOHIGHLIGHT=8
};


typedef int (WINAPI *FARAPIMENU)(
  INT_PTR PluginNumber,
  int X,
  int Y,
  int MaxHeight,
  unsigned int Flags,
  char *Title,
  char *Bottom,
  char *HelpTopic,
  int *BreakKeys,
  int *BreakCode,
  struct FarMenuItem *Item,
  int ItemsNumber
);

typedef int (WINAPI *FARAPIDIALOG)(
  INT_PTR PluginNumber,
  int X1,
  int Y1,
  int X2,
  int Y2,
  char *HelpTopic,
  struct FarDialogItem *Item,
  int ItemsNumber
);

enum {
  FMSG_WARNING             = 0x00000001,
  FMSG_ERRORTYPE           = 0x00000002,
  FMSG_KEEPBACKGROUND      = 0x00000004,
  FMSG_DOWN                = 0x00000008,
  FMSG_LEFTALIGN           = 0x00000010,

  FMSG_ALLINONE            = 0x00000020,

  FMSG_MB_OK               = 0x00010000,
  FMSG_MB_OKCANCEL         = 0x00020000,
  FMSG_MB_ABORTRETRYIGNORE = 0x00030000,
  FMSG_MB_YESNO            = 0x00040000,
  FMSG_MB_YESNOCANCEL      = 0x00050000,
  FMSG_MB_RETRYCANCEL      = 0x00060000
};

typedef int (WINAPI *FARAPIMESSAGE)(
  INT_PTR PluginNumber,
  unsigned int Flags,
  const char *HelpTopic,
  const char * const *Items,
  int ItemsNumber,
  int ButtonsNumber
);

typedef char* (WINAPI *FARAPIGETMSG)(
  INT_PTR PluginNumber,
  int MsgId
);


enum DialogItemTypes {
  DI_TEXT,
  DI_VTEXT,
  DI_SINGLEBOX,
  DI_DOUBLEBOX,
  DI_EDIT,
  DI_PSWEDIT,
  DI_FIXEDIT,
  DI_BUTTON,
  DI_CHECKBOX,
  DI_RADIOBUTTON
};

enum FarDialogItemFlags {
  DIF_COLORMASK       =    0xff,
  DIF_SETCOLOR        =   0x100,
  DIF_BOXCOLOR        =   0x200,
  DIF_GROUP           =   0x400,
  DIF_LEFTTEXT        =   0x800,
  DIF_MOVESELECT      =  0x1000,
  DIF_SHOWAMPERSAND   =  0x2000,
  DIF_CENTERGROUP     =  0x4000,
  DIF_NOBRACKETS      =  0x8000,
  DIF_SEPARATOR       = 0x10000,
  DIF_EDITOR          = 0x20000,
  DIF_HISTORY         = 0x40000
};

struct FarDialogItem
{
  int Type;
  int X1,Y1,X2,Y2;
  int Focus;
  union
  {
    int Selected;
    const char *History;
    const char *Mask;
    struct FarList *ListItems;
    int ListPos;
    CHAR_INFO *VBuf;
  };
  unsigned int Flags;
  int DefaultButton;
  char Data[512];
};


struct FarMenuItem
{
  char Text[128];
  int Selected;
  int Checked;
  int Separator;
};


enum {FCTL_CLOSEPLUGIN,FCTL_GETPANELINFO,FCTL_GETANOTHERPANELINFO,
      FCTL_UPDATEPANEL,FCTL_UPDATEANOTHERPANEL,
      FCTL_REDRAWPANEL,FCTL_REDRAWANOTHERPANEL,
      FCTL_SETANOTHERPANELDIR,FCTL_GETCMDLINE,FCTL_SETCMDLINE,
      FCTL_SETSELECTION,FCTL_SETANOTHERSELECTION,
      FCTL_SETVIEWMODE,FCTL_SETANOTHERVIEWMODE,FCTL_INSERTCMDLINE,
      FCTL_SETUSERSCREEN,FCTL_SETPANELDIR,FCTL_SETCMDLINEPOS,
      FCTL_GETCMDLINEPOS
};

enum {PTYPE_FILEPANEL,PTYPE_TREEPANEL,PTYPE_QVIEWPANEL,PTYPE_INFOPANEL};

struct PanelInfo
{
  int PanelType;
  int Plugin;
  RECT PanelRect;
  struct PluginPanelItem *PanelItems;
  int ItemsNumber;
  struct PluginPanelItem *SelectedItems;
  int SelectedItemsNumber;
  int CurrentItem;
  int TopPanelItem;
  int Visible;
  int Focus;
  int ViewMode;
  char ColumnTypes[80];
  char ColumnWidths[80];
  char CurDir[NM];
  int ShortNames;
  int SortMode;
  DWORD Flags;
  DWORD Reserved;
};


struct PanelRedrawInfo
{
  int CurrentItem;
  int TopPanelItem;
};


typedef int (WINAPI *FARAPICONTROL)(
  HANDLE hPlugin,
  int Command,
  void *Param
);

typedef HANDLE (WINAPI *FARAPISAVESCREEN)(int X1,int Y1,int X2,int Y2);

typedef void (WINAPI *FARAPIRESTORESCREEN)(HANDLE hScreen);

typedef int (WINAPI *FARAPIGETDIRLIST)(
  char *Dir,
  struct PluginPanelItem **pPanelItem,
  int *pItemsNumber
);

typedef int (WINAPI *FARAPIGETPLUGINDIRLIST)(
  INT_PTR PluginNumber,
  HANDLE hPlugin,
  char *Dir,
  struct PluginPanelItem **pPanelItem,
  int *pItemsNumber
);

typedef void (WINAPI *FARAPIFREEDIRLIST)(struct PluginPanelItem *PanelItem);

enum VIEWER_FLAGS {
  VF_NONMODAL=1,VF_DELETEONCLOSE=2
};

typedef int (WINAPI *FARAPIVIEWER)(
  char *FileName,
  char *Title,
  int X1,
  int Y1,
  int X2,
  int Y2,
  DWORD Flags
);

typedef int (WINAPI *FARAPIEDITOR)(
  char *FileName,
  char *Title,
  int X1,
  int Y1,
  int X2,
  int Y2,
  DWORD Flags,
  int StartLine,
  int StartChar
);

typedef int (WINAPI *FARAPICMPNAME)(
  char *Pattern,
  char *String,
  int SkipPath
);


#define FCT_DETECT 0x40000000

struct CharTableSet
{
  char DecodeTable[256];
  char EncodeTable[256];
  char UpperTable[256];
  char LowerTable[256];
  char TableName[128];
};

typedef int (WINAPI *FARAPICHARTABLE)(
  int Command,
  char *Buffer,
  int BufferSize
);

typedef void (WINAPI *FARAPITEXT)(
  int X,
  int Y,
  int Color,
  char *Str
);


typedef int (WINAPI *FARAPIEDITORCONTROL)(
  int Command,
  void *Param
);

struct PluginStartupInfo
{
  int StructSize;
  char ModuleName[NM];
  INT_PTR ModuleNumber;
  char *RootKey;
  FARAPIMENU Menu;
  FARAPIDIALOG Dialog;
  FARAPIMESSAGE Message;
  FARAPIGETMSG GetMsg;
  FARAPICONTROL Control;
  FARAPISAVESCREEN SaveScreen;
  FARAPIRESTORESCREEN RestoreScreen;
  FARAPIGETDIRLIST GetDirList;
  FARAPIGETPLUGINDIRLIST GetPluginDirList;
  FARAPIFREEDIRLIST FreeDirList;
  FARAPIVIEWER Viewer;
  FARAPIEDITOR Editor;
  FARAPICMPNAME CmpName;
  FARAPICHARTABLE CharTable;
  FARAPITEXT Text;
  FARAPIEDITORCONTROL EditorControl;
};


enum PLUGIN_FLAGS {
  PF_PRELOAD        = 0x0001,
  PF_DISABLEPANELS  = 0x0002,
  PF_EDITOR         = 0x0004,
  PF_VIEWER         = 0x0008
};


struct PluginInfo
{
  int StructSize;
  DWORD Flags;
  char **DiskMenuStrings;
  int *DiskMenuNumbers;
  int DiskMenuStringsNumber;
  char **PluginMenuStrings;
  int PluginMenuStringsNumber;
  char **PluginConfigStrings;
  int PluginConfigStringsNumber;
  char *CommandPrefix;
};

struct InfoPanelLine
{
  char Text[kInfoPanelLineSize];
  char Data[kInfoPanelLineSize];
  int Separator;
};


struct PanelMode
{
  char *ColumnTypes;
  char *ColumnWidths;
  char **ColumnTitles;
  int FullScreen;
  int DetailedStatus;
  int AlignExtensions;
  int CaseConversion;
  char *StatusColumnTypes;
  char *StatusColumnWidths;
  DWORD Reserved[2];
};


enum OPENPLUGININFO_FLAGS {
  OPIF_USEFILTER               = 0x0001,
  OPIF_USESORTGROUPS           = 0x0002,
  OPIF_USEHIGHLIGHTING         = 0x0004,
  OPIF_ADDDOTS                 = 0x0008,
  OPIF_RAWSELECTION            = 0x0010,
  OPIF_REALNAMES               = 0x0020,
  OPIF_SHOWNAMESONLY           = 0x0040,
  OPIF_SHOWRIGHTALIGNNAMES     = 0x0080,
  OPIF_SHOWPRESERVECASE        = 0x0100,
  OPIF_FINDFOLDERS             = 0x0200,
  OPIF_COMPAREFATTIME          = 0x0400,
  OPIF_EXTERNALGET             = 0x0800,
  OPIF_EXTERNALPUT             = 0x1000,
  OPIF_EXTERNALDELETE          = 0x2000,
  OPIF_EXTERNALMKDIR           = 0x4000,
  OPIF_USEATTRHIGHLIGHTING     = 0x8000
};


enum OPENPLUGININFO_SORTMODES {
  SM_DEFAULT,SM_UNSORTED,SM_NAME,SM_EXT,SM_MTIME,SM_CTIME,
  SM_ATIME,SM_SIZE,SM_DESCR,SM_OWNER,SM_COMPRESSEDSIZE,SM_NUMLINKS
};


struct KeyBarTitles
{
  char *Titles[12];
  char *CtrlTitles[12];
  char *AltTitles[12];
  char *ShiftTitles[12];
};


struct OpenPluginInfo
{
  int StructSize;
  DWORD Flags;
  const char *HostFile;
  const char *CurDir;
  const char *Format;
  const char *PanelTitle;
  const struct InfoPanelLine *InfoLines;
  int InfoLinesNumber;
  const char * const *DescrFiles;
  int DescrFilesNumber;
  const struct PanelMode *PanelModesArray;
  int PanelModesNumber;
  int StartPanelMode;
  int StartSortMode;
  int StartSortOrder;
  const struct KeyBarTitles *KeyBar;
  const char *ShortcutData;
  // long Reserverd;
};

enum {
  OPEN_DISKMENU,
  OPEN_PLUGINSMENU,
  OPEN_FINDLIST,
  OPEN_SHORTCUT,
  OPEN_COMMANDLINE,
  OPEN_EDITOR,
  OPEN_VIEWER
};

enum {PKF_CONTROL=1,PKF_ALT=2,PKF_SHIFT=4};

enum FAR_EVENTS {
  FE_CHANGEVIEWMODE,
  FE_REDRAW,
  FE_IDLE,
  FE_CLOSE,
  FE_BREAK,
  FE_COMMAND
};

enum OPERATION_MODES {
  OPM_SILENT=1,
  OPM_FIND=2,
  OPM_VIEW=4,
  OPM_EDIT=8,
  OPM_TOPLEVEL=16,
  OPM_DESCR=32
};

#ifndef _WIN64
#if defined(__BORLANDC__) && (__BORLANDC <= 0x520)
  #pragma option -a.
#elif defined(__GNUC__) || (defined(__WATCOMC__) && (__WATCOMC__ < 1100))
  #pragma pack()
#else
  #pragma pack(pop)
#endif
#endif

/*
EXTERN_C_BEGIN

  void   WINAPI _export ClosePluginW(HANDLE hPlugin);
  int    WINAPI _export CompareW(HANDLE hPlugin,const struct PluginPanelItem *Item1,const struct PluginPanelItem *Item2,unsigned int Mode);
  int    WINAPI _export ConfigureW(int ItemNumber);
  int    WINAPI _export DeleteFilesW(HANDLE hPlugin,struct PluginPanelItem *PanelItem,int ItemsNumber,int OpMode);
  void   WINAPI _export ExitFARW(void);
  void   WINAPI _export FreeFindDataW(HANDLE hPlugin,struct PluginPanelItem *PanelItem,int ItemsNumber);
  void   WINAPI _export FreeVirtualFindDataW(HANDLE hPlugin,struct PluginPanelItem *PanelItem,int ItemsNumber);
  int    WINAPI _export GetFilesW(HANDLE hPlugin,struct PluginPanelItem *PanelItem,int ItemsNumber,int Move,const wchar_t **DestPath,int OpMode);
  int    WINAPI _export GetFindDataW(HANDLE hPlugin,struct PluginPanelItem **pPanelItem,int *pItemsNumber,int OpMode);
  int    WINAPI _export GetMinFarVersionW(void);
  void   WINAPI _export GetOpenPluginInfoW(HANDLE hPlugin,struct OpenPluginInfo *Info);
  void   WINAPI _export GetPluginInfoW(struct PluginInfo *Info);
  int    WINAPI _export GetVirtualFindDataW(HANDLE hPlugin,struct PluginPanelItem **pPanelItem,int *pItemsNumber,const wchar_t *Path);
  int    WINAPI _export MakeDirectoryW(HANDLE hPlugin,const wchar_t **Name,int OpMode);
  HANDLE WINAPI _export OpenFilePluginW(const wchar_t *Name,const unsigned char *Data,int DataSize,int OpMode);
  HANDLE WINAPI _export OpenPluginW(int OpenFrom,INT_PTR Item);
  int    WINAPI _export ProcessDialogEventW(int Event,void *Param);
  int    WINAPI _export ProcessEditorEventW(int Event,void *Param);
  int    WINAPI _export ProcessEditorInputW(const INPUT_RECORD *Rec);
  int    WINAPI _export ProcessEventW(HANDLE hPlugin,int Event,void *Param);
  int    WINAPI _export ProcessHostFileW(HANDLE hPlugin,struct PluginPanelItem *PanelItem,int ItemsNumber,int OpMode);
  int    WINAPI _export ProcessKeyW(HANDLE hPlugin,int Key,unsigned int ControlState);
  int    WINAPI _export ProcessSynchroEventW(int Event,void *Param);
  int    WINAPI _export ProcessViewerEventW(int Event,void *Param);
  int    WINAPI _export PutFilesW(HANDLE hPlugin,struct PluginPanelItem *PanelItem,int ItemsNumber,int Move,const wchar_t *SrcPath,int OpMode);
  int    WINAPI _export SetDirectoryW(HANDLE hPlugin,const wchar_t *Dir,int OpMode);
  int    WINAPI _export SetFindListW(HANDLE hPlugin,const struct PluginPanelItem *PanelItem,int ItemsNumber);
  void   WINAPI _export SetStartupInfoW(const struct PluginStartupInfo *Info);

EXTERN_C_END
*/

#endif
