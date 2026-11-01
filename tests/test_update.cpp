// El juego se actualiza solo: firmas, huellas y cuándo se instala (core/update).
#include <doctest/doctest.h>

#include <cstring>
#include <string>

#include "core/monocypher.h"
#include "core/update.h"

using namespace ps;

namespace {
std::string hx(const uint8_t* b, size_t n) {
  static const char H[] = "0123456789abcdef";
  std::string s;
  for (size_t i = 0; i < n; ++i) { s += H[b[i] >> 4]; s += H[b[i] & 15]; }
  return s;
}
// Una publicación firmada como lo hace el VPS (tools/publicar_programa.sh).
std::string entry_json(const uint8_t sk[64], int serie, int build, const char* url = "b10/pokesurvivor-3ds.3dsx") {
  UpdateEntry e;
  e.serie = serie; e.build = build; e.version = "0.1.0"; e.commit = "abc1234";
  e.f3dsx.size = 1000; e.cia.size = 2000;
  for (int i = 0; i < 32; ++i) { e.f3dsx.hash[i] = uint8_t(i); e.cia.hash[i] = uint8_t(255 - i); }
  const std::string m = update_message(e);
  uint8_t sig[64];
  crypto_eddsa_sign(sig, sk, reinterpret_cast<const uint8_t*>(m.data()), m.size());
  char b[1024];
  std::snprintf(b, sizeof(b),
                "{\"serie\":%d,\"build\":%d,\"version\":\"0.1.0\",\"commit\":\"abc1234\",\"fecha\":\"2026-10-21\",\"notas\":\"\","
                "\"3dsx\":{\"url\":\"%s\",\"tam\":1000,\"huella\":\"%s\"},\"cia\":{\"url\":\"b10/pokesurvivor-3ds.cia\",\"tam\":2000,\"huella\":\"%s\"},\"firma\":\"%s\"}",
                serie, build, url, hx(e.f3dsx.hash, 32).c_str(), hx(e.cia.hash, 32).c_str(), hx(sig, 64).c_str());
  return b;
}
}  // namespace

TEST_CASE("actualizar: la firma manda") {
  uint8_t seed[32], sk[64], pk[32], seed2[32], sk2[64], pk2[32];
  for (int i = 0; i < 32; ++i) { seed[i] = uint8_t(i * 7 + 1); seed2[i] = uint8_t(i * 3 + 5); }
  crypto_eddsa_key_pair(sk, pk, seed);
  crypto_eddsa_key_pair(sk2, pk2, seed2);
  UpdateEntry a, b;
  std::string why;
  const std::string good = "{\"actual\":" + entry_json(sk, 5, 130) + ",\"anterior\":" + entry_json(sk, 4, 120) + "}";
  REQUIRE_MESSAGE(update_parse(good, pk, a, b, why), why);
  CHECK(a.ok); CHECK(a.serie == 5); CHECK(a.build == 130); CHECK(a.f3dsx.size == 1000); CHECK(a.cia.url == "b10/pokesurvivor-3ds.cia");
  CHECK(b.ok); CHECK(b.build == 120);
  // Con otra clave: nada.
  CHECK(!update_parse(good, pk2, a, b, why));
  CHECK(why.find("firma") != std::string::npos);
  // Tocado (otra build u otro tamaño sin volver a firmar): nada.
  std::string bad = good;
  bad.replace(bad.find("\"build\":130"), 11, "\"build\":131");
  CHECK(!update_parse(bad, pk, a, b, why));
  bad = good;
  bad.replace(bad.find("\"tam\":1000"), 10, "\"tam\":1001");
  CHECK(!update_parse(bad, pk, a, b, why));
  // Firmado por otra clave: nada.
  CHECK(!update_parse("{\"actual\":" + entry_json(sk2, 5, 130) + "}", pk, a, b, why));
  // Rutas raras (salir de la carpeta): nada, aunque estén firmadas.
  CHECK(!update_parse("{\"actual\":" + entry_json(sk, 5, 130, "../../x.3dsx") + "}", pk, a, b, why));
  CHECK(!update_parse("{\"actual\":" + entry_json(sk, 5, 130, "sdmc:/x.3dsx") + "}", pk, a, b, why));
  // La anterior rota no estropea la actual.
  REQUIRE(update_parse("{\"actual\":" + entry_json(sk, 5, 130) + ",\"anterior\":" + entry_json(sk2, 4, 120) + "}", pk, a, b, why));
  CHECK(a.ok); CHECK(!b.ok);
  CHECK(!update_parse("basura", pk, a, b, why));
}

