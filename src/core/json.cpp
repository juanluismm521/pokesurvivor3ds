// json.cpp
#include "core/json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ps {

const Json& Json::null_() { static const Json n; return n; }

const Json& Json::operator[](const char* key) const {
  if (k_ != OBJ) return null_();
  for (const auto& kv : o_) if (kv.first == key) return kv.second;
  return null_();
}

bool Json::has(const char* key) const {
  if (k_ != OBJ) return false;
  for (const auto& kv : o_) if (kv.first == key) return true;
  return false;
}

Json* Json::get(const std::string& key) {
  if (k_ != OBJ) return nullptr;
  for (auto& kv : o_) if (kv.first == key) return &kv.second;
  return nullptr;
}

Json& Json::ref(const std::string& key) {
  if (k_ != OBJ) { *this = object(); }
  if (Json* v = get(key)) return *v;
  o_.emplace_back(key, Json());
  return o_.back().second;
}

bool Json::remove(const std::string& key) {
  if (k_ != OBJ) return false;
  for (size_t i = 0; i < o_.size(); ++i) if (o_[i].first == key) { o_.erase(o_.begin() + long(i)); return true; }
  return false;
}

Json& Json::set(const std::string& key, Json v) {
  if (k_ != OBJ) *this = object();   // (null → objeto, como `x = x || {}`)
  for (auto& kv : o_) if (kv.first == key) { kv.second = std::move(v); return kv.second; }
  o_.emplace_back(key, std::move(v));
  return o_.back().second;
}

namespace {
void skip_ws(const char*& p, const char* end) { while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p; }

void put_utf8(std::string& s, uint32_t c) {
  if (c < 0x80) s += char(c);
  else if (c < 0x800) { s += char(0xC0 | (c >> 6)); s += char(0x80 | (c & 0x3F)); }
  else if (c < 0x10000) { s += char(0xE0 | (c >> 12)); s += char(0x80 | ((c >> 6) & 0x3F)); s += char(0x80 | (c & 0x3F)); }
  else { s += char(0xF0 | (c >> 18)); s += char(0x80 | ((c >> 12) & 0x3F)); s += char(0x80 | ((c >> 6) & 0x3F)); s += char(0x80 | (c & 0x3F)); }
}

bool parse_string(const char*& p, const char* end, std::string& out) {
  if (p >= end || *p != '"') return false;
  ++p;
  while (p < end && *p != '"') {
    if (*p == '\\') {
      if (++p >= end) return false;
      switch (*p) {
        case '"': out += '"'; break;
        case '\\': out += '\\'; break;
        case '/': out += '/'; break;
        case 'b': out += '\b'; break;
        case 'f': out += '\f'; break;
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        case 't': out += '\t'; break;
        case 'u': {
          if (end - p < 5) return false;
          char hex[5] = {p[1], p[2], p[3], p[4], 0};
          uint32_t c = uint32_t(std::strtoul(hex, nullptr, 16));
          p += 4;
          if (c >= 0xD800 && c < 0xDC00 && end - p >= 7 && p[1] == '\\' && p[2] == 'u') {
            char h2[5] = {p[3], p[4], p[5], p[6], 0};
            const uint32_t lo = uint32_t(std::strtoul(h2, nullptr, 16));
            if (lo >= 0xDC00 && lo < 0xE000) { c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00); p += 6; }
          }
          put_utf8(out, c);
          break;
        }
        default: return false;
      }
      ++p;
    } else {
      out += *p++;
    }
  }
  if (p >= end) return false;
  ++p;
  return true;
}
}  // namespace

