// ExtractEngine.h

#ifndef __EXTRACT_ENGINE_H
#define __EXTRACT_ENGINE_H

#include "../../../Common/MyCom.h"
#include "../../../Common/MyString.h"

#include "../../IPassword.h"
#include "../Agent/IFolderArchive.h"

#include "ProgressBox.h"

class CExtractCallbackImp:
  public IFolderArchiveExtractCallback,
  public IFolderArchiveExtractCallback2,
  public ICryptoGetTextPassword,
  public CMyUnknownImp
{
public:
  MY_UNKNOWN_IMP2(ICryptoGetTextPassword, IFolderArchiveExtractCallback2)

  // IProgress
  STDMETHOD(SetTotal)(UInt64 size);
  STDMETHOD(SetCompleted)(const UInt64 *completeValue);

  INTERFACE_IFolderArchiveExtractCallback(;)
  INTERFACE_IFolderArchiveExtractCallback2(;)

  // ICryptoGetTextPassword
  STDMETHOD(CryptoGetTextPassword)(BSTR *password);

private:
  UString m_CurrentFilePath;

  CProgressBox *_percent;
  UINT m_CodePage;

  bool m_PasswordIsDefined;
  UString m_Password;

  void CreateComplexDirectory(const UStringVector &dirPathParts);
  /*
  void GetPropertyValue(LPITEMIDLIST anItemIDList, PROPID aPropId,
      PROPVARIANT *aValue);
  bool IsEncrypted(LPITEMIDLIST anItemIDList);
  */
  void AddErrorMessage(LPCTSTR message);
public:
  // CExtractCallbackImp() {}
  ~CExtractCallbackImp();
  void Init(UINT codePage,
      CProgressBox *progressBox,
      bool passwordIsDefined, const UString &password);
};

#endif
