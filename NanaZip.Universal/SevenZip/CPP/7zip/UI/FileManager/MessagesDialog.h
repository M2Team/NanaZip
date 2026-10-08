// MessagesDialog.h

#ifndef ZIP7_INC_MESSAGES_DIALOG_H
#define ZIP7_INC_MESSAGES_DIALOG_H

#include "../../../Windows/Control/Dialog.h"
#include "../../../Windows/Control/ListView.h"

#include "MessagesDialogRes.h"

class CMessagesDialog: public NWindows::NControl::CModalDialog
{
  NWindows::NControl::CListView _messageList;

  void AddMessageDirect(LPCWSTR message);
  void AddMessage(LPCWSTR message);
  virtual bool OnInit() Z7_override;
  virtual bool OnSize(WPARAM wParam, int xSize, int ySize) Z7_override;
public:
  const UStringVector *Messages;

  INT_PTR Create(HWND parent = NULL) { return CModalDialog::Create(IDD_MESSAGES, parent); }
};

#endif
