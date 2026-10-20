// Paquete de datos: lo que la consola baja sola (datapack.h). Que el de la
// misma web no cambie nada, que se rechace lo que no vale y que un cambio de
// equilibrio se aplique de verdad.
#include <doctest/doctest.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "core/datapack.h"
#include "core/gamedata.h"
#include "core/i18n.h"
#include "core/json.h"

using namespace ps;

namespace {
std::string read_pack() {
  std::string s;
  if (FILE* f = std::fopen(PS_PAQUETE, "rb")) {
    char b[65536];
    size_t n;
    while ((n = std::fread(b, 1, sizeof(b), f)) > 0) s.append(b, n);
    std::fclose(f);
  }
  return s;
}
uint32_t fnv(const std::string& s) {
  uint32_t h = 2166136261u;
  for (unsigned char c : s) { h ^= c; h *= 16777619u; }
  return h;
}
// Rehace el paquete con otros datos (y su huella buena).
std::string repack(const std::string& text, const Json& datos) {
  Json p;
  p.parse(text);
  const std::string d = datos.dump();
  char head[256];
  std::snprintf(head, sizeof(head), "{\"formato\":1,\"esquema\":\"%s\",\"web\":\"prueba\",\"fecha\":\"2026-10-20\",\"huella\":%u,\"datos\":",
                p["esquema"].as_str().c_str(), fnv(d));
  return std::string(head) + d + "}";
}
// Todas las tablas que el paquete puede cambiar, en bytes (sin los punteros a textos).
std::string numbers() {
  std::string s;
  for (int i = 0; i < kDexCount; ++i) { const Mon& m = kDex[i]; s.append(reinterpret_cast<const char*>(&m.hp), sizeof(m.hp) * 2); s.append(reinterpret_cast<const char*>(&m.atk), sizeof(float) * 3 + 4); }
  for (int i = 0; i < kMoveCount; ++i) s.append(reinterpret_cast<const char*>(kMoves[i].v), sizeof(kMoves[i].v)), s.append(reinterpret_cast<const char*>(&kMoves[i].has), 4);
  s.append(reinterpret_cast<const char*>(kTypeChart), sizeof(kTypeChart));
  for (int i = 0; i < kTierCount; ++i) s.append(reinterpret_cast<const char*>(&kTiers[i]), sizeof(Tier));
  s.append(reinterpret_cast<const char*>(kGachaRates), sizeof(kGachaRates));
  for (int i = 0; i < UP_COUNT; ++i) { s += char(kUpgrades[i].max); s.append(reinterpret_cast<const char*>(&kUpgrades[i].grow), 4); }
  for (int i = 0; i < kMissionCount; ++i) s.append(reinterpret_cast<const char*>(kMissions[i].goals), sizeof(kMissions[i].goals));
  return s;
}
}  // namespace

TEST_CASE("paquete de datos: el de la misma web deja todo igual") {
  const std::string text = read_pack();
  REQUIRE(text.size() > 1000);
  const std::string before = numbers();
  const uint32_t hash = kDataHash;
  const std::string name = kMoves[0].name;
  std::string why;
  REQUIRE_MESSAGE(datapack_apply(text, why), why);
  CHECK(numbers() == before);
  CHECK(kDataHash == hash);           // (mismos datos, misma huella: el cooperativo los ve iguales)
  CHECK(std::string(kMoves[0].name) == name);
  CHECK(datapack_current().on);
  i18n_set(LANG_EN);
  CHECK(std::string(tr("Volver")) == "Back");
  i18n_set(LANG_ES);
  datapack_reset();
  CHECK(numbers() == before);
}

TEST_CASE("paquete de datos: se rechaza lo que no vale (y no se toca nada)") {
  const std::string text = read_pack();
  const std::string before = numbers();
  std::string why;
  // Cortado.
  CHECK(!datapack_apply(text.substr(0, text.size() / 2), why));
  // Un byte cambiado: la huella no cuadra.
  std::string bad = text;
  const size_t at = bad.find("Placaje");
  REQUIRE(at != std::string::npos);
  bad[at] = 'X';
  CHECK(!datapack_apply(bad, why));
  CHECK(why.find("huella") != std::string::npos);
  // Para otra versión del programa.
  std::string other = text;
  const size_t es = other.find("\"esquema\":\"") + 11;
  other[es] = other[es] == '0' ? '1' : '0';
  CHECK(!datapack_apply(other, why));
  CHECK(why.find("otra versión") != std::string::npos);
  // Bien hecho pero con números absurdos (recarga 0, rareza 9...).
  Json p;
  REQUIRE(p.parse(text));
  Json d = p["datos"];
  d.ref("movs").elems()[0].elems()[2].elems()[MS_CD] = Json::num(0);
  CHECK(!datapack_apply(repack(text, d), why));
  d = p["datos"];
  d.ref("dex").elems()[0].elems()[8] = Json::num(9);
  CHECK(!datapack_apply(repack(text, d), why));
  d = p["datos"];
  d.ref("movs").elems().pop_back();   // (un movimiento de menos: no es la misma forma)
  CHECK(!datapack_apply(repack(text, d), why));
  CHECK(numbers() == before);
  CHECK(!datapack_current().on);
}

TEST_CASE("paquete de datos: un cambio de equilibrio se aplica") {
  const std::string text = read_pack();
  Json p;
  REQUIRE(p.parse(text));
  Json d = p["datos"];
  const int mi = move_index("thunder-shock");
  REQUIRE(mi >= 0);
  const float dmg0 = kMoves[mi].v[MS_DMG];
  d.ref("movs").elems()[size_t(mi)].elems()[2].elems()[MS_DMG] = Json::num(99);
  d.ref("movs").elems()[size_t(mi)].elems()[0] = Json::str("Impactrueno Plus");
  d.ref("mejoras").elems()[0].elems()[3] = Json::num(321);
  std::string why;
  const uint32_t h0 = kDataHash;
  REQUIRE_MESSAGE(datapack_apply(repack(text, d), why), why);
  CHECK(kMoves[mi].v[MS_DMG] == 99.0f);
  CHECK(std::string(kMoves[mi].name) == "Impactrueno Plus");
  CHECK(kUpgrades[0].base == 321);
  CHECK(kDataHash != h0);             // (otros datos: no se juntan en el cooperativo con quien no los tenga)
  CHECK(datapack_current().web == "prueba");
  datapack_reset();
  CHECK(kMoves[mi].v[MS_DMG] == dmg0);
  CHECK(kDataHash == h0);
}