bool Json::parse_value(const char*& p, const char* end, int depth) {
  if (depth > 64) return false;
  skip_ws(p, end);
  if (p >= end) return false;
  const char c = *p;
  if (c == '{') {
    *this = object();
    ++p;
    skip_ws(p, end);
    if (p < end && *p == '}') { ++p; return true; }
    while (p < end) {
      skip_ws(p, end);
      std::string key;
      if (!parse_string(p, end, key)) return false;
      skip_ws(p, end);
      if (p >= end || *p != ':') return false;
      ++p;
      Json v;
      if (!v.parse_value(p, end, depth + 1)) return false;
      o_.emplace_back(std::move(key), std::move(v));
      skip_ws(p, end);
      if (p < end && *p == ',') { ++p; continue; }
      if (p < end && *p == '}') { ++p; return true; }
      return false;
    }
    return false;
  }
  if (c == '[') {
    *this = array();
    ++p;
    skip_ws(p, end);
    if (p < end && *p == ']') { ++p; return true; }
    while (p < end) {
      Json v;
      if (!v.parse_value(p, end, depth + 1)) return false;
      a_.push_back(std::move(v));
      skip_ws(p, end);
      if (p < end && *p == ',') { ++p; continue; }
      if (p < end && *p == ']') { ++p; return true; }
      return false;
    }
    return false;
  }
  if (c == '"') { k_ = STR; s_.clear(); return parse_string(p, end, s_); }
  if (end - p >= 4 && !std::strncmp(p, "true", 4)) { k_ = BOOL; n_ = 1; p += 4; return true; }
  if (end - p >= 5 && !std::strncmp(p, "false", 5)) { k_ = BOOL; n_ = 0; p += 5; return true; }
  if (end - p >= 4 && !std::strncmp(p, "null", 4)) { k_ = NUL; p += 4; return true; }
  if (c == '-' || (c >= '0' && c <= '9')) {
    char buf[64];
    int n = 0;
    while (p < end && n < 63 && (std::strchr("+-.eE", *p) || (*p >= '0' && *p <= '9'))) buf[n++] = *p++;
    buf[n] = 0;
    char* e = nullptr;
    n_ = std::strtod(buf, &e);
    k_ = NUM;
    return e && *e == 0;
  }
  return false;
}

bool Json::parse(const char* text, size_t len) {
  const char* p = text;
  const char* end = text + len;
  *this = Json();
  if (!parse_value(p, end, 0)) { *this = Json(); return false; }
  skip_ws(p, end);
  return p == end;
}

void Json::dump_to(std::string& out) const {
  switch (k_) {
    case NUL: out += "null"; break;
    case BOOL: out += n_ != 0 ? "true" : "false"; break;
    case NUM: {
      char b[32];
      if (std::isfinite(n_) && n_ == std::floor(n_) && std::fabs(n_) < 9e15) std::snprintf(b, sizeof(b), "%lld", (long long)n_);
      else if (std::isfinite(n_)) std::snprintf(b, sizeof(b), "%.17g", n_);
      else std::snprintf(b, sizeof(b), "null");
      out += b;
      break;
    }
    case STR: {
      out += '"';
      for (const char ch : s_) {
        switch (ch) {
          case '"': out += "\\\""; break;
          case '\\': out += "\\\\"; break;
          case '\n': out += "\\n"; break;
          case '\r': out += "\\r"; break;
          case '\t': out += "\\t"; break;
          default:
            if (uint8_t(ch) < 0x20) { char b[8]; std::snprintf(b, sizeof(b), "\\u%04x", ch); out += b; }
            else out += ch;
        }
      }
      out += '"';
      break;
    }
    case ARR:
      out += '[';
      for (size_t i = 0; i < a_.size(); ++i) { if (i) out += ','; a_[i].dump_to(out); }
      out += ']';
      break;
    case OBJ:
      out += '{';
      for (size_t i = 0; i < o_.size(); ++i) {
        if (i) out += ',';
        Json::str(o_[i].first).dump_to(out);
        out += ':';
        o_[i].second.dump_to(out);
      }
      out += '}';
      break;
  }
}

std::string Json::dump() const { std::string s; dump_to(s); return s; }

}  // namespace ps
