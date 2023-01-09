// Quick and dirty C++ emulation of C VLAs

#ifndef S3_VLA_EMULATION_HPP
#define S3_VLA_EMULATION_HPP

#include <cassert>

template<class T> class vla {
  T* _ptr;
  size_t _count;

public:
  vla(size_t count)
      : _ptr(new T[count]), _count(count) {}
  ~vla() { delete[] _ptr; }
  vla(const vla&) = delete;
  vla(vla&&) = delete;
  vla&     operator=(const vla&) = delete;
  vla&     operator=(vla&&) = delete;
  const T& operator[](size_t i) const {
    assert(i < _count);
    return _ptr[i];
  }
  T& operator[](size_t i) {
    assert(i < _count);
    return _ptr[i];
  }
  operator T*() { return _ptr; }
  size_t   size() const { return _count; }
  size_t   size_bytes() const { return _count * sizeof(T); }
};

#endif
