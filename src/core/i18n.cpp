// i18n.cpp — ver i18n.h (core() de i18n.js, sin expresiones regulares).
#include "core/i18n.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/gamedata.h"

namespace ps {

namespace {
Lang g_lang = LANG_ES;

struct Pattern { std::vector<std::string> parts, low; const char* en; };
struct Dict {
  bool built = false;
  std::unordered_map<std::string, const char*> exact, exact_low;
  std::vector<Pattern> patterns;
  // Trozos: clave en minúsculas → traducción, agrupados por el primer byte.
  std::vector<std::pair<std::string, const char*>> frags[256];
  std::unordered_map<std::string, std::string> cache;
  // Lo que ya es una traducción: no se vuelve a traducir (el código traduce un
  // texto con tr() y luego el dibujo lo pasa otra vez por tr()).
  std::unordered_set<std::string> outs;
};
Dict& dict() { static Dict d; return d; }
bool g_record = false;
std::vector<std::pair<std::string, std::string>> g_miss;   // (sólo con tr_record)
// Traducciones en uso: las de fábrica o las de un paquete de datos (datapack).
const Tr* g_tr = kLangEn;
int g_ntr = kLangEnCount;

// Minúsculas / mayúsculas de ASCII y de las letras del español (UTF-8).
std::string lower(const std::string& s) {
  std::string o;
  o.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    unsigned char c = s[i];
    if (c >= 'A' && c <= 'Z') { o += char(c + 32); continue; }
    if (c == 0xC3 && i + 1 < s.size()) {
      unsigned char d = s[i + 1];
      if (d >= 0x80 && d <= 0x9E && d != 0x97) d = static_cast<unsigned char>(d + 0x20);   // Á→á, Ñ→ñ, Ü→ü...
      o += char(c); o += char(d); ++i; continue;
    }
    o += char(c);
  }
  return o;
}
std::string upper(const std::string& s) {
  std::string o;
  o.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    unsigned char c = s[i];
    if (c >= 'a' && c <= 'z') { o += char(c - 32); continue; }
    if (c == 0xC3 && i + 1 < s.size()) {
      unsigned char d = s[i + 1];
      if (d >= 0xA0 && d <= 0xBE && d != 0xB7) d = static_cast<unsigned char>(d - 0x20);
      o += char(c); o += char(d); ++i; continue;
    }
    o += char(c);
  }
  return o;
}
bool has_letter(const std::string& s) {
  for (unsigned char c : s) if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c >= 0xC0) return true;
  return false;
}
bool is_mon_name(const std::string& s) {
  for (int i = 0; i < kDexCount; ++i) if (s == kDex[i].name) return true;
  return false;
}
bool is_upper(const std::string& s) { return has_letter(s) && upper(s) == s; }
// ¿Es un carácter de "palabra" (letra o número) el byte de esa posición?
bool word_byte(unsigned char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c >= 0x80; }

void build() {
  Dict& d = dict();
  if (d.built) return;
  d.built = true;
  std::vector<std::pair<std::string, const char*>> fr;
  // Primero las del port (mandan: emplace no pisa) y luego las de la web.
  for (int i = 0; i < kLangEnPortCount + g_ntr; ++i) {
    const Tr& t = i < kLangEnPortCount ? kLangEnPort[i] : g_tr[i - kLangEnPortCount];
    const std::string es = t.es;
    const char* en = t.en;
    const size_t at = es.find("{x}");
    if (at != std::string::npos) {
      Pattern p;
      size_t start = 0, pos;
      while ((pos = es.find("{x}", start)) != std::string::npos) { p.parts.push_back(es.substr(start, pos - start)); start = pos + 3; }
      p.parts.push_back(es.substr(start));
      for (auto& x : p.parts) p.low.push_back(lower(x));
      p.en = en;
      d.patterns.push_back(std::move(p));
    } else {
      d.exact.emplace(es, en);
      d.exact_low.emplace(lower(es), en);
      if (es.size() >= 4 || es.find(' ') != std::string::npos) fr.emplace_back(lower(es), en);
    }
  }
  // Los más largos primero (como la expresión de la web).
  std::stable_sort(fr.begin(), fr.end(), [](const auto& a, const auto& b) { return a.first.size() > b.first.size(); });
  for (auto& f : fr) d.frags[uint8_t(f.first[0])].push_back(f);
}

