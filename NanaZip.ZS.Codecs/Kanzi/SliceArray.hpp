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
#ifndef knz_SliceArray
#define knz_SliceArray

namespace kanzi
{

   template <class T>
   class SliceArray
   {
   public:
      T* _array;
      int _length; // buffer length (a.k.a capacity)
      int _index;

      SliceArray(T* arr, int len, int index = 0) : _array(arr), _length(len), _index(index) {}

#if __cplusplus < 201103L
      SliceArray(const SliceArray& sa) { _array = sa._array; _length = sa._length; _index = sa._index; }

      SliceArray& operator=(const SliceArray& sa);

      ~SliceArray(){} // does not deallocate buffer memory
#else
      SliceArray(SliceArray&& sa) noexcept = default;

      SliceArray& operator=(SliceArray&& sa) noexcept = default;

      ~SliceArray() = default;
#endif

      // Utility methods
      static bool isValid(const SliceArray& sa);
   };

   template <class T>
   inline bool SliceArray<T>::isValid(const SliceArray& sa) {
       return ((sa._array != nullptr) && (sa._index >= 0) && (sa._length >= 0) && (sa._index <= sa._length));
   }

#if __cplusplus < 201103L
   template <class T>
   inline SliceArray<T>& SliceArray<T>::operator=(const SliceArray& sa) {
      _array = sa._array;
      _length = sa._length;
      _index = sa._index;
      return *this;
   }
#endif

}
#endif

