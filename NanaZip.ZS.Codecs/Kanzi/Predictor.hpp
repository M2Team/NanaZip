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
#ifndef knz_Predictor
#define knz_Predictor

namespace kanzi
{

   // Predictor predicts the probability of the next bit being 1.
   class Predictor
   {
   public:
       Predictor(){}

       // Updates the internal probability model based on the observed bit
       virtual void update(int bit) = 0;

       // Returns the value representing the probability of the next bit being 1
       // in the [0..4095] range.
       // E.G. 410 represents roughly a probability of 10% for 1
       virtual int get() = 0;

       virtual ~Predictor(){}
   };

}
#endif

