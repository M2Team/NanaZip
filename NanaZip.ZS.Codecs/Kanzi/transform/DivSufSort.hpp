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
#ifndef knz_DivSufSort
#define knz_DivSufSort

#include "../types.hpp"

#if __cplusplus >= 201103L
   #include <utility>
#else
   #include <algorithm>
#endif


namespace kanzi
{

   // DivSufSort is a fast two-stage suffix sorting algorithm by Yuta Mori.
   // The original C code is here: https://code.google.com/p/libdivsufsort/
   // See also https://code.google.com/p/libdivsufsort/source/browse/wiki/SACA_Benchmarks.wiki
   // for comparison of different suffix array construction algorithms.
   // It is used to implement the forward stage of the BWT in linear time.
   struct StackElement
   {
       int _a, _b, _c, _d, _e;

       StackElement() { _a = _b = _c = _d = _e = 0; }
   };

   // A stack of pre-allocated elements
   class Stack
   {
       friend class DivSufSort;

   private:
       StackElement* _arr;
       int _index;

       Stack(int size)
       {
           _arr = new StackElement[size];
           _index = 0;
       }

       ~Stack() { delete[] _arr; }

       StackElement* get(int idx) const { return &_arr[idx]; }

       int size() const { return _index; }

       void push(int a, int b, int c, int d, int e)
       {
           StackElement* elt = &_arr[_index];
           elt->_a = a;
           elt->_b = b;
           elt->_c = c;
           elt->_d = d;
           elt->_e = e;
           _index++;
       }

       StackElement* pop() { return (_index == 0) ? nullptr : &_arr[--_index]; }
   };



   struct TRBudget
   {
       int _chance;
       int _remain;
       int _incVal;
       int _count;

       TRBudget(int chance, int incval)
          : _chance(chance)
          , _remain(incval)
       {
           _incVal = incval;
           _count = 0;
       }

       bool check(int size);
   };


   inline bool TRBudget::check(int size)
   {
       if (size <= _remain) {
           _remain -= size;
           return true;
       }

       if (_chance == 0) {
           _count += size;
           return false;
       }

       _remain += (_incVal - size);
       _chance--;
       return true;
   }



   class DivSufSort
   {
   private:
       static const int SS_INSERTIONSORT_THRESHOLD;
       static const int SS_BLOCKSIZE;
       static const int SS_MISORT_STACKSIZE;
       static const int SS_SMERGE_STACKSIZE;
       static const int TR_STACKSIZE;
       static const int TR_INSERTIONSORT_THRESHOLD;
       static const int SQQ_TABLE[];
       static const int LOG_TABLE[];

       int* _sa;
       const uint8* _buffer;
       Stack* _ssStack;
       Stack* _trStack;
       Stack* _mergeStack;
       int _bucketA[256];
       int _bucketB[65536];


       void constructSuffixArray(int bucketA[], int bucketB[], int n, int m);

       int constructBWT(int bucketA[], int bucketB[], int n, int m, int indexes[], int idxCount);

       int sortTypeBstar(int bucketA[], int bucketB[], int n);

       void ssSort(int pa, int first, int last, int buf, int bufSize,
           int depth, int n, bool lastSuffix);

       int ssCompare(int pa, int pb, int p2, const int depth) const;

       int ssCompare(const int s1[], const int s2[], const int depth) const;

       void ssInplaceMerge(int pa, int first, int middle, int last, int depth);

       void ssRotate(int first, int middle, int last);

       void ssBlockSwap(int a, int b, int n);

       static int getIndex(int a) { return (a >= 0) ? a : ~a; }

       void ssSwapMerge(int pa, int first, int middle, int last, int buf,
           int bufSize, int depth);

       void ssMergeForward(int pa, int first, int middle, int last, int buf,
           int depth);

       void ssMergeBackward(int pa, int first, int middle, int last, int buf,
           int depth);

       void ssInsertionSort(int pa, int first, int last, int depth);

       int ssIsqrt(int x) const;

       void ssMultiKeyIntroSort(int pa, int first, int last, int depth);

       int ssPivot(int td, int pa, int first, int last) const;

       int ssMedian5(const uint8 buf[], int pa, int v1, int v2, int v3, int v4, int v5) const;

       int ssMedian3(const uint8 buf[], int pa, int v1, int v2, int v3) const;

       int ssPartition(int pa, int first, int last, int depth);

       void ssHeapSort(int idx, int pa, int saIdx, int size);

       void ssFixDown(int idx, int pa, int saIdx, int i, int size);

       static int ssIlg(int n);

       void trSort(int n, int depth);

