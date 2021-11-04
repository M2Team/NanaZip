// MyExplorerCommand.h

#ifndef __MY_EXPLORER_COMMAND_H
#define __MY_EXPLORER_COMMAND_H

#if _MSC_VER >= 1910
#define USE_SYS_shobjidl_core
#endif

#ifdef USE_SYS_shobjidl_core

// #include <shobjidl_core.h>

#else

#ifndef __IShellItemArray_INTERFACE_DEFINED__
#define __IShellItemArray_INTERFACE_DEFINED__

// propsys.h

typedef /* [v1_enum] */
enum GETPROPERTYSTOREFLAGS
{
  GPS_DEFAULT = 0,
  GPS_HANDLERPROPERTIESONLY = 0x1,
  GPS_READWRITE = 0x2,
  GPS_TEMPORARY = 0x4,
  GPS_FASTPROPERTIESONLY  = 0x8,
  GPS_OPENSLOWITEM  = 0x10,
  GPS_DELAYCREATION = 0x20,
  GPS_BESTEFFORT  = 0x40,
  GPS_NO_OPLOCK = 0x80,
  GPS_PREFERQUERYPROPERTIES = 0x100,
  GPS_EXTRINSICPROPERTIES = 0x200,
  GPS_EXTRINSICPROPERTIESONLY = 0x400,
  GPS_VOLATILEPROPERTIES  = 0x800,
  GPS_VOLATILEPROPERTIESONLY  = 0x1000,
  GPS_MASK_VALID  = 0x1fff
} GETPROPERTYSTOREFLAGS;

// DEFINE_ENUM_FLAG_OPERATORS(GETPROPERTYSTOREFLAGS)


#ifndef PROPERTYKEY_DEFINED
#define PROPERTYKEY_DEFINED

typedef
struct _tagpropertykey
{
  GUID fmtid;
  DWORD pid;
} PROPERTYKEY;

#endif // PROPERTYKEY_DEFINED

// propkeydef.h
#define REFPROPERTYKEY const PROPERTYKEY &

#ifdef INITGUID
#define DEFINE_PROPERTYKEY(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8, pid) EXTERN_C const PROPERTYKEY DECLSPEC_SELECTANY name = { { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }, pid }
#else
#define DEFINE_PROPERTYKEY(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8, pid) EXTERN_C const PROPERTYKEY name
#endif // INITGUID


// <shobjidl_core.h>
typedef /* [v1_enum] */
enum SIATTRIBFLAGS
{
  SIATTRIBFLAGS_AND = 0x1,
  SIATTRIBFLAGS_OR  = 0x2,
  SIATTRIBFLAGS_APPCOMPAT = 0x3,
  SIATTRIBFLAGS_MASK  = 0x3,
  SIATTRIBFLAGS_ALLITEMS  = 0x4000
} SIATTRIBFLAGS;

// DEFINE_ENUM_FLAG_OPERATORS(SIATTRIBFLAGS)


// MIDL_INTERFACE("70629033-e363-4a28-a567-0db78006e6d7")
DEFINE_GUID(IID_IEnumShellItems, 0x70629033, 0xe363, 0xe363, 0xa5, 0x67, 0x0d, 0xb7, 0x80, 0x06, 0xe6, 0xd7);

struct IEnumShellItems : public IUnknown
{
  STDMETHOD (Next) (ULONG celt, IShellItem **rgelt, ULONG *pceltFetched) = 0;
  STDMETHOD (Skip) (ULONG celt) = 0;
  STDMETHOD (Reset) (void) = 0;
  STDMETHOD (Clone) (IEnumShellItems **ppenum) = 0;
};


//  MIDL_INTERFACE("b63ea76d-1f85-456f-a19c-48159efa858b")
DEFINE_GUID(IID_IShellItemArray, 0xb63ea76d, 0x1f85, 0x456f, 0xa1, 0x9c, 0x48, 0x15, 0x9e, 0xfa, 0x85, 0x8b);