std::string keep_case(const std::string& src, const std::string& en) { return is_upper(src) ? upper(en) : en; }

std::string core(const std::string& s, TrKind* kind = nullptr);

// Patrón con huecos: los huecos son "(.+?)" anclados al principio y al final.
bool match_parts(const std::string& body, const std::vector<std::string>& parts, size_t k, size_t pos, std::vector<std::string>& caps) {
  if (k == parts.size() - 1) {
    const std::string& last = parts[k];
    if (body.size() < pos + 1 + last.size()) return false;           // el hueco anterior no puede ir vacío
    if (body.compare(body.size() - last.size(), last.size(), last) != 0) return false;
    caps.push_back(body.substr(pos, body.size() - last.size() - pos));
    return true;
  }
  const std::string& next = parts[k];
  for (size_t p = pos + 1; p + next.size() <= body.size(); ++p) {     // el hueco, lo más corto posible
    if (body.compare(p, next.size(), next) != 0) continue;
    caps.push_back(body.substr(pos, p - pos));
    if (match_parts(body, parts, k + 1, p + next.size(), caps)) return true;
    caps.pop_back();
  }
  return false;
}

bool try_pattern(const Pattern& p, const std::string& body, bool up, std::string& out) {
  const std::string& b = up ? lower(body) : body;
  const auto& parts = up ? p.low : p.parts;
  if (b.compare(0, parts[0].size(), parts[0]) != 0) return false;
  std::vector<std::string> caps;
  if (!match_parts(b, parts, 1, parts[0].size(), caps)) return false;
  // Con mayúsculas se compara en minúsculas: los huecos se sacan del original.
  if (up) {
    // (las partes fijas miden lo mismo en minúsculas)
    size_t pos = parts[0].size();
    for (size_t k = 0; k < caps.size(); ++k) { caps[k] = body.substr(pos, caps[k].size()); pos += caps[k].size() + parts[k + 1].size(); }
  }
  std::string r;
  size_t ci = 0;
  for (const char* e = p.en; *e; ++e) {
    if (e[0] == '{' && e[1] == 'x' && e[2] == '}') { if (ci < caps.size()) r += core(caps[ci++]); e += 2; }
    else r += *e;
  }
  out = r;
  return true;
}

std::string core(const std::string& s, TrKind* kind) {
  Dict& d = dict();
  TrKind dummy;
  if (!kind) kind = &dummy;
  *kind = TRK_NONE;
  // Respeta los espacios de alrededor.
  size_t a = 0, z = s.size();
  while (a < z && (s[a] == ' ' || s[a] == '\n')) ++a;
  while (z > a && (s[z - 1] == ' ' || s[z - 1] == '\n')) --z;
  const std::string lead = s.substr(0, a), tail = s.substr(z), body = s.substr(a, z - a);
  if (body.empty()) return s;
  const bool up = is_upper(body);
  auto it = d.exact.find(body);
  if (it != d.exact.end()) { *kind = TRK_EXACT; return lead + it->second + tail; }
  if (up) {
    auto il = d.exact_low.find(lower(body));
    if (il != d.exact_low.end()) { *kind = TRK_EXACT; return lead + keep_case(body, il->second) + tail; }
  }
  // Si casan varios patrones, el más concreto (más texto fijo); a igualdad, el
  // primero (los del port van antes).
  std::string r, best;
  size_t best_fixed = 0;
  bool found = false;
  for (const Pattern& p : d.patterns) {
    size_t fixed = 0;
    for (const auto& x : p.parts) fixed += x.size();
    if (found && fixed <= best_fixed) continue;
    if (try_pattern(p, body, up, r)) { best = r; best_fixed = fixed; found = true; }
  }
  if (found) { *kind = TRK_PATTERN; return lead + keep_case(body, best) + tail; }
  // Trozos: palabras enteras, los más largos primero.
  const std::string low = lower(body);
  std::string out;
  size_t i = 0;
  bool changed = false;
  while (i < body.size()) {
    const bool start_ok = i == 0 || !word_byte(uint8_t(body[i - 1])) || (uint8_t(body[i - 1]) >= 0x80 && uint8_t(body[i - 1]) < 0xC0 && false);
    bool hit = false;
    if (start_ok) {
      for (const auto& f : d.frags[uint8_t(low[i])]) {
        const size_t n = f.first.size();
        if (i + n > low.size() || low.compare(i, n, f.first) != 0) continue;
        if (i + n < body.size() && word_byte(uint8_t(body[i + n]))) continue;
        out += keep_case(body.substr(i, n), f.second);
        i += n;
        hit = changed = true;
        break;
      }
    }
    if (!hit) out += body[i++];
  }
  if (changed) *kind = TRK_FRAGS;
  return lead + (changed ? out : body) + tail;
}
}  // namespace

