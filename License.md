# NanaZip License

For giving the maximum respect for the upstream projects and following the 
philosophy about open-source software from Kenji Mouri (MouriNaruto), the one
of the M2-Team founders. 

The source code of NanaZip (not including the source code from third-party 
libraries, 7-Zip or other 7-Zip derivatives) is distributed under the MIT 
License.

The source code from 7-Zip or other 7-Zip derivatives (these contents are only 
in `SevenZip` and `NanaZipPackage\Lang` folders) is distributed under the 7-Zip
License.

The source code from the third-party libraries is distributed under the original
license used in the third-party libraries.

This permission notice shall be included in all copies or substantial portions
of the Software.

### The philosophy about open-source software from Kenji Mouri (MouriNaruto)

```
You shouldn't make your software open source if you don't want your source code
or ideas used in proprietary software. Because they always have the way to cross
restrictions if they really want to do, even you distributed your source code 
under the strictest copyleft license, they can use clean room to resolve it. 

Use copyleft licenses make you feel anxious because you always need to worry 
about someone uses your source code in proprietary software. So, I choose to 
give the maximum respect to users and I also hope every people treat others 
kindly.

Kenji Mouri
```

### The MIT License

```
The MIT License (MIT)

Copyright (c) M2-Team and Contributors. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### The 7-Zip License

```
  7-Zip
  ~~~~~
  License for use and distribution
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  7-Zip Copyright (C) 1999-2021 Igor Pavlov.

  The licenses for files are:

    1) 7z.dll:
         - The "GNU LGPL" as main license for most of the code
         - The "GNU LGPL" with "unRAR license restriction" for some code
         - The "BSD 3-clause License" for some code
    2) All other files: the "GNU LGPL".

  Redistributions in binary form must reproduce related license information 
  from this file.

  Note:
    You can use 7-Zip on any computer, including a computer in a commercial
    organization. You don't need to register or pay for 7-Zip.


  GNU LGPL information
  --------------------

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You can receive a copy of the GNU Lesser General Public License from
    http://www.gnu.org/


  BSD 3-clause License
  --------------------

    The "BSD 3-clause License" is used for the code in 7z.dll that implements
    LZFSE data decompression. That code was derived from the code in the "LZFSE
    compression library" developed by Apple Inc, that also uses the "BSD 
    3-clause License":

    ----
    Copyright (c) 2015-2016, Apple Inc. All rights reserved.

    Redistribution and use in source and binary forms, with or without 
    modification, are permitted provided that the following conditions are met:

    1.  Redistributions of source code must retain the above copyright notice, 
        this list of conditions and the following disclaimer.

    2.  Redistributions in binary form must reproduce the above copyright 
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.

    3.  Neither the name of the copyright holder(s) nor the names of any 
        contributors may be used to endorse or promote products derived from 
        this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
    THE POSSIBILITY OF SUCH DAMAGE.
    ----


  unRAR license restriction
  -------------------------

    The decompression engine for RAR archives was developed using source
    code of unRAR program.
    All copyrights to original unRAR code are owned by Alexander Roshal.

    The license for original unRAR code has the following restriction:

      The unRAR sources cannot be used to re-create the RAR compression 
      algorithm, which is proprietary. Distribution of modified unRAR sources
      in separate form or as a part of other software is permitted, provided
      that it is clearly stated in the documentation and source comments that
      the code may not be used to develop a RAR (WinRAR) compatible archiver.


  --
  Igor Pavlov
```

### The third-party libraries used in NanaZip

- C++/WinRT, https://github.com/microsoft/cppwinrt
- Mile.Cpp, https://github.com/ProjectMile/Mile.Cpp
- VC-LTL, https://github.com/Chuyu-Team/VC-LTL5
- Windows UI Library, https://github.com/microsoft/microsoft-ui-xaml
