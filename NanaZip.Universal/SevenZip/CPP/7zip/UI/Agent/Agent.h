// Agent/Agent.h

#ifndef ZIP7_INC_AGENT_AGENT_H
#define ZIP7_INC_AGENT_AGENT_H

#include "../../../Common/MyCom.h"

#include "../../../Windows/PropVariant.h"

#include "../Common/LoadCodecs.h"
#include "../Common/OpenArchive.h"
#include "../Common/UpdateAction.h"

#include "../FileManager/IFolder.h"

#include "AgentProxy.h"
#include "IFolderArchive.h"

extern CCodecs *g_CodecsObj;
HRESULT LoadGlobalCodecs();
void FreeGlobalCodecs();

class CAgentFolder;

Z7_PURE_INTERFACES_BEGIN

#define Z7_IFACEM_IArchiveFolderInternal(x) \
  x(GetAgentFolder(CAgentFolder **agentFolder))
Z7_IFACE_CONSTR_FOLDERARC(IArchiveFolderInternal, 0xC)

Z7_PURE_INTERFACES_END

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

class CAgentFolder Z7_final:
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
  public IFolderSetZoneIdMode,
  public IFolderSetZoneIdFile,
  public IFolderOperations,
  public IFolderSetFlatMode,
  public CMyUnknownImp
{
  Z7_COM_QI_BEGIN2(IFolderFolder)
    Z7_COM_QI_ENTRY(IFolderAltStreams)
    Z7_COM_QI_ENTRY(IFolderProperties)
    Z7_COM_QI_ENTRY(IArchiveGetRawProps)
    Z7_COM_QI_ENTRY(IGetFolderArcProps)
    Z7_COM_QI_ENTRY(IFolderCompare)
    Z7_COM_QI_ENTRY(IFolderGetItemName)
    Z7_COM_QI_ENTRY(IArchiveFolder)
    Z7_COM_QI_ENTRY(IArchiveFolderInternal)
    Z7_COM_QI_ENTRY(IInArchiveGetStream)
    Z7_COM_QI_ENTRY(IFolderSetZoneIdMode)
    Z7_COM_QI_ENTRY(IFolderSetZoneIdFile)
    Z7_COM_QI_ENTRY(IFolderOperations)
    Z7_COM_QI_ENTRY(IFolderSetFlatMode)
  Z7_COM_QI_END
  Z7_COM_ADDREF_RELEASE

  Z7_IFACE_COM7_IMP(IFolderFolder)
  Z7_IFACE_COM7_IMP(IFolderAltStreams)
  Z7_IFACE_COM7_IMP(IFolderProperties)
  Z7_IFACE_COM7_IMP(IArchiveGetRawProps)
  Z7_IFACE_COM7_IMP(IGetFolderArcProps)
  Z7_IFACE_COM7_IMP(IFolderCompare)
  Z7_IFACE_COM7_IMP(IFolderGetItemName)
  Z7_IFACE_COM7_IMP(IArchiveFolder)
  Z7_IFACE_COM7_IMP(IArchiveFolderInternal)
  Z7_IFACE_COM7_IMP(IInArchiveGetStream)
  Z7_IFACE_COM7_IMP(IFolderSetZoneIdMode)
  Z7_IFACE_COM7_IMP(IFolderSetZoneIdFile)
  Z7_IFACE_COM7_IMP(IFolderOperations)
  Z7_IFACE_COM7_IMP(IFolderSetFlatMode)

   void LoadFolder(unsigned proxyDirIndex);
public:
  HRESULT BindToFolder_Internal(unsigned proxyDirIndex, IFolderFolder **resultFolder);
  HRESULT BindToAltStreams_Internal(unsigned proxyDirIndex, IFolderFolder **resultFolder);
  int GetRealIndex(unsigned index) const;
  void GetRealIndices(const UInt32 *indices, UInt32 numItems,
      bool includeAltStreams, bool includeFolderSubItemsInFlatMode, CUIntVector &realIndices) const;

  int CompareItems3(UInt32 index1, UInt32 index2, PROPID propID);
  int CompareItems2(UInt32 index1, UInt32 index2, PROPID propID, Int32 propIsRaw);
  int ComparePrefixes(UInt32 index1, UInt32 index2);

  CAgentFolder():
      _isAltStreamFolder(false),
      _flatMode(false),
      _loadAltStreams(false), // _loadAltStreams alt streams works in flat mode, but we don't use it now
      // _loadAltStreams(true), // for debug
      _proxyDirIndex(0),
      _zoneMode(NExtract::NZoneIdMode::kNone)
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
  bool _isAltStreamFolder;
  bool _flatMode;
  bool _loadAltStreams; // in Flat mode
  const CProxyArc *_proxy;
  const CProxyArc2 *_proxy2;
  unsigned _proxyDirIndex;
  NExtract::NZoneIdMode::EEnum _zoneMode;
  CByteBuffer _zoneBuf;
  // Int32 _replaceAltStreamCharsMode;
  // CMyComPtr<IFolderFolder> _parentFolder;
  CMyComPtr<IInFolderArchive> _agent;
  CAgent *_agentSpec;
  CRecordVector<CProxyItem> _items;

  UString _temp1, _temp2;
};



