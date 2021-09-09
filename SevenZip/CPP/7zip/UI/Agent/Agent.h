// Agent/Agent.h

#ifndef __AGENT_AGENT_H
#define __AGENT_AGENT_H

#include "../../../Common/MyCom.h"

#include "../../../Windows/PropVariant.h"

#include "../Common/OpenArchive.h"
#include "../Common/UpdateAction.h"

#ifdef NEW_FOLDER_INTERFACE
#include "../FileManager/IFolder.h"
#include "../Common/LoadCodecs.h"
#endif

#include "AgentProxy.h"
#include "IFolderArchive.h"

extern CCodecs *g_CodecsObj;
HRESULT LoadGlobalCodecs();
void FreeGlobalCodecs();

class CAgentFolder;

DECL_INTERFACE(IArchiveFolderInternal, 0x01, 0xC)
{
  STDMETHOD(GetAgentFolder)(CAgentFolder **agentFolder) PURE;
};

struct CProxyItem
{
  unsigned DirIndex;
  unsigned Index;
};

class CAgent;

enum AGENT_OP
{
  AGENT_OP_Uni,
  AGENT_OP_Delete,
  AGENT_OP_CreateFolder,
  AGENT_OP_Rename,
  AGENT_OP_CopyFromFile,
  AGENT_OP_Comment
};

class CAgentFolder:
  public IFolderFolder,
  public IFolderAltStreams,
  public IFolderProperties,
  public IArchiveGetRawProps,
  public IGetFolderArcProps,
  public IFolderCompare,
  public IFolderGetItemName,
  public IArchiveFolder,
  public IArchiveFolderInternal,
  public IInArchiveGetStream,
  // public IFolderSetReplaceAltStreamCharsMode,
#ifdef NEW_FOLDER_INTERFACE
  public IFolderOperations,
  public IFolderSetFlatMode,