       uint64 trPartition(int isad, int first, int middle, int last, int v);

       void trIntroSort(int isa, int isad, int first, int last, TRBudget& budget);

       int trPivot(const int arr[], int isad, int first, int last) const;

       int trMedian5(const int arr[], int isad, int v1, int v2, int v3, int v4, int v5) const;

       int trMedian3(const int arr[], int isad, int v1, int v2, int v3) const;

       void trHeapSort(int isad, int saIdx, int size);

       void trFixDown(int isad, int saIdx, int i, int size);

       void trInsertionSort(const int arr[], int first, int last);

       void trPartialCopy(int isa, int first, int a, int b, int last, int depth);

       void trCopy(int isa, int first, int a, int b, int last, int depth);

       void reset();

       int trIlg(int n) const;

   public:
       DivSufSort();

       ~DivSufSort();

       bool computeSuffixArray(const byte input[], int sa[], int length);

       bool computeBWT(const byte input[], byte output[], int sa[], int length, int indexes[], int idxCount = 8);
   };


   inline int DivSufSort::ssIlg(int n)
   {
       return (n > 255) ? 8 + LOG_TABLE[n >> 8] : LOG_TABLE[n & 0xFF];
   }


   inline void DivSufSort::ssBlockSwap(int a, int b, int n)
   {
       while (n-- > 0) {
           std::swap(_sa[a], _sa[b]);
           a++;
           b++;
       }
   }


   inline int DivSufSort::trIlg(int n) const
   {
       return ((n & 0xFFFF0000) != 0) ? (((n & 0xFF000000) != 0) ? 24 + LOG_TABLE[(n >> 24) & 0xFF]
                                                                 : 16 + LOG_TABLE[(n >> 16) & 0xFF])
                                      : (((n & 0x0000FF00) != 0) ? 8 + LOG_TABLE[(n >> 8) & 0xFF]
                                                                 : LOG_TABLE[n & 0xFF]);
   }


   inline int DivSufSort::trMedian5(const int sa[], int isad, int v1, int v2, int v3, int v4, int v5) const
   {
       if (sa[isad + sa[v2]] > sa[isad + sa[v3]]) {
           std::swap(v2, v3);
       }

       if (sa[isad + sa[v4]] > sa[isad + sa[v5]]) {
           const int t = v4;
           v4 = v5;
           v5 = t;
       }

       if (sa[isad + sa[v2]] > sa[isad + sa[v4]]) {
           std::swap(v2, v4);
           std::swap(v3, v5);
       }

       if (sa[isad + sa[v1]] > sa[isad + sa[v3]]) {
           std::swap(v1, v3);
       }

       if (sa[isad + sa[v1]] > sa[isad + sa[v4]]) {
           std::swap(v1, v4);
           std::swap(v3, v5);
       }

       if (sa[isad + sa[v3]] > sa[isad + sa[v4]])
           return v4;

       return v3;
   }


   inline int DivSufSort::trMedian3(const int sa[], int isad, int v1, int v2, int v3) const
   {
       if (sa[isad + sa[v1]] > sa[isad + sa[v2]]) {
           std::swap(v1, v2);
       }

       if (sa[isad + sa[v2]] > sa[isad + sa[v3]]) {
           if (sa[isad + sa[v1]] > sa[isad + sa[v3]])
               return v1;

           return v3;
       }

       return v2;
   }


   inline int DivSufSort::ssIsqrt(int x) const
   {
       if (x >= (SS_BLOCKSIZE * SS_BLOCKSIZE))
           return SS_BLOCKSIZE;

       const int e = ((x & 0xFFFF0000) != 0) ? (((x & 0xFF000000) != 0) ? 24 + LOG_TABLE[(x >> 24) & 0xFF]
                                                                        : 16 + LOG_TABLE[(x >> 16) & 0xFF])
                                             : (((x & 0x0000FF00) != 0) ? 8 + LOG_TABLE[(x >> 8) & 0xFF]
                                                                        : LOG_TABLE[x & 0xFF]);

       if (e < 8)
           return SQQ_TABLE[x] >> 4;

       int y;

       if (e >= 16) {
           y = SQQ_TABLE[x >> ((e - 6) - (e & 1))] << ((e >> 1) - 7);

           if (e >= 24) {
               y = (y + 1 + x / y) >> 1;
           }

           y = (y + 1 + x / y) >> 1;
       }
       else {
           y = (SQQ_TABLE[x >> ((e - 6) - (e & 1))] >> (7 - (e >> 1))) + 1;
       }

       return (x < y * y) ? y - 1 : y;
   }

}
#endif