class CAgent Z7_final:
  public IInFolderArchive,
  public IFolderArcProps,
 #ifndef Z7_EXTRACT_ONLY
  public IOutFolderArchive,
  public ISetProperties,
 #endif
  public CMyUnknownImp
{
  Z7_COM_QI_BEGIN2(IInFolderArchive)
    Z7_COM_QI_ENTRY(IFolderArcProps)
 #ifndef Z7_EXTRACT_ONLY
    Z7_COM_QI_ENTRY(IOutFolderArchive)
    Z7_COM_QI_ENTRY(ISetProperties)
 #endif
  Z7_COM_QI_END
  Z7_COM_ADDREF_RELEASE

  Z7_IFACE_COM7_IMP(IInFolderArchive)
  Z7_IFACE_COM7_IMP(IFolderArcProps)

 #ifndef Z7_EXTRACT_ONLY
  Z7_IFACE_COM7_IMP(ISetProperties)

public:
  Z7_IFACE_COM7_IMP(IOutFolderArchive)
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
 #endif

private:
  HRESULT ReadItems();

public:
  CProxyArc *_proxy;
  CProxyArc2 *_proxy2;
  CArchiveLink _archiveLink;

  UString ArchiveType;

  FStringVector _names;
  FString _folderPrefix; // for new files from disk

  UString _updatePathPrefix;
  CAgentFolder *_agentFolder;

  UString _archiveFilePath; // it can be path of non-existing file if file is virtual

  DWORD _attrib;
  bool _updatePathPrefix_is_AltFolder;
  bool ThereIsPathProp;
  bool _isDeviceFile;
  bool _isHashHandler;

  FString _hashBaseFolderPrefix;

 #ifndef Z7_EXTRACT_ONLY
  CObjectVector<UString> m_PropNames;
  CObjectVector<NWindows::NCOM::CPropVariant> m_PropValues;
 #endif

  IArchiveOpenCallback *_progress_ArchiveOpenCallback_for_Open; // it's for parsing after archive openning
  IProgress *_progress_for_Open; // it's for parsing after archive openning

  CAgent();
  ~CAgent();

  const CArc &GetArc() const { return _archiveLink.Arcs.Back(); }
  IInArchive *GetArchive() const { if ( _archiveLink.Arcs.IsEmpty()) return NULL; return GetArc().Archive; }
  bool CanUpdate() const;

  bool Is_Attrib_ReadOnly() const
  {
    return _attrib != INVALID_FILE_ATTRIBUTES && (_attrib & FILE_ATTRIBUTE_READONLY);
  }

  bool IsThere_ReadOnlyArc() const
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
    for (int i = (int)_archiveLink.Arcs.Size() - 1; i >= 0; i--)
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


// #ifdef NEW_FOLDER_INTERFACE

struct CCodecIcons
{
  struct CIconPair
  {
    UString Ext;
    int IconIndex;
  };
  CObjectVector<CIconPair> IconPairs;

  // void Clear() { IconPairs.Clear(); }
  void LoadIcons(HMODULE m);
  bool FindIconIndex(const UString &ext, int &iconIndex) const;
};


Z7_CLASS_IMP_COM_1(
  CArchiveFolderManager
  , IFolderManager
)
  CObjectVector<CCodecIcons> CodecIconsVector;
  CCodecIcons InternalIcons;
  bool WasLoaded;

  void LoadFormats();
  // int FindFormat(const UString &type);
public:
  CArchiveFolderManager():
      WasLoaded(false)
      {}
};

// #endif

#endif