#endif
  public CMyUnknownImp
{
  void LoadFolder(unsigned proxyDirIndex);
public:

  MY_QUERYINTERFACE_BEGIN2(IFolderFolder)
    MY_QUERYINTERFACE_ENTRY(IFolderAltStreams)
    MY_QUERYINTERFACE_ENTRY(IFolderProperties)
    MY_QUERYINTERFACE_ENTRY(IArchiveGetRawProps)
    MY_QUERYINTERFACE_ENTRY(IGetFolderArcProps)
    MY_QUERYINTERFACE_ENTRY(IFolderCompare)
    MY_QUERYINTERFACE_ENTRY(IFolderGetItemName)
    MY_QUERYINTERFACE_ENTRY(IArchiveFolder)
    MY_QUERYINTERFACE_ENTRY(IArchiveFolderInternal)
    MY_QUERYINTERFACE_ENTRY(IInArchiveGetStream)
    // MY_QUERYINTERFACE_ENTRY(IFolderSetReplaceAltStreamCharsMode)
  #ifdef NEW_FOLDER_INTERFACE
    MY_QUERYINTERFACE_ENTRY(IFolderOperations)
    MY_QUERYINTERFACE_ENTRY(IFolderSetFlatMode)
  #endif
  MY_QUERYINTERFACE_END
  MY_ADDREF_RELEASE

  HRESULT BindToFolder_Internal(unsigned proxyDirIndex, IFolderFolder **resultFolder);
  HRESULT BindToAltStreams_Internal(unsigned proxyDirIndex, IFolderFolder **resultFolder);
  int GetRealIndex(unsigned index) const;
  void GetRealIndices(const UInt32 *indices, UInt32 numItems,
      bool includeAltStreams, bool includeFolderSubItemsInFlatMode, CUIntVector &realIndices) const;

  // INTERFACE_FolderSetReplaceAltStreamCharsMode(;)

  INTERFACE_FolderFolder(;)
  INTERFACE_FolderAltStreams(;)
  INTERFACE_FolderProperties(;)
  INTERFACE_IArchiveGetRawProps(;)
  INTERFACE_IFolderGetItemName(;)

  STDMETHOD(GetFolderArcProps)(IFolderArcProps **object);
  STDMETHOD_(Int32, CompareItems)(UInt32 index1, UInt32 index2, PROPID propID, Int32 propIsRaw);
  int CompareItems3(UInt32 index1, UInt32 index2, PROPID propID);
  int CompareItems2(UInt32 index1, UInt32 index2, PROPID propID, Int32 propIsRaw);

  // IArchiveFolder
  INTERFACE_IArchiveFolder(;)
  
  STDMETHOD(GetAgentFolder)(CAgentFolder **agentFolder);

  STDMETHOD(GetStream)(UInt32 index, ISequentialInStream **stream);

  #ifdef NEW_FOLDER_INTERFACE
  INTERFACE_FolderOperations(;)

  STDMETHOD(SetFlatMode)(Int32 flatMode);
  #endif

  CAgentFolder():
      _proxyDirIndex(0),
      _isAltStreamFolder(false),
      _flatMode(false),
      _loadAltStreams(false) // _loadAltStreams alt streams works in flat mode, but we don't use it now
      /* , _replaceAltStreamCharsMode(0) */
      {}

  void Init(const CProxyArc *proxy, const CProxyArc2 *proxy2,
      unsigned proxyDirIndex,
      /* IFolderFolder *parentFolder, */
      CAgent *agent)
  {
    _proxy = proxy;
    _proxy2 = proxy2;
    _proxyDirIndex = proxyDirIndex;
    _isAltStreamFolder = false;
    if (_proxy2)
      _isAltStreamFolder = _proxy2->IsAltDir(proxyDirIndex);
    // _parentFolder = parentFolder;
    _agent = (IInFolderArchive *)agent;
    _agentSpec = agent;
  }

  void GetPathParts(UStringVector &pathParts, bool &isAltStreamFolder);
  HRESULT CommonUpdateOperation(
      AGENT_OP operation,
      bool moveMode,
      const wchar_t *newItemName,
      const NUpdateArchive::CActionSet *actionSet,
      const UInt32 *indices, UInt32 numItems,
      IProgress *progress);


  void GetPrefix(UInt32 index, UString &prefix) const;
  UString GetName(UInt32 index) const;
  UString GetFullPrefix(UInt32 index) const; // relative too root folder of archive

public:
  const CProxyArc *_proxy;
  const CProxyArc2 *_proxy2;
  unsigned _proxyDirIndex;
  bool _isAltStreamFolder;
  // CMyComPtr<IFolderFolder> _parentFolder;
  CMyComPtr<IInFolderArchive> _agent;
  CAgent *_agentSpec;

  CRecordVector<CProxyItem> _items;
  bool _flatMode;
  bool _loadAltStreams; // in Flat mode
  // Int32 _replaceAltStreamCharsMode;
};