struct IShellItemArray : public IUnknown
{
  STDMETHOD (BindToHandler) (IBindCtx *pbc, REFGUID bhid, REFIID riid, void **ppvOut) = 0;
  STDMETHOD (GetPropertyStore) (GETPROPERTYSTOREFLAGS flags, REFIID riid, void **ppv) = 0;
  STDMETHOD (GetPropertyDescriptionList) (REFPROPERTYKEY keyType, REFIID riid, void **ppv) = 0;
  STDMETHOD (GetAttributes) ( SIATTRIBFLAGS AttribFlags, SFGAOF sfgaoMask, SFGAOF *psfgaoAttribs) = 0;
  STDMETHOD (GetCount) (DWORD *pdwNumItems) = 0;
  STDMETHOD (GetItemAt) (DWORD dwIndex, IShellItem **ppsi) = 0;
  STDMETHOD (EnumItems) (IEnumShellItems **ppenumShellItems) = 0;
};


#ifndef __IEnumExplorerCommand_INTERFACE_DEFINED__
#define __IEnumExplorerCommand_INTERFACE_DEFINED__

struct IExplorerCommand;

// MIDL_INTERFACE("a88826f8-186f-4987-aade-ea0cef8fbfe8")
DEFINE_GUID(IID_IEnumExplorerCommand , 0xa88826f8, 0x186f, 0x4987, 0xaa, 0xde, 0xea, 0x0c, 0xef, 0x8f, 0xbf, 0xe8);

struct IEnumExplorerCommand : public IUnknown
{
  STDMETHOD (Next) (ULONG celt, IExplorerCommand **pUICommand, ULONG *pceltFetched) = 0;
  STDMETHOD (Skip) (ULONG celt) = 0;
  STDMETHOD (Reset) (void) = 0;
  STDMETHOD (Clone) (IEnumExplorerCommand **ppenum) = 0;
};


enum _EXPCMDSTATE
{
  ECS_ENABLED = 0,
  ECS_DISABLED  = 0x1,
  ECS_HIDDEN  = 0x2,
  ECS_CHECKBOX  = 0x4,
  ECS_CHECKED = 0x8,
  ECS_RADIOCHECK  = 0x10
};

typedef DWORD EXPCMDSTATE;

/* [v1_enum] */
enum _EXPCMDFLAGS
{
  ECF_DEFAULT = 0,
  ECF_HASSUBCOMMANDS  = 0x1,
  ECF_HASSPLITBUTTON  = 0x2,
  ECF_HIDELABEL = 0x4,
  ECF_ISSEPARATOR = 0x8,
  ECF_HASLUASHIELD  = 0x10,
  ECF_SEPARATORBEFORE = 0x20,
  ECF_SEPARATORAFTER  = 0x40,
  ECF_ISDROPDOWN  = 0x80,
  ECF_TOGGLEABLE  = 0x100,
  ECF_AUTOMENUICONS = 0x200
};
typedef DWORD EXPCMDFLAGS;


// MIDL_INTERFACE("a08ce4d0-fa25-44ab-b57c-c7b1c323e0b9")
DEFINE_GUID(IID_IExplorerCommand, 0xa08ce4d0, 0xfa25, 0x44ab, 0xb5, 0x7c, 0xc7, 0xb1, 0xc3, 0x23, 0xe0, 0xb9);

struct IExplorerCommand : public IUnknown
{
  STDMETHOD (GetTitle) (IShellItemArray *psiItemArray, LPWSTR *ppszName) = 0;
  STDMETHOD (GetIcon) (IShellItemArray *psiItemArray, LPWSTR *ppszIcon) = 0;
  STDMETHOD (GetToolTip) (IShellItemArray *psiItemArray, LPWSTR *ppszInfotip) = 0;
  STDMETHOD (GetCanonicalName) (GUID *pguidCommandName) = 0;
  STDMETHOD (GetState) (IShellItemArray *psiItemArray, BOOL fOkToBeSlow, EXPCMDSTATE *pCmdState) = 0;
  STDMETHOD (Invoke) (IShellItemArray *psiItemArray, IBindCtx *pbc) = 0;
  STDMETHOD (GetFlags) (EXPCMDFLAGS *pFlags) = 0;
  STDMETHOD (EnumSubCommands) (IEnumExplorerCommand **ppEnum) = 0;
};

#endif // IShellItemArray
#endif // __IEnumExplorerCommand_INTERFACE_DEFINED__
#endif // USE_SYS_shobjidl_core

#endif // __MY_EXPLORER_COMMAND_H
