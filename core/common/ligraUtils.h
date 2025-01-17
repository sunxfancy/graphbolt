// This code is part of the project "Ligra: A Lightweight Graph Processing
// Framework for Shared Memory", presented at Principles and Practice of
// Parallel Programming, 2013.
// Copyright (c) 2013 Julian Shun and Guy Blelloch
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights (to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef LIGRA_UTIL_H
#define LIGRA_UTIL_H
#include "math.h"
#include "parallel.h"
#include <fstream>
#include <iostream>
#include <stdlib.h>
using namespace std;

// Needed to make frequent large allocations efficient with standard
// malloc implementation.  Otherwise they are allocated directly from
// vm.
#if !defined __APPLE__
#include <malloc.h>
// comment out the following two lines if running out of memory
static int __ii = mallopt(M_MMAP_MAX, 0);
static int __jj = mallopt(M_TRIM_THRESHOLD, -1);
#endif

typedef unsigned int uint;
typedef unsigned long ulong;

#define newAWithZero(__E, __n) (__E *)calloc(__n, sizeof(__E))
#define newA(__E, __n) (__E *)malloc((__n) * sizeof(__E))
#define deleteA(__E) free(__E)
#define renewA(__E, __Array, __n) (__E *)realloc(__Array, (__n) * sizeof(__E))

template <class E> struct identityF {
  E operator()(const E &x) { return x; }
};

template <class E> struct addF {
  E operator()(const E &a, const E &b) const { return a + b; }
};

template <class E> struct ascendingF {
  E operator()(const E &a, const E &b) const { return (a > b); }
};

template <class E> struct minF {
  E operator()(const E &a, const E &b) const { return (a < b) ? a : b; }
};

template <class E> struct maxF {
  E operator()(const E &a, const E &b) const { return (a > b) ? a : b; }
};

struct nonMaxF {
  bool operator()(uintE &a) { return (a != UINT_E_MAX); }
};

void setCustomWorkers(int n_workers) {
  if (n_workers == getWorkers()) {
    return;
  }
  setWorkers(n_workers);
  cout << "Number of workers : " << getWorkers() << endl;
}

#define _SCAN_LOG_BSIZE 10
#define _SCAN_BSIZE (1 << _SCAN_LOG_BSIZE)

template <class T> struct _seq {
  T *A;
  long n;
  _seq() {
    A = NULL;
    n = 0;
  }
  _seq(T *_A, long _n) : A(_A), n(_n) {}
  void del() { free(A); }
};

namespace sequence {
template <class intT> struct boolGetA {
  bool *A;
  boolGetA(bool *AA) : A(AA) {}
  intT operator()(intT i) { return (intT)A[i]; }
};

template <class ET, class intT> struct getA {
  ET *A;
  getA(ET *AA) : A(AA) {}
  ET operator()(intT i) { return A[i]; }
};

template <class IT, class OT, class intT, class F> struct getAF {
  IT *A;
  F f;
  getAF(IT *AA, F ff) : A(AA), f(ff) {}
  OT operator()(intT i) { return f(A[i]); }
};

template <class vertex, class intT> struct getDegree {
  vertex *V;
  bool *activeV;
  getDegree(vertex *VV, bool *activeVV) : V(VV), activeV(activeVV) {}
  inline intT operator()(intT i) {
    return (activeV[i]) ? (intT)V[i].getOutDegree() : 0;
  }
};

#define nblocks(_n, _bsize) (1 + ((_n)-1) / (_bsize))

#define blocked_for(_i, _s, _e, _bsize, _body)                                 \
  {                                                                            \
    long _ss = _s;                                                             \
    long _ee = _e;                                                             \
    long _n = _ee - _ss;                                                       \
    long _l = nblocks(_n, _bsize);                                             \
    parallel_for(long _i = 0; _i < _l; _i++) {                                 \
      long _s = _ss + _i * (_bsize);                                           \
      long _e = min(_s + (_bsize), _ee);                                       \
      _body                                                                    \
    }                                                                          \
  }

#define blocked_for_withIncrement(_i, _s, _e, _bsize, _body, _incrementBy)     \
  {                                                                            \
    long _ss = _s;                                                             \
    long _ee = _e;                                                             \
    long _n = _ee - _ss;                                                       \
    long _l = nblocks(_n, _bsize);                                             \
    parallel_for(long _i = 0; _i < _l; _i += _incrementBy) {                   \
      long _s = _ss + _i * (_bsize);                                           \
      long _e = min(_s + (_bsize), _ee);                                       \
      _body                                                                    \
    }                                                                          \
  }

template <class OT, class intT, class F, class G>
OT reduceSerial(intT s, intT e, F f, G g) {
  OT r = g(s);
  for (intT j = s + 1; j < e; j++)
    r = f(r, g(j));
  return r;
}

template <class OT, class intT, class F, class G>
OT reduce(intT s, intT e, F f, G g) {
  intT l = nblocks(e - s, _SCAN_BSIZE);
  if (l <= 1)
    return reduceSerial<OT>(s, e, f, g);
  OT *Sums = newA(OT, l);
  blocked_for(i, s, e, _SCAN_BSIZE, Sums[i] = reduceSerial<OT>(s, e, f, g););
  OT r = reduce<OT>((intT)0, l, f, getA<OT, intT>(Sums));
  free(Sums);
  return r;
}

template <class OT, class intT, class F> OT reduce(OT *A, intT n, F f) {
  return reduce<OT>((intT)0, n, f, getA<OT, intT>(A));
}

//================================
template <class OT, class intT, class F, class G, class H>
void applySerial(intT s, intT e, F applyF, G endCondition, H getter) {
  for (intT j = s; j < e; j++) {
    if (endCondition())
      return;
    applyF(getter(j), j);
  }
}

template <class OT, class intT, class F, class G, class H>
void apply(intT s, intT e, F applyF, G endCondition, H getter) {
  intT l = nblocks(e - s, _SCAN_BSIZE);
  if (l <= 1) {
    applySerial<OT>(s, e, applyF, endCondition, getter);
    return;
  }
  blocked_for(i, s, e, _SCAN_BSIZE,
              { applySerial<OT>(s, e, applyF, endCondition, getter); });
}

template <class OT, class intT, class F, class G>
void apply(OT *A, intT n, F applyF, G endCondition) {
  // auto getter = getA<OT, intT>(A);
  apply<OT>((intT)0, n, applyF, endCondition, getA<OT, intT>(A));
}

//================================
template <class OT, class intT, class F, class G, class H>
void applySerialWithIncrement(intT s, intT e, F applyF, G endCondition,
                              H getter, intT incrementBy) {
  for (intT j = s; j < e; j += incrementBy) {
    if (endCondition())
      return;
    applyF(getter(j), j);
  }
}

template <class OT, class intT, class F, class G, class H>
void applyWithIncrement(intT s, intT e, F applyF, G endCondition, H getter,
                        intT incrementBy) {
  intT l = nblocks(e - s, _SCAN_BSIZE);
  if (l <= 1) {
    applySerialWithIncrement<OT>(s, e, applyF, endCondition, getter,
                                 incrementBy);
    return;
  }
  blocked_for_withIncrement(
      i, s, e, _SCAN_BSIZE,
      { applySerialWithIncrement<OT>(s, e, applyF, endCondition, getter); },
      incrementBy);
}

template <class OT, class intT, class F, class G>
void applyWithIncrement(OT *A, intT n, F applyF, G endCondition,
                        intT incrementBy) {
  // auto getter = getA<OT, intT>(A);
  applyWithIncrement<OT>((intT)0, n, applyF, endCondition, getA<OT, intT>(A),
                         incrementBy);
}

//======================================

template <class OT, class intT> OT plusReduce(OT *A, intT n) {
  return reduce<OT>((intT)0, n, addF<OT>(), getA<OT, intT>(A));
}

template <class vertex, class intT>
intT plusReduceDegree(vertex *A, bool *activeV, intT n) {
  return reduce<intT>((intT)0, n, addF<intT>(),
                      getDegree<vertex, intT>(A, activeV));
}

// g is the map function (applied to each element)
// f is the reduce function
// need to specify OT since it is not an argument
template <class OT, class IT, class intT, class F, class G>
OT mapReduce(IT *A, intT n, F f, G g) {
  return reduce<OT>((intT)0, n, f, getAF<IT, OT, intT, G>(A, g));
}

template <class intT> intT sum(bool *In, intT n) {
  return reduce<intT>((intT)0, n, addF<intT>(), boolGetA<intT>(In));
}

template <class ET, class intT, class F, class G>
ET scanSerial(ET *Out, intT s, intT e, F f, G g, ET zero, bool inclusive,
              bool back) {
  ET r = zero;
  if (inclusive) {
    if (back) {
      intT i = e;
      while (i > s) {
        i--;
        Out[i] = r = f(r, g(i));
      }
    } else
      for (intT i = s; i < e; i++)
        Out[i] = r = f(r, g(i));
  } else {
    if (back) {
      intT i = e;
      while (i > s) {
        i--;
        ET t = g(i);
        Out[i] = r;
        r = f(r, t);
      }
    } else
      for (intT i = s; i < e; i++) {
        ET t = g(i);
        Out[i] = r;
        r = f(r, t);
      }
  }
  return r;
}

template <class ET, class intT, class F>
ET scanSerial(ET *In, ET *Out, intT n, F f, ET zero) {
  return scanSerial(Out, (intT)0, n, f, getA<ET, intT>(In), zero, false, false);
}

// back indicates it runs in reverse direction
template <class ET, class intT, class F, class G>
ET scan(ET *Out, intT s, intT e, F f, G g, ET zero, bool inclusive, bool back) {
  intT n = e - s;
  intT l = nblocks(n, _SCAN_BSIZE);
  if (l <= 2)
    return scanSerial(Out, s, e, f, g, zero, inclusive, back);
  ET *Sums = newA(ET, nblocks(n, _SCAN_BSIZE));
  blocked_for(i, s, e, _SCAN_BSIZE, Sums[i] = reduceSerial<ET>(s, e, f, g););
  ET total = scan(Sums, (intT)0, l, f, getA<ET, intT>(Sums), zero, false, back);
  blocked_for(i, s, e, _SCAN_BSIZE,
              scanSerial(Out, s, e, f, g, Sums[i], inclusive, back););
  free(Sums);
  return total;
}

template <class ET, class intT, class F>
ET scan(ET *In, ET *Out, intT n, F f, ET zero) {
  return scan(Out, (intT)0, n, f, getA<ET, intT>(In), zero, false, false);
}

template <class ET, class intT, class F>
ET scanI(ET *In, ET *Out, intT n, F f, ET zero) {
  return scan(Out, (intT)0, n, f, getA<ET, intT>(In), zero, true, false);
}

template <class ET, class intT, class F>
ET scanBack(ET *In, ET *Out, intT n, F f, ET zero) {
  return scan(Out, (intT)0, n, f, getA<ET, intT>(In), zero, false, true);
}

template <class ET, class intT, class F>
ET scanIBack(ET *In, ET *Out, intT n, F f, ET zero) {
  return scan(Out, (intT)0, n, f, getA<ET, intT>(In), zero, true, true);
}

template <class ET, class intT> ET plusScan(ET *In, ET *Out, intT n) {
  return scan(Out, (intT)0, n, addF<ET>(), getA<ET, intT>(In), (ET)0, false,
              false);
}

template <class ET, class intT> ET plusScanI(ET *In, ET *Out, intT n) {
  return scan(Out, (intT)0, n, addF<ET>(), getA<ET, intT>(In), (ET)0, true,
              false);
}

#define _F_BSIZE (2 * _SCAN_BSIZE)

// sums a sequence of n boolean flags
// an optimized version that sums blocks of 4 booleans by treating
// them as an integer
// Only optimized when n is a multiple of 512 and Fl is 4byte aligned
template <class intT> intT sumFlagsSerial(bool *Fl, intT n) {
  cout << "sumFlagsSerial FL = " << Fl << " n = " << n << endl;
  intT r = 0;
  if (n >= 128 && (n & 511) == 0 && ((long)Fl & 3) == 0) {
    int *IFl = (int *)Fl;
    for (int k = 0; k < (n >> 9); k++) {
      int rr = 0;
      for (int j = 0; j < 128; j++)
        rr += IFl[j];
      r += (rr & 255) + ((rr >> 8) & 255) + ((rr >> 16) & 255) +
           ((rr >> 24) & 255);
      IFl += 128;
    }
  } else
    for (intT j = 0; j < n; j++)
      r += Fl[j];
  return r;
}

template <class ET, class intT, class F>
_seq<ET> packSerial(ET *Out, bool *Fl, intT s, intT e, F f) {
  if (Out == NULL) {
    intT m = sumFlagsSerial(Fl + s, e - s);
    Out = newA(ET, m);
  }
  intT k = 0;
  for (intT i = s; i < e; i++)
    if (Fl[i])
      Out[k++] = f(i);
  return _seq<ET>(Out, k);
}

template <class ET, class intT, class F>
_seq<ET> pack(ET *Out, bool *Fl, intT s, intT e, F f) {
  intT l = nblocks(e - s, _F_BSIZE);
  if (l <= 1)
    return packSerial(Out, Fl, s, e, f);
  intT *Sums = newA(intT, l);
  blocked_for(i, s, e, _F_BSIZE, Sums[i] = sumFlagsSerial(Fl + s, e - s););
  for (intT i = 0; i < l; i++)
    cout << "Sums[" << i << "] = " << Sums[i] << endl;
  intT m = plusScan(Sums, Sums, l);
  cout << "m = " << m << endl;
  if (Out == NULL)
    Out = newA(ET, m);
  blocked_for(i, s, e, _F_BSIZE, packSerial(Out + Sums[i], Fl, s, e, f););
  free(Sums);
  return _seq<ET>(Out, m);
}

template <class ET, class intT> intT pack(ET *In, ET *Out, bool *Fl, intT n) {
  return pack(Out, Fl, (intT)0, n, getA<ET, intT>(In)).n;
}

template <class intT> _seq<intT> packIndex(bool *Fl, intT n) {
  return pack((intT *)NULL, Fl, (intT)0, n, identityF<intT>());
}

template <class ET, class intT, class PRED>
intT filter(ET *In, ET *Out, bool *Fl, intT n, PRED p) {
  parallel_for(intT i = 0; i < n; i++) Fl[i] = (bool)p(In[i]);
  intT m = pack(In, Out, Fl, n);
  return m;
}

template <class ET, class intT, class PRED>
intT filter(ET *In, ET *Out, intT n, PRED p) {
  bool *Fl = newA(bool, n);
  intT m = filter(In, Out, Fl, n, p);
  free(Fl);
  return m;
}

} // namespace sequence

template <class ET> inline bool CAS(ET *ptr, ET oldv, ET newv) {
  if (sizeof(ET) == 1) {
    return __sync_bool_compare_and_swap((bool *)ptr, *((bool *)&oldv),
                                        *((bool *)&newv));
  } else if (sizeof(ET) == 4) {
    return __sync_bool_compare_and_swap((int *)ptr, *((int *)&oldv),
                                        *((int *)&newv));
  } else if (sizeof(ET) == 8) {
    return __sync_bool_compare_and_swap((long *)ptr, *((long *)&oldv),
                                        *((long *)&newv));
  } else {
    std::cout << "CAS bad length : " << sizeof(ET) << std::endl;
    abort();
  }
}

template <class ET> inline bool writeMin(ET *a, ET b) {
  ET c;
  bool r = 0;
  do
    c = *a;
  while (c > b && !(r = CAS(a, c, b)));
  return r;
}

template <class ET> inline void writeAdd(ET *a, ET b) {
  volatile ET newV, oldV;
  do {
    oldV = *a;
    newV = oldV + b;
  } while (!CAS(a, oldV, newV));
}

template <class ET> inline void multiplyAndSave(ET *a, ET b) {
  volatile ET newV, oldV;
  do {
    oldV = *a;
    newV = oldV * b;
  } while (!CAS(a, oldV, newV));
}

template <class ET> inline void divideAndSave(ET *a, ET b) {
  volatile ET newV, oldV;
  do {
    oldV = *a;
    newV = oldV / b;
  } while (!CAS(a, oldV, newV));
}

inline uint hashInt(uint a) {
  a = (a + 0x7ed55d16) + (a << 12);
  a = (a ^ 0xc761c23c) ^ (a >> 19);
  a = (a + 0x165667b1) + (a << 5);
  a = (a + 0xd3a2646c) ^ (a << 9);
  a = (a + 0xfd7046c5) + (a << 3);
  a = (a ^ 0xb55a4f09) ^ (a >> 16);
  return a;
}

inline ulong hashInt(ulong a) {
  a = (a + 0x7ed55d166bef7a1d) + (a << 12);
  a = (a ^ 0xc761c23c510fa2dd) ^ (a >> 9);
  a = (a + 0x165667b183a9c0e1) + (a << 59);
  a = (a + 0xd3a2646cab3487e3) ^ (a << 49);
  a = (a + 0xfd7046c5ef9ab54c) + (a << 3);
  a = (a ^ 0xb55a4f090dd4a67b) ^ (a >> 32);
  return a;
}

// Remove duplicate integers in [0,...,n-1].
// Assumes that flags is already allocated and cleared to UINT_E_MAX.
// Sets all duplicate values in the array to UINT_E_MAX and resets flags to
// UINT_E_MAX.
template <class G>
void remDuplicates(G &get_key, uintE *flags, long m, long n) {
  parallel_for(size_t i = 0; i < m; i++) {
    uintE key = get_key(i);
    if (key != UINT_E_MAX && flags[key] == UINT_E_MAX) {
      CAS(&flags[key], (uintE)UINT_E_MAX, static_cast<uintE>(i));
    }
  }
  // reset flags
  parallel_for(size_t i = 0; i < m; i++) {
    uintE key = get_key(i);
    if (key != UINT_E_MAX) {
      if (flags[key] == i) {     // win
        flags[key] = UINT_E_MAX; // reset
      } else {
        get_key(i) = UINT_E_MAX; // lost
      }
    }
  }
}

#define granular_for(_i, _start, _end, _cond, _body)                           \
  {                                                                            \
    if (_cond) {                                                               \
      {                                                                        \
        parallel_for(long _i = _start; _i < _end; _i++) { _body }            \
      }                                                                        \
    } else {                                                                   \
      {                                                                        \
        for (long _i = _start; _i < _end; _i++) {                            \
          _body                                                                \
        }                                                                      \
      }                                                                        \
    }                                                                          \
  }

namespace pbbs {

struct empty {};

typedef uint32_t flags;
const flags no_flag = 0;
const flags fl_sequential = 1;
const flags fl_debug = 2;
const flags fl_time = 4;

template <typename T> inline void assign_uninitialized(T &a, const T &b) {
  new (static_cast<void *>(std::addressof(a))) T(b);
}

template <typename T> inline void move_uninitialized(T &a, const T &b) {
  new (static_cast<void *>(std::addressof(a))) T(std::move(b));
}

// a 32-bit hash function
uint32_t hash32(uint32_t a) {
  a = (a + 0x7ed55d16) + (a << 12);
  a = (a ^ 0xc761c23c) ^ (a >> 19);
  a = (a + 0x165667b1) + (a << 5);
  a = (a + 0xd3a2646c) ^ (a << 9);
  a = (a + 0xfd7046c5) + (a << 3);
  a = (a ^ 0xb55a4f09) ^ (a >> 16);
  return a;
}

// from numerical recipes
uint64_t hash64(uint64_t u) {
  uint64_t v = u * 3935559000370003845 + 2691343689449507681;
  v ^= v >> 21;
  v ^= v << 37;
  v ^= v >> 4;
  v *= 4768777513237032717;
  v ^= v << 20;
  v ^= v >> 41;
  v ^= v << 5;
  return v;
}

// Does not initialize the array
template <typename E> E *new_array_no_init(size_t n, bool touch_pages = false) {
  // pads in case user wants to allign with cache lines
  size_t line_size = 64;
  size_t bytes = ((n * sizeof(E)) / line_size + 1) * line_size;
#ifndef __APPLE__
  E *r = (E *)aligned_alloc(line_size, bytes);
#else
  E *r;
  if (posix_memalign((void **)&r, line_size, bytes) != 0) {
    fprintf(stderr, "Cannot allocate space");
    exit(1);
  }
#endif
  if (r == NULL) {
    fprintf(stderr, "Cannot allocate space");
    exit(1);
  }
  // a hack to make sure tlb is full for huge pages
  if (touch_pages)
    parallel_for(size_t i = 0; i < bytes; i = i + (1 << 21))((bool *)r)[i] = 0;
  return r;
}

// Initializes in parallel
template <typename E> E *new_array(size_t n) {
  E *r = new_array_no_init<E>(n);
  if (!std::is_trivially_default_constructible<E>::value) {
    if (n > 2048)
      parallel_for(size_t i = 0; i < n; i++) new ((void *)(r + i)) E;
    else
      for (size_t i = 0; i < n; i++)
        new ((void *)(r + i)) E;
  }
  return r;
}

// Destructs in parallel
template <typename E> void delete_array(E *A, size_t n) {
  // C++14 -- suppored by gnu C++11
  if (!std::is_trivially_destructible<E>::value) {
    if (n > 2048)
      parallel_for(size_t i = 0; i < n; i++) A[i].~E();
    else
      for (size_t i = 0; i < n; i++)
        A[i].~E();
  }
  free(A);
}

template <typename ET> inline bool CAS_GCC(ET *ptr, ET oldv, ET newv) {
  return __sync_bool_compare_and_swap(ptr, oldv, newv);
}

template <typename E, typename EV> inline E fetch_and_add(E *a, EV b) {
  volatile E newV, oldV;
  do {
    oldV = *a;
    newV = oldV + b;
  } while (!CAS_GCC(a, oldV, newV));
  return oldV;
}

template <typename E, typename EV> inline void write_add(E *a, EV b) {
  volatile E newV, oldV;
  do {
    oldV = *a;
    newV = oldV + b;
  } while (!CAS_GCC(a, oldV, newV));
}

template <typename ET, typename F> inline bool write_min(ET *a, ET b, F less) {
  ET c;
  bool r = 0;
  do
    c = *a;
  while (less(b, c) && !(r = CAS_GCC(a, c, b)));
  return r;
}

// returns the log base 2 rounded up (works on ints or longs or unsigned
// versions)
template <class T> static int log2_up(T i) {
  int a = 0;
  T b = i - 1;
  while (b > 0) {
    b = b >> 1;
    a++;
  }
  return a;
}

template <class T> T *copy_array(T *src, T *target, uintEE size) {
  memcpy(target, src, size);
}

template <class T> T *create_copy(intE n, T *ref) {
  T *temp = newA(T, n);
  parallel_for(int i = 0; i < n; i++) temp[i] = ref[i];
  // std::copy(ref, ref+n, temp);
  return temp;
}

template <class T> T **create_copy(intE n, intE s, T **ref) {
  T **temp = newA(T *, n);
  parallel_for(int i = 0; i < n; i++) {
    temp[i] = newA(T, s);
    for (int j = 0; j < s; j++) {
      temp[i][j] = ref[i][j];
    }
  }
  // std::copy(ref, ref+n, temp);
  return temp;
}

} // namespace pbbs

#endif
