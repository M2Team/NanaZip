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
#ifndef knz_IOUtil
#define knz_IOUtil

#include <algorithm>
#include <errno.h>
#include <sstream>
#include <vector>
#include <sys/stat.h>
#include <string.h>

#if __cplusplus >= 201703L
#include <filesystem>
#endif

#include "../types.hpp"

#ifdef _MSC_VER
#include "../msvc_dirent.hpp"
#include <direct.h>
#else
#include <dirent.h>
#endif

// Note stat64/lstat64 are deprecated on MacOS/Linux
// Use _FILE_OFFSET_BITS and stat/lstat instead

#ifdef _WIN32
   #define STAT _stat64
   #define LSTAT _stat64
#else
   #define STAT stat
   #define LSTAT lstat
#endif

#if defined(__MINGW32__)
   // Missing from stat in mingw32
   const int S_IFLNK = 40960;
#endif


namespace kanzi
{
   struct FileData {
         std::string _path;
         std::string _name;
         int64 _size;
         int64 _modifTime;

         FileData(const std::string& path, int64 size, int64 _modifTime = 0)
            : _size(size)
            , _modifTime(_modifTime)
         {
            size_t idx = path.find_last_of(PATH_SEPARATOR);

            if (idx != std::string::npos) {
               _path = path.substr(0, idx + 1);
               _name = path.substr(idx + 1);
            }
            else {
               _path = "";
               _name = path;
            }
         }

         std::string fullPath() const {
            return (_path.length() == 0) ? _name : _path + _name;
         }
   };


   struct FileListConfig
   {
      bool _recursive;
      bool _ignoreLinks; // Do not follow links
      bool _continueOnErrors;
      bool _ignoreDotFiles; // Do not process dot files
   };



