// IPassword.h

#ifndef __IPASSWORD_H
#define __IPASSWORD_H

#include "../Common/MyTypes.h"
#include "../Common/MyUnknown.h"

#include "IDecl.h"

#define PASSWORD_INTERFACE(i, x) DECL_INTERFACE(i, 5, x)

/*
How to use output parameter (BSTR *password):

in:  The caller is required to set BSTR value as NULL (no string).
     The callee (in 7-Zip code) ignores the input value stored in BSTR variable,

out: The callee rewrites BSTR variable (*password) with new allocated string pointer.
     The caller must free BSTR string with function SysFreeString();
*/

PASSWORD_INTERFACE(ICryptoGetTextPassword, 0x10)
{
  STDMETHOD(CryptoGetTextPassword)(BSTR *password) PURE;
};


/*
CryptoGetTextPassword2()
in:
  The caller is required to set BSTR value as NULL (no string).
  The caller is not required to set (*passwordIsDefined) value.

out:
  Return code: != S_OK : error code
  Return code:    S_OK : success
   
  if (*passwordIsDefined == 1), the variable (*password) contains password string
    
  if (*passwordIsDefined == 0), the password is not defined,
     but the callee still could set (*password) to some allocated string, for example, as empty string.
  
  The caller must free BSTR string with function SysFreeString()
*/


PASSWORD_INTERFACE(ICryptoGetTextPassword2, 0x11)
{
  STDMETHOD(CryptoGetTextPassword2)(Int32 *passwordIsDefined, BSTR *password) PURE;
};

#endif
