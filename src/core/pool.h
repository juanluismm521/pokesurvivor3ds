// pool.h — grupos de tamaño fijo (sin memoria dinámica en la partida).
#pragma once
#include <cstdint>

namespace ps {

// Lista compacta de capacidad fija. Quitar un elemento mueve el último a su
// hueco (el orden no se conserva) salvo con remove_ordered.
template <class T, int N>
struct FixedList {
  T items[N];
  int n = 0;

  static constexpr int capacity() { return N; }
  bool full() const { return n >= N; }
  void clear() { n = 0; }
  T* push() { return n < N ? &items[n++] : nullptr; }
  // (un índice que ya no está —la lista se vació mientras se recorría— no hace nada:
  // nunca deja n por debajo de 0; ver judgment_hit_arceus, 01-11-2026)
  void remove_swap(int i) { if (i < 0 || i >= n) return; items[i] = items[--n]; }
  void remove_ordered(int i) { if (i < 0 || i >= n) return; for (int k = i; k < n - 1; ++k) items[k] = items[k + 1]; --n; }
  T& operator[](int i) { return items[i]; }
  const T& operator[](int i) const { return items[i]; }
  T* begin() { return items; }
  T* end() { return items + n; }
  const T* begin() const { return items; }
  const T* end() const { return items + n; }
};

}  // namespace ps
