// Regalos firmados (core/gift.h): sólo con la clave del proyecto, sólo a quien van, una
// sola vez (también tras guardar y juntar con la nube) y con todo lo que llevan.
#include <doctest/doctest.h>

#include <cstdio>
#include <memory>
#include <string>

#include "core/gift.h"
#include "core/json.h"
#include "core/monocypher.h"
#include "core/save.h"
#include "core/save_io.h"
#include "core/websave.h"

using namespace ps;

namespace {
std::string hx(const uint8_t* b, size_t n) {
  static const char H[] = "0123456789abcdef";
  std::string s;
  for (size_t i = 0; i < n; ++i) { s += H[b[i] >> 4]; s += H[b[i] & 15]; }
  return s;
}
// Un regalo como lo escribe tools/regalo.py.
std::string gift_json(const uint8_t sk[64], const Gift& g, const char* tamper = nullptr) {
  const std::string m = gift_message(g);
  uint8_t sig[64];
  crypto_eddsa_sign(sig, sk, reinterpret_cast<const uint8_t*>(m.data()), m.size());
  std::string sh = "[";
  for (int i = 0; i < g.nshiny; ++i) sh += (i ? "," : "") + std::to_string(g.shiny[i]);
  sh += "]";
  char b[1024];
  std::snprintf(b, sizeof(b), "{\"id\":\"%s\",\"para\":\"%s\",\"nota\":\"%s\",\"monedas\":%d,\"tickets\":%d,\"tickets10\":%d,\"caramelos\":%d,\"medalla\":%d,\"shinies\":%s,\"firma\":\"%s\"}",
                g.id.c_str(), g.para.c_str(), g.nota.c_str(), tamper ? g.coins + 1 : g.coins, g.t1, g.t10, g.candy, g.medal, sh.c_str(), hx(sig, 64).c_str());
  return b;
}
Gift lost_win() {   // (lo del dueño, 02-11-2026)
  Gift g;
  g.id = "2026-11-02-victoria"; g.para = "juanluismm520"; g.nota = "Premios de la victoria perdida";
  g.coins = 12898; g.t1 = 61; g.t10 = 10; g.candy = 88; g.medal = 94;
  g.shiny[0] = 192; g.nshiny = 1;
  return g;
}
}  // namespace

TEST_CASE("regalos: firma, a quién van y una sola vez") {
  uint8_t seed[32], sk[64], pk[32], seed2[32], sk2[64], pk2[32];
  for (int i = 0; i < 32; ++i) { seed[i] = uint8_t(i * 5 + 2); seed2[i] = uint8_t(i * 9 + 1); }
  crypto_eddsa_key_pair(sk, pk, seed);
  crypto_eddsa_key_pair(sk2, pk2, seed2);
  const Gift g = lost_win();
  Gift other = g; other.id = "otro"; other.para = "alguien";
  const std::string file = "{\"regalos\":[" + gift_json(sk, g) + "," + gift_json(sk2, other) + "," + gift_json(sk, other, "retocado") + "," + gift_json(sk, other) + "]}";
  Gift got[kMaxGiftsFile];
  const int n = gift_parse(file, pk, got, kMaxGiftsFile);
  REQUIRE(n == 2);   // (el de otra clave y el retocado, fuera)
  CHECK(got[0].id == g.id);
  CHECK(got[0].coins == 12898); CHECK(got[0].t1 == 61); CHECK(got[0].t10 == 10); CHECK(got[0].candy == 88);
  CHECK(got[0].medal == 94); CHECK(got[0].nshiny == 1); CHECK(got[0].shiny[0] == 192);
  CHECK(gift_parse("basura", pk, got, kMaxGiftsFile) == 0);
  // A quién: por el nombre del jugador o el de la cuenta, sin mayúsculas.
  CHECK(gift_for(got[0], "JuanLuisMM520", ""));
  CHECK(gift_for(got[0], "Jugador", "juanluismm520"));
  CHECK_FALSE(gift_for(got[0], "juanluismm52", "otra"));
  CHECK_FALSE(gift_for(got[0], "", nullptr));
  Gift all = got[0]; all.para = "*";
  CHECK(gift_for(all, "cualquiera", ""));   // (para todos)
  // Se da una vez, con todo.
  std::unique_ptr<Save> s(new Save());
  new_save(*s, "juanluismm520", 1000);
  const int c0 = s->coins;
  REQUIRE(gift_apply(*s, got[0], 5000));
  CHECK(s->coins == c0 + 12898);
  CHECK(s->t1 == 61); CHECK(s->t10 == 10); CHECK(s->candy == 88);
  CHECK(s->arceus_win[94] == 5000);
  CHECK(s->shiny_at[192] != 0); CHECK(s->owned_at[192] != 0);
  CHECK_FALSE(gift_apply(*s, got[0], 6000));
  CHECK(s->coins == c0 + 12898);
  // Guardado y vuelto a leer: sigue apuntado, no se repite.
  std::unique_ptr<Save> b(new Save());
  REQUIRE(save_from_json(*b, save_to_json(*s)));
  CHECK(gift_received(*b, g.id));
  CHECK_FALSE(gift_apply(*b, got[0], 7000));
  CHECK(b->coins == s->coins);
}

TEST_CASE("regalos: al juntar con la nube se conservan (monedas por la libreta, y el apunte)") {
  uint8_t seed[32], sk[64], pk[32];
  for (int i = 0; i < 32; ++i) seed[i] = uint8_t(i * 5 + 2);
  crypto_eddsa_key_pair(sk, pk, seed);
  // La nube: la partida de antes del regalo (como la tenía la web).
  std::unique_ptr<Save> s(new Save());
  new_save(*s, "juanluismm520", 1000);
  Json cloud = web::migrate(save_full_json(*s), "", 2000, false);
  web::fold(cloud, "3ds");
  // En la consola: el regalo.
  Gift got[kMaxGiftsFile];
  REQUIRE(gift_parse("{\"regalos\":[" + gift_json(sk, lost_win()) + "]}", pk, got, kMaxGiftsFile) == 1);
  REQUIRE(gift_apply(*s, got[0], 3000));
  s->updated = 3000;
  Json mine = web::migrate(save_full_json(*s), "", 4000, false);
  web::fold(mine, "3ds");
  const Json merged = web::merge_safe(mine, cloud, 4000);
  std::unique_ptr<Save> out(new Save());
  REQUIRE(save_from_json(*out, merged.dump()));
  CHECK(out->coins == s->coins);
  CHECK(out->t1 == 61); CHECK(out->t10 == 10); CHECK(out->candy == 88);
  CHECK(out->arceus_win[94] > 0);
  CHECK(out->shiny_at[192] != 0);
  CHECK(gift_received(*out, "2026-11-02-victoria"));
  // Y al revés (la nube más nueva, sin el apunte): tampoco se pierde.
  Json cloud2 = cloud; cloud2.set("updated", Json::num(9999));
  const Json merged2 = web::merge_safe(cloud2, mine, 10000);
  std::unique_ptr<Save> out2(new Save());
  REQUIRE(save_from_json(*out2, merged2.dump()));
  CHECK(gift_received(*out2, "2026-11-02-victoria"));
  CHECK(out2->coins == s->coins);
}
