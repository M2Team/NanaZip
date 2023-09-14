// Common/AutoPtr.h

#ifndef ZIP7_INC_COMMON_AUTOPTR_H
#define ZIP7_INC_COMMON_AUTOPTR_H

template<class T> class CMyAutoPtr
{
  T *_p;
public:
  CMyAutoPtr(T *p = NULL) : _p(p) {}
  CMyAutoPtr(CMyAutoPtr<T>& p): _p(p.release()) {}
  CMyAutoPtr<T>& operator=(CMyAutoPtr<T>& p)
  {
    reset(p.release());
    return (*this);
  }
  ~CMyAutoPtr() { delete _p; }
  T& operator*() const { return *_p; }
  // T* operator->() const { return (&**this); }
  T* get() const { return _p; }
  T* release()
  {
    T *tmp = _p;
    _p = NULL;
    return tmp;
  }
  void reset(T* p = NULL)
  {
    if (p != _p)
      delete _p;
    _p = p;
  }
};

#endif