   static inline void createFileList(std::string& target, std::vector<FileData>& files, const FileListConfig& cfg,
                                     std::vector<std::string>& errors)
   {
       if (target.size() == 0)
           return;

       // Note: old version of Windows/Visual Studio require a trailing '/' to stat network folders !
       // In this scenario, "//PC/share" does not work but "//PC/share/" does
   #ifndef _MSC_VER
       if ((target.size() > 1) && (target[target.size() - 1] == PATH_SEPARATOR))
           target.resize(target.size() - 1);
   #endif

       if (cfg._ignoreDotFiles == true) {
           size_t idx = target.rfind(PATH_SEPARATOR);

           if ((idx != std::string::npos) && (idx < target.length() - 1) && (target[idx + 1] == '.'))
               return;
       }

       struct STAT buffer;
       int res = cfg._ignoreLinks ? LSTAT(target.c_str(), &buffer) : STAT(target.c_str(), &buffer);

       if (res != 0) {
           std::stringstream ss;
           ss << "Cannot access input file '" << target << "': " << strerror(errno);
           errors.push_back(ss.str());

           if (cfg._continueOnErrors == false)
              return;
       }

    #ifdef _WIN32
       if (S_ISREG(buffer.st_mode)) {
   #else
       if (S_ISREG(buffer.st_mode) || (!cfg._ignoreLinks && S_ISLNK(buffer.st_mode))) {
   #endif
   #if __cplusplus >= 201103L
           files.emplace_back(target, buffer.st_size, buffer.st_mtime);
   #else
           files.push_back(FileData(target, buffer.st_size, buffer.st_mtime));
   #endif

           return;
       }

       if (!S_ISDIR(buffer.st_mode)) {
           // Target is neither regular file nor directory, ignore
           return;
       }

       if (cfg._recursive) {
          if (target[target.size() - 1] != PATH_SEPARATOR)
             target += PATH_SEPARATOR;
       }
       else {
          target.resize(target.size() - 1);
       }

       DIR* dir = opendir(target.c_str());

       if (dir != nullptr) {
           const struct dirent* ent;

           while ((ent = readdir(dir)) != nullptr) {
               std::string dirName = ent->d_name;

               if ((dirName == ".") || (dirName == ".."))
                  continue;

               std::string fullpath = target + dirName;
               res = cfg._ignoreLinks ? LSTAT(fullpath.c_str(), &buffer) : STAT(fullpath.c_str(), &buffer);

               if (res != 0) {
                   std::stringstream ss;
                   ss << "Cannot access input file '" << fullpath << "': " << strerror(errno);
                   errors.push_back(ss.str());

                   if (cfg._continueOnErrors == false) {
                       closedir(dir);
                       return;
                   }
               }

            #ifdef _WIN32
               if (S_ISREG(buffer.st_mode)) {
            #else
               if (S_ISREG(buffer.st_mode) || (!cfg._ignoreLinks && S_ISLNK(buffer.st_mode))) {
            #endif
                  // Target is regular file
                  if (cfg._ignoreDotFiles == true) {
                     size_t idx = fullpath.rfind(PATH_SEPARATOR);

                     if ((idx != std::string::npos) && (idx < fullpath.length() - 1) && (fullpath[idx + 1] == '.'))
                        continue;
                  }

   #if __cplusplus >= 201103L
                  files.emplace_back(fullpath, buffer.st_size, buffer.st_mtime);
   #else
                  files.push_back(FileData(fullpath, buffer.st_size, buffer.st_mtime));
   #endif
               }
               else if (cfg._recursive && S_ISDIR(buffer.st_mode)) {
                  if (cfg._ignoreDotFiles == true) {
                     size_t idx = fullpath.rfind(PATH_SEPARATOR);

                     if ((idx != std::string::npos) && (idx < fullpath.length() - 1) && (fullpath[idx + 1] == '.'))
                        continue;
                  }

                  createFileList(fullpath, files, cfg, errors);
               }
           }

           closedir(dir);
       }
       else {
           std::stringstream ss;
           ss << "Cannot read directory '" << target << "'";
           errors.push_back(ss.str());
       }
   }


   struct FileDataComparator
   {
       bool _sortBySize;

       bool operator() (const FileData& f1, const FileData& f2) const
       {
           // First, compare parent directory paths
           if (f1._path != f2._path)
              return f1._path < f2._path;

           // Then compare file sizes (decreasing order)
           return _sortBySize ? f1._size > f2._size : f1._name < f2._name;
       }
   };


   static inline void sortFilesByPathAndSize(std::vector<FileData>& files, bool sortBySize = false)
   {
       if (files.size() > 1) {
          FileDataComparator c = { sortBySize };
          sort(files.begin(), files.end(), c);
       }
   }


#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)
   static inline int mkdirAll(const std::string& path)
   {
      bool foundDrive = false;
#else
   static inline int mkdirAll(const std::string& path, mode_t mode = S_IRWXU | S_IRWXG | S_IROTH | S_IWOTH | S_IXOTH)
   {
#endif

      int res = 0;
      errno = 0;

      // Scan path, ignoring potential PATH_SEPARATOR at position 0
      for (size_t i = 1; i < path.size(); i++) {
          if (path[i] == PATH_SEPARATOR) {
              std::string curPath = path;
              curPath.resize(i);

              if (curPath.length() == 0)
                  continue;

#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)
              //Skip if drive
              if ((foundDrive == false) && (curPath.length() == 2) && (curPath[1] == ':')) {
                  foundDrive = true;
                  continue;
              }
#endif

#if defined(_MSC_VER)
              res = _mkdir(curPath.c_str());
#elif defined(__MINGW32__)
              res = mkdir(curPath.c_str());
#else
              res = mkdir(curPath.c_str(), mode);
#endif
              if ((res != 0) && (errno != EEXIST))
                  return errno;
          }
      }
      errno = 0;

#if defined(_MSC_VER)
      res = _mkdir(path.c_str());
#elif defined(__MINGW32__)
      res = mkdir(path.c_str());
#else
      res = mkdir(path.c_str(), mode);
#endif

      return (res == 0) ? 0 : errno;
   }


   static inline bool samePaths(const std::string& f1, const std::string& f2)
   {
#if __cplusplus >= 201703L
      // Simpler and safer code with C++17
      std::error_code ec;
      return std::filesystem::equivalent(f1, f2, ec);
#else
      if (f1.compare(f2) == 0)
         return true;

      struct STAT buf1;
      int s1 = STAT(f1.c_str(), &buf1);
      struct STAT buf2;
      int s2 = STAT(f2.c_str(), &buf2);

      if ((s1 < 0) && (s2 < 0))
         return false;

      if (s1 != s2)
         return false;

      if (buf1.st_dev != buf2.st_dev)
         return false;

      if (buf1.st_ino != buf2.st_ino)
         return false;

      if (buf1.st_mode != buf2.st_mode)
         return false;

      if (buf1.st_nlink != buf2.st_nlink)
         return false;

      if (buf1.st_uid != buf2.st_uid)
         return false;

      if (buf1.st_gid != buf2.st_gid)
         return false;

      if (buf1.st_rdev != buf2.st_rdev)
         return false;

      if (buf1.st_size != buf2.st_size)
         return false;

      if (buf1.st_atime != buf2.st_atime)
         return false;

      if (buf1.st_mtime != buf2.st_mtime)
         return false;

      if (buf1.st_ctime != buf2.st_ctime)
         return false;

      return true;
#endif
   }

}
#endif