class CAgent:
  public IInFolderArchive,
  public IFolderArcProps,
  #ifndef EXTRACT_ONLY
  public IOutFolderArchive,
  public ISetProperties,
  #endif
  public CMyUnknownImp
{
public:

  MY_QUERYINTERFACE_BEGIN2(IInFolderArchive)
    MY_QUERYINTERFACE_ENTRY(IFolderArcProps)
  #ifndef EXTRACT_ONLY
    MY_QUERYINTERFACE_ENTRY(IOutFolderArchive)
    MY_QUERYINTERFACE_ENTRY(ISetProperties)
  #endif
  MY_QUERYINTERFACE_END
  MY_ADDREF_RELEASE

  INTERFACE_IInFolderArchive(;)
  INTERFACE_IFolderArcProps(;)

  #ifndef EXTRACT_ONLY
  INTERFACE_IOutFolderArchive(;)

  HRESULT CommonUpdate(ISequentialOutStream *outArchiveStream,
      unsigned numUpdateItems, IArchiveUpdateCallback *updateCallback);
  
  HRESULT CreateFolder(ISequentialOutStream *outArchiveStream,
      const wchar_t *folderName, IFolderArchiveUpdateCallback *updateCallback100);

  HRESULT RenameItem(ISequentialOutStream *outArchiveStream,
      const UInt32 *indices, UInt32 numItems, const wchar_t *newItemName,
      IFolderArchiveUpdateCallback *updateCallback100);

  HRESULT CommentItem(ISequentialOutStream *outArchiveStream,
      const UInt32 *indices, UInt32 numItems, const wchar_t *newItemName,
      IFolderArchiveUpdateCallback *updateCallback100);

  HRESULT UpdateOneFile(ISequentialOutStream *outArchiveStream,
      const UInt32 *indices, UInt32 numItems, const wchar_t *diskFilePath,
      IFolderArchiveUpdateCallback *updateCallback100);

  // ISetProperties
  STDMETHOD(SetProperties)(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps);
  #endif

  CAgent();
  ~CAgent();
private:
  HRESULT ReadItems();
public:
  CProxyArc *_proxy;
  CProxyArc2 *_proxy2;
  CArchiveLink _archiveLink;

  bool ThereIsPathProp;
  // bool ThereIsAltStreamProp;

  UString ArchiveType;

  FStringVector _names;
  FString _folderPrefix;

  bool _updatePathPrefix_is_AltFolder;
  UString _updatePathPrefix;
  CAgentFolder *_agentFolder;

  UString _archiveFilePath;
  DWORD _attrib;
  bool _isDeviceFile;

  #ifndef EXTRACT_ONLY
  CObjectVector<UString> m_PropNames;
  CObjectVector<NWindows::NCOM::CPropVariant> m_PropValues;
  #endif

  const CArc &GetArc() const { return _archiveLink.Arcs.Back(); }
  IInArchive *GetArchive() const { if ( _archiveLink.Arcs.IsEmpty()) return 0; return GetArc().Archive; }
  bool CanUpdate() const;

  bool Is_Attrib_ReadOnly() const
  {
    return _attrib != INVALID_FILE_ATTRIBUTES && (_attrib & FILE_ATTRIBUTE_READONLY);
  }

  bool IsThereReadOnlyArc() const
  {
    FOR_VECTOR (i, _archiveLink.Arcs)
    {
      const CArc &arc = _archiveLink.Arcs[i];
      if (arc.FormatIndex < 0
          || arc.IsReadOnly
          || !g_CodecsObj->Formats[arc.FormatIndex].UpdateEnabled)
        return true;
    }
    return false;
  }

  UString GetTypeOfArc(const CArc &arc) const
  {
    if (arc.FormatIndex < 0)
      return UString("Parser");
    return g_CodecsObj->GetFormatNamePtr(arc.FormatIndex);
  }

  UString GetErrorMessage() const
  {
    UString s;
    for (int i = _archiveLink.Arcs.Size() - 1; i >= 0; i--)
    {
      const CArc &arc = _archiveLink.Arcs[i];

      UString s2;
      if (arc.ErrorInfo.ErrorFormatIndex >= 0)
      {
        if (arc.ErrorInfo.ErrorFormatIndex == arc.FormatIndex)
          s2 += "Warning: The archive is open with offset";
        else
        {
          s2 += "Cannot open the file as [";
          s2 += g_CodecsObj->GetFormatNamePtr(arc.ErrorInfo.ErrorFormatIndex);
          s2 += "] archive";
        }
      }

      if (!arc.ErrorInfo.ErrorMessage.IsEmpty())
      {
        if (!s2.IsEmpty())
          s2.Add_LF();
        s2 += "\n[";
        s2 += GetTypeOfArc(arc);
        s2 += "]: ";
        s2 += arc.ErrorInfo.ErrorMessage;
      }
      
      if (!s2.IsEmpty())
      {
        if (!s.IsEmpty())
          s += "--------------------\n";
        s += arc.Path;
        s.Add_LF();
        s += s2;
        s.Add_LF();
      }
    }
    return s;
  }

  void KeepModeForNextOpen() { _archiveLink.KeepModeForNextOpen(); }

};


#ifdef NEW_FOLDER_INTERFACE

class CArchiveFolderManager:
  public IFolderManager,
  public CMyUnknownImp
{
  void LoadFormats();
  int FindFormat(const UString &type);
public:
  MY_UNKNOWN_IMP1(IFolderManager)
  INTERFACE_IFolderManager(;)
};

#endif

#endif