TEST_CASE("actualizar: cuándo se instala (y volver atrás no se deshace solo)") {
  UpdateEntry e;
  e.ok = true; e.serie = 7; e.build = 140;
  CHECK(update_wanted(e, 130, 0));     // build nueva
  CHECK(!update_wanted(e, 140, 0));    // ya la tienes
  CHECK(!update_wanted(e, 130, 7));    // volviste atrás desde ella: no se reinstala
  e.serie = 8; e.build = 120;          // el servidor vuelve atrás (serie nueva, build vieja)
  CHECK(update_wanted(e, 140, 7));
  e.ok = false;
  CHECK(!update_wanted(e, 130, 0));    // sin firma buena, nunca
}

TEST_CASE("actualizar: la huella es BLAKE2b-256 (igual que la del VPS)") {
  UpdateHasher h;
  h.add("PokeSurvivor ", 13);
  h.add("3DS", 3);   // (por trozos, como al descargar)
  uint8_t out[32];
  h.done(out);
  CHECK(hx(out, 32) == "d5b923205f7928c021658116b0c3755b16969cbce12876669fb5f529b48207cc");
}

TEST_CASE("actualizar: parches contra builds anteriores (opcionales y sin firmar)") {
  uint8_t seed[32], sk[64], pk[32];
  for (int i = 0; i < 32; ++i) seed[i] = uint8_t(i * 7 + 1);
  crypto_eddsa_key_pair(sk, pk, seed);
  const std::string h64(64, 'a');
  std::string e = entry_json(sk, 5, 130);
  // Se añaden a la .cia sin volver a firmar (el resultado se comprueba con la huella firmada).
  const std::string parches = ",\"parches\":[{\"desde\":121,\"url\":\"b130/desde-b121.cia.psd\",\"tam\":300,\"huella\":\"" + h64 + "\"},"
                              "{\"desde\":119,\"url\":\"../fuera.psd\",\"tam\":300,\"huella\":\"" + h64 + "\"},"   // (ruta mala: fuera)
                              "{\"desde\":118,\"url\":\"b130/desde-b118.cia.psd\",\"tam\":5000,\"huella\":\"" + h64 + "\"},"   // (más que el archivo: fuera)
                              "{\"desde\":117,\"url\":\"b130/desde-b117.cia.psd\",\"tam\":400,\"huella\":\"corta\"},"   // (huella mala: fuera)
                              "\"basura\","
                              "{\"desde\":112,\"url\":\"b130/desde-b112.cia.psd\",\"tam\":900,\"huella\":\"" + h64 + "\"}]";
  const size_t at = e.find("\"cia\":{");
  e.insert(e.find('}', at), parches);
  UpdateEntry a, b;
  std::string why;
  REQUIRE_MESSAGE(update_parse("{\"actual\":" + e + "}", pk, a, b, why), why);
  CHECK(a.ok);
  CHECK(a.cia.npatches == 2);
  CHECK(a.f3dsx.npatches == 0);
  const UpdatePatch* p = update_patch_for(a.cia, 121);
  REQUIRE(p);
  CHECK(p->url == "b130/desde-b121.cia.psd");
  CHECK(p->size == 300);
  CHECK(p->hash[0] == 0xaa);
  CHECK(update_patch_for(a.cia, 112));
  CHECK_FALSE(update_patch_for(a.cia, 119));
  CHECK_FALSE(update_patch_for(a.cia, 130));
  CHECK_FALSE(update_patch_for(a.f3dsx, 121));
}
