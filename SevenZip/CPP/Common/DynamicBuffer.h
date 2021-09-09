// Common/DynamicBuffer.h

#ifndef __COMMON_DYNAMIC_BUFFER_H
#define __COMMON_DYNAMIC_BUFFER_H

template <class T> class CDynamicBuffer
{
  T *_items;
  size_t _size;
  size_t _pos;

  CDynamicBuffer(const CDynamicBuffer &buffer);
  void operator=(const CDynamicBuffer &buffer);

  void Grow(size_t size)
  {
    size_t delta = _size >= 64 ? _size : 64;
    if (delta < size)
      delta = size;
    size_t newCap = _size + delta;
    if (newCap < delta)
    {
      newCap = _size + size;
      if (newCap < size)
        throw 20120116;
    }

    T *newBuffer = new T[newCap];
    if (_pos != 0)
      memcpy(newBuffer, _items, _pos * sizeof(T));
    delete []_items;
    _items = newBuffer;
    _size = newCap;
  }

public:
  CDynamicBuffer(): _items(0), _size(0), _pos(0) {}
  // operator T *() { return _items; }
  operator const T *() const { return _items; }
  ~CDynamicBuffer() { delete []_items; }

  T *GetCurPtrAndGrow(size_t addSize)
  {
    size_t rem = _size - _pos;
    if (rem < addSize)
      Grow(addSize - rem);
    T *res = _items + _pos;
    _pos += addSize;
    return res;
  }

  void AddData(const T *data, size_t size)
  {
    memcpy(GetCurPtrAndGrow(size), data, size * sizeof(T));
  }

  size_t GetPos() const { return _pos; }

  // void Empty() { _pos = 0; }
};

typedef CDynamicBuffer<unsigned char> CByteDynamicBuffer;

#endif
