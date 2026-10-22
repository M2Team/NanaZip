/*
Copyright 2011-2026 Frederic Langlet
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
you may obtain a copy of the License at

                http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#pragma once
#ifndef knz_Transform
#define knz_Transform

#include "SliceArray.hpp"

namespace kanzi
{

   // Transform is a class used to transform an input byte array and write
   // the result to an output byte array. The result may have a different size.
   // The transform must be stateless to ensure that the compression results
   // are the same regardless of the number of jobs (ie no information is retained
   // between two invocations of forward or inverse).
   template <class T>
   class Transform
   {
   public:
       Transform(){}

       // The index of each slice array returns the number of bytes read & written respectively.
       // If the method returns false, the values of the indexes are best effort only.
       virtual bool forward(SliceArray<T>& src, SliceArray<T>& dst, int length) = 0;

       // The index of each slice array returns the number of bytes read & written respectively.
       // If the method returns false, the values of the indexes are best effort only.
       virtual bool inverse(SliceArray<T>& src, SliceArray<T>& dst, int length) = 0;

       // Return the maximum possible length of the transformed data
       virtual int getMaxEncodedLength(int srcLen) const = 0;

       virtual ~Transform(){}
   };

}
#endif