void i18n_set(Lang l) { g_lang = l; dict().cache.clear(); }
void i18n_set_table(const Tr* t, int n) { g_tr = t ? t : kLangEn; g_ntr = t ? n : kLangEnCount; dict() = Dict(); }
Lang i18n_lang() { return g_lang; }

const char* tr(const char* s) {
  if (g_lang == LANG_ES || !s || !*s) return s;
  const std::string key(s);
  if (!has_letter(key)) return s;
  build();
  Dict& d = dict();
  auto it = d.cache.find(key);
  if (it != d.cache.end()) return it->second.c_str();
  auto io = d.outs.find(key);
  if (io != d.outs.end()) return io->c_str();
  if (d.cache.size() > 4000) { d.cache.clear(); d.outs.clear(); }
  TrKind k = TRK_NONE;
  auto res = d.cache.emplace(key, core(key, &k));
  if (res.first->second != key) d.outs.insert(res.first->second);
  const bool bad = k == TRK_NONE || k == TRK_FRAGS || tr_looks_spanish(res.first->second.c_str());
  if (g_record && bad && !is_mon_name(key) && g_miss.size() < 3000) {
    bool seen = false;
    for (const auto& m : g_miss) if (m.first == key) { seen = true; break; }
    if (!seen) g_miss.emplace_back(key, res.first->second);
  }
  return res.first->second.c_str();
}

void tr_record(bool on) { g_record = on; }

bool tr_looks_spanish(const char* s) {
  std::string t = lower(s ? s : "");
  // (nombres que llevan tilde también en inglés)
  for (const char* ok : {"pokémon", "poké", "pokédex", "español", "flabébé"})
    for (size_t p; (p = t.find(ok)) != std::string::npos;) t.replace(p, std::strlen(ok), std::strlen(ok), ' ');   // (mismo largo: s y t casan)
  for (const char* acc : {"á", "é", "í", "ó", "ú", "ñ", "¿", "¡"}) if (t.find(acc) != std::string::npos) return true;
  static const char* const W[] = {"de", "del", "la", "las", "el", "los", "con", "y", "para", "que", "se", "al", "un", "una", "tu",
                                  "tus", "su", "sus", "sin", "es", "lo", "ya", "ni", "o", "en", "por", "cada", "hay", "muy", "le"};
  size_t i = 0;
  while (i < t.size()) {
    while (i < t.size() && !word_byte(uint8_t(t[i]))) ++i;
    size_t j = i;
    while (j < t.size() && word_byte(uint8_t(t[j]))) ++j;
    if (j > i) {
      const std::string w = t.substr(i, j - i);
      // («Y» y «O» en mayúscula son botones o letras de megas, no la y / o)
      const bool button = (w == "y" || w == "o") && s[i] != 'y' && s[i] != 'o';
      for (const char* x : W) if (w == x && !button) return true;
    }
    i = j;
  }
  return false;
}
void tr_misses(void (*fn)(const char*, const char*, void*), void* ud) {
  for (const auto& m : g_miss) fn(m.first.c_str(), m.second.c_str(), ud);
}

TrKind tr_kind(const char* s) {
  if (!s || !*s || !has_letter(s)) return TRK_EXACT;
  build();
  TrKind k = TRK_NONE;
  core(s, &k);
  return k;
}

}  // namespace ps
