// json.h — JSON mínimo (leer y escribir) para el guardado y las pruebas.
// Sin excepciones: si el texto está mal, parse() devuelve false. Sólo se usa
// fuera de la partida (cargar/guardar), así que puede reservar memoria.
#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace ps {

class Json {
 public:
  enum Kind : uint8_t { NUL, BOOL, NUM, STR, ARR, OBJ };

  Json() = default;
  static Json num(double v) { Json j; j.k_ = NUM; j.n_ = v; return j; }
  static Json boolean(bool v) { Json j; j.k_ = BOOL; j.n_ = v ? 1 : 0; return j; }
  static Json str(std::string s) { Json j; j.k_ = STR; j.s_ = std::move(s); return j; }
  static Json array() { Json j; j.k_ = ARR; return j; }
  static Json object() { Json j; j.k_ = OBJ; return j; }

  Kind kind() const { return k_; }
  bool is_null() const { return k_ == NUL; }
  bool is_num() const { return k_ == NUM; }
  bool is_str() const { return k_ == STR; }
  bool is_arr() const { return k_ == ARR; }
  bool is_obj() const { return k_ == OBJ; }
  bool is_bool() const { return k_ == BOOL; }

  double as_num(double def = 0) const { return k_ == NUM || k_ == BOOL ? n_ : def; }
  int64_t as_int(int64_t def = 0) const { return k_ == NUM || k_ == BOOL ? int64_t(n_) : def; }
  bool as_bool(bool def = false) const { return k_ == BOOL || k_ == NUM ? n_ != 0 : def; }
  const std::string& as_str() const { return s_; }

  // Arrays
  size_t size() const { return k_ == ARR ? a_.size() : k_ == OBJ ? o_.size() : 0; }
  const Json& operator[](size_t i) const { return (k_ == ARR && i < a_.size()) ? a_[i] : null_(); }
  const Json& operator[](int i) const { return i < 0 ? null_() : (*this)[size_t(i)]; }
  Json& push(Json v) { a_.push_back(std::move(v)); return a_.back(); }
  // Objetos (se conserva el orden de las claves)
  const Json& operator[](const char* key) const;
  Json& set(const std::string& key, Json v);
  bool has(const char* key) const;
  const std::vector<std::pair<std::string, Json>>& items() const { return o_; }

  bool parse(const char* text, size_t len);
  bool parse(const std::string& s) { return parse(s.data(), s.size()); }
  std::string dump() const;

 private:
  static const Json& null_();
  bool parse_value(const char*& p, const char* end, int depth);
  void dump_to(std::string& out) const;

  Kind k_ = NUL;
  double n_ = 0;
  std::string s_;
  std::vector<Json> a_;
  std::vector<std::pair<std::string, Json>> o_;
};

}  // namespace ps
