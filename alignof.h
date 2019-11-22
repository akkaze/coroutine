#pragma once
#include <cstddef>

#ifndef _MSC_VER

template<std::size_t Alignment, std::size_t Size>
struct AlignedCharArray {
  alignas(Alignment) char buffer[Size];
};

#else // _MSC_VER

/// Create a type with an aligned char buffer.
template<std::size_t Alignment, std::size_t Size>
struct AlignedCharArray;

template<std::size_t Size>
struct AlignedCharArray<1, Size> {
  union {
    char aligned;
    char buffer[Size];
  };
};

template<std::size_t Size>
struct AlignedCharArray<2, Size> {
  union {
    short aligned;
    char buffer[Size];
  };
};

template<std::size_t Size>
struct AlignedCharArray<4, Size> {
  union {
    int aligned;
    char buffer[Size];
  };
};

template<std::size_t Size>
struct AlignedCharArray<8, Size> {
  union {
    double aligned;
    char buffer[Size];
  };
};



#define ALIGNEDCHARARRAY_TEMPLATE_ALIGNMENT(x) \
  template<std::size_t Size> \
  struct AlignedCharArray<x, Size> { \
    __declspec(align(x)) char buffer[Size]; \
  };

ALIGNEDCHARARRAY_TEMPLATE_ALIGNMENT(16)
ALIGNEDCHARARRAY_TEMPLATE_ALIGNMENT(32)
ALIGNEDCHARARRAY_TEMPLATE_ALIGNMENT(64)
ALIGNEDCHARARRAY_TEMPLATE_ALIGNMENT(128)

#undef ALIGNEDCHARARRAY_TEMPLATE_ALIGNMENT

#endif // _MSC_VER

namespace detail {
template <typename T1,
          typename T2 = char, typename T3 = char, typename T4 = char,
          typename T5 = char, typename T6 = char, typename T7 = char,
          typename T8 = char, typename T9 = char, typename T10 = char>
class AlignerImpl {
  T1 t1; T2 t2; T3 t3; T4 t4; T5 t5; T6 t6; T7 t7; T8 t8; T9 t9; T10 t10;

  AlignerImpl() = delete;
};

template <typename T1,
          typename T2 = char, typename T3 = char, typename T4 = char,
          typename T5 = char, typename T6 = char, typename T7 = char,
          typename T8 = char, typename T9 = char, typename T10 = char>
union SizerImpl {
  char arr1[sizeof(T1)], arr2[sizeof(T2)], arr3[sizeof(T3)], arr4[sizeof(T4)],
       arr5[sizeof(T5)], arr6[sizeof(T6)], arr7[sizeof(T7)], arr8[sizeof(T8)],
       arr9[sizeof(T9)], arr10[sizeof(T10)];
};
} // end namespace detail

template <typename T1,
          typename T2 = char, typename T3 = char, typename T4 = char,
          typename T5 = char, typename T6 = char, typename T7 = char,
          typename T8 = char, typename T9 = char, typename T10 = char>
struct AlignedCharArrayUnion : AlignedCharArray<
    alignof(detail::AlignerImpl<T1, T2, T3, T4, T5,
                                      T6, T7, T8, T9, T10>),
    sizeof(::detail::SizerImpl<T1, T2, T3, T4, T5,
                                     T6, T7, T8, T9, T10>)> {
};

