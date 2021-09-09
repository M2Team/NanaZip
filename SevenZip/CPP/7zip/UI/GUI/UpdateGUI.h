// GUI/UpdateGUI.h

#ifndef __UPDATE_GUI_H
#define __UPDATE_GUI_H

#include "../Common/Update.h"

#include "UpdateCallbackGUI.h"

/*
  callback->FailedFiles contains names of files for that there were problems.
  RESULT can be S_OK, even if there are such warnings!!!
  
  RESULT = E_ABORT - user break.
  RESULT != E_ABORT:
  {
   messageWasDisplayed = true  - message was displayed already.
   messageWasDisplayed = false - there was some internal error, so you must show error message.
  }
*/

HRESULT UpdateGUI(
    CCodecs *codecs,
    const CObjectVector<COpenType> &formatIndices,
    const UString &cmdArcPath2,
    NWildcard::CCensor &censor,
    CUpdateOptions &options,
    bool showDialog,
    bool &messageWasDisplayed,
    CUpdateCallbackGUI *callback,
    HWND hwndParent = NULL);

#endif
