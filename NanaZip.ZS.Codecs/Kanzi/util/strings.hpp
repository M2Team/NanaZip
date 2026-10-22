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
#ifndef knz_strings
#define knz_strings

#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>



#if __cplusplus < 201103L
   // to_string() not available before C++ 11
   template <typename T>
   std::string to_string(T value)
   {
       std::ostringstream os;
       os << value;
       return os.str();
   }

   #define TOSTR(v) to_string(v)
#else
   #define TOSTR(v) std::to_string(v)
#endif


inline void to_binary(int num, char* buffer, int length)
{
    for (int i = length - 2; i >= 0; i--) {
        buffer[i] = (num & 1) ? '1' : '0';
        num >>= 1;
    }

    buffer[length - 1] = '\0';
}

// trim from end of string (right)
inline std::string& rtrim(std::string& s)
{
    static const char* whitespaces = " \t\f\v\n\r";
    std::size_t pos = s.find_last_not_of(whitespaces);

    if (pos == std::string::npos)
       s.clear();
    else
       s.erase(pos + 1);

    return s;
}

// trim from beginning of string (left)
inline std::string& ltrim(std::string& s)
{
    static const char* whitespaces = " \t\f\v\n\r";
    std::size_t pos = s.find_first_not_of(whitespaces);

    if (pos == std::string::npos)
       s.clear();
    else
       s.erase(0, pos);

    return s;
}

// Ensures that the function works on platforms where char is signed
inline char safeToUpper(char c)
{
    return static_cast<char>(::toupper(static_cast<unsigned char>(c)));
}

// trim from both ends of string (right then left)
inline std::string& trim(std::string& s)
{
    return ltrim(rtrim(s));
}

inline void tokenize(const std::string& str, std::vector<std::string>& v, char token)
{
   std::istringstream ss(str);
   std::string s;    

   while (getline(ss, s, token)) 
      v.push_back(s);   
}    

inline std::string escapeJSONString(const std::string& src)
{
    std::stringstream ss;

    for (size_t i = 0; i < src.size(); i++) {
        const unsigned char c = static_cast<unsigned char>(src[i]);

        switch (c) {
            case '"':  ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b";  break;
            case '\f': ss << "\\f";  break;
            case '\n': ss << "\\n";  break;
            case '\r': ss << "\\r";  break;
            case '\t': ss << "\\t";  break;
            default:
                if (c < 0x20) {
                    ss << "\\u00";
                    ss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << int(c);
                    ss << std::dec;
                }
                else {
                    ss << char(c);
                }
        }
    }

    return ss.str();
}

inline std::string formatSize(double size)
{
    std::stringstream ss;
    std::string s;

    if (size >= double(1 << 30)) {
       size /= double(1024 * 1024 * 1024);
       ss << std::fixed << std::setprecision(2) << size << " GiB";
       s = ss.str();
    }
    else if (size >= double(1 << 20)) {
       size  /= double(1024 * 1024);
       ss << std::fixed << std::setprecision(2) << size << " MiB";
       s = ss.str();
    }
    else if (size >= double(1 << 10)) {
       size /= double(1024);
       ss << std::fixed << std::setprecision(2) << size << " KiB";
       s = ss.str();
    }
    else {
       ss << size;
       s = ss.str();
    }

   return s;
}

inline std::string formatSize(const std::string& input)
{
    return formatSize(atof(input.c_str()));
}

#endif
