// test_golden.cpp — casos dorados: el port tiene que dar lo mismo que el JS
// original de la web (tools/golden/golden.js lo ejecuta con node).
#include <doctest/doctest.h>

#include "core/stats.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "core/anim.h"
#include "core/game.h"
#include "core/json.h"
#include "core/moves.h"
#include "core/tiles_paint.h"
#include "core/types.h"
#include "core/upgrades.h"
#include "core/util.h"
#include "core/world.h"

using namespace ps;

namespace {
std::string read_file(const std::string& path) {
  std::string s;
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return s;
  char buf[65536];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
  std::fclose(f);
  return s;
}

const Json& golden() {
  static Json j;
  static bool loaded = false;
  if (!loaded) {
    loaded = true;
    const std::string s = read_file(std::string(PS_GOLDEN_DIR) + "/golden.json");
    REQUIRE_MESSAGE(!s.empty(), "falta golden.json (node tools/golden/golden.js)");
    REQUIRE(j.parse(s));
  }
  return j;
}

const char* const kPropNames[PR_COUNT] = {"tree", "rock", "chest", "grass", "altar", "spring", "bigchest", "trap"};

// El mundo es grande (caché de chunks): uno estático para todas las pruebas.
World& test_world() {
  static std::unique_ptr<World> w(new World());
  return *w;
}

bool close_rel(double a, double b, double eps = 2e-5) {
  return std::fabs(a - b) <= eps * std::fmax(1.0, std::fabs(b));
}
}  // namespace

TEST_CASE("dorado: hash y ruido idénticos a la web") {
  const Json& g = golden();
  for (size_t i = 0; i < g["hash"].size(); ++i) {
    const Json& h = g["hash"][i];
    CHECK(hash(int32_t(h[0].as_int()), int32_t(h[1].as_int()), int32_t(h[2].as_int())) == h[3].as_num());
  }
  World& w = test_world();
  for (size_t i = 0; i < g["fbm"].size(); ++i) {
    const Json& h = g["fbm"][i];
    CHECK(w.fbm(h[0].as_num(), h[1].as_num(), int32_t(h[2].as_int())) == doctest::Approx(h[3].as_num()).epsilon(1e-12));
  }
}

TEST_CASE("dorado: terreno y objetos del mapa") {
  const Json& g = golden()["world"];
  World& w = test_world();
  w.reset(int32_t(g["seed"].as_int()));
  const int R = int(g["r"].as_int());
  const std::string& terr = g["terrain"].as_str();
  int bad = 0, k = 0;
  for (int ty = -R; ty < R; ++ty)
    for (int tx = -R; tx < R; ++tx) if (char('0' + w.terrain_at(tx, ty)) != terr[k++]) bad++;
  CHECK(bad == 0);
  for (size_t c = 0; c < g["props"].size(); ++c) {
    const Json& pc = g["props"][c];
    Chunk& ch = w.chunk(int32_t(pc["cx"].as_int()), int32_t(pc["cy"].as_int()));
    const Json& list = pc["list"];
    REQUIRE(size_t(ch.nprops) == list.size());
    for (int i = 0; i < ch.nprops; ++i) {
      CHECK(std::string(kPropNames[ch.props[i].type]) == list[i][0].as_str());
      CHECK(ch.props[i].tx == list[i][1].as_int());
      CHECK(ch.props[i].ty == list[i][2].as_int());
      CHECK(ch.props[i].kind == list[i][3].as_int());
    }
  }
}

TEST_CASE("dorado: arenas de la grieta y de la Sentencia") {
  const Json& arenas = golden()["arenas"];
  World& w = test_world();
  const int32_t seed = int32_t(golden()["world"]["seed"].as_int());
  for (size_t a = 0; a < arenas.size(); ++a) {
    const Json& A = arenas[a];
    w.reset(seed);
    const int tx = int(A["tx"].as_int()), ty = int(A["ty"].as_int()), r = int(A["r"].as_int());
    w.set_arena(true, tx, ty, float(r), arena_theme_index(A["type"].as_str().c_str()));
    const std::string& s = A["terrain"].as_str();
    int bad = 0, k = 0;
    for (int y = ty - r - 3; y <= ty + r + 3; ++y)
      for (int x = tx - r - 3; x <= tx + r + 3; ++x) if (char('0' + w.terrain_at(x, y)) != s[k++]) bad++;
    CHECK_MESSAGE(bad == 0, A["type"].as_str());
    for (size_t c = 0; c < A["props"].size(); ++c) {
      const Json& pc = A["props"][c];
      Chunk& ch = w.chunk(int32_t(pc["cx"].as_int()), int32_t(pc["cy"].as_int()));
      REQUIRE(size_t(ch.nprops) == pc["list"].size());
      for (int i = 0; i < ch.nprops; ++i) {
        CHECK(std::string(kPropNames[ch.props[i].type]) == pc["list"][i][0].as_str());
        CHECK(ch.props[i].tx == pc["list"][i][1].as_int());
      }
    }
  }
}

TEST_CASE("dorado: suelo pintado píxel a píxel") {
  const Json& g = golden();
  World& w = test_world();
  w.reset(int32_t(g["world"]["seed"].as_int()));
  std::vector<uint32_t> px(kChunkPx * kChunkPx);
  static ChunkPainter painter;
  for (size_t i = 0; i < g["chunks"].size(); ++i) {
    const Json& c = g["chunks"][i];
    const std::string ref = read_file(std::string(PS_GOLDEN_DIR) + "/" + c["file"].as_str());
    REQUIRE(ref.size() == px.size() * 4);
    painter.begin(w, int32_t(c["cx"].as_int()), int32_t(c["cy"].as_int()), int(c["biome"].as_int()));
    // Por tiras, como en la consola.
    for (int r = 0; r < 16; r += 4) painter.paint_rows(r, r + 4, px.data());
    int bad = 0;
    for (size_t k = 0; k < px.size(); ++k) {
      uint32_t v;
      std::memcpy(&v, ref.data() + k * 4, 4);
      if (v != px[k]) bad++;
    }
    CHECK_MESSAGE(bad == 0, "chunk " << i << ": " << bad << " píxeles distintos");
  }
}

TEST_CASE("dorado: tabla de tipos") {
  const Json& t = golden()["typeMult"];
  for (size_t i = 0; i < t.size(); ++i) {
    const int8_t d[2] = {int8_t(t[i][1].as_int()), int8_t(t[i][2].as_int())};
    CHECK(type_mult(int(t[i][0].as_int()), d) == doctest::Approx(t[i][3].as_num()).epsilon(1e-6));
  }
}

TEST_CASE("dorado: experiencia, escalado y apuntado") {
  const Json& g = golden();
  for (size_t l = 0; l < g["xpFor"].size(); ++l) CHECK(xp_for(int(l) + 1) == float(g["xpFor"][l].as_num()));
  for (size_t i = 0; i < g["scale"].size(); ++i) {
    const Json& s = g["scale"][i];
    const float t = float(s[0].as_num());
    const Scale3 a = enemies_scale(t), b = enemies_boss_scale(t);
    CHECK(close_rel(a.hp, s[1].as_num()));
    CHECK(close_rel(a.dmg, s[2].as_num()));
    CHECK(close_rel(a.spd, s[3].as_num()));
    CHECK(close_rel(b.hp, s[4].as_num()));
    CHECK(close_rel(b.dmg, s[5].as_num()));
    CHECK(close_rel(spawner_rate(t), s[6].as_num()));
  }
  for (size_t i = 0; i < g["dir"].size(); ++i) CHECK(dir_from_angle(float(g["dir"][i][0].as_num())) == g["dir"][i][1].as_int());
}

TEST_CASE("dorado: Pokédex (personalidad, cuerpo y movimiento inicial)") {
  const Json& d = golden()["dex"];
  REQUIRE(int(d.size()) == kDexCount);
  int bad = 0;
  for (size_t i = 0; i < d.size(); ++i) {
    const Mon* m = mon_by_dex(int(d[i][0].as_int()));
    REQUIRE(m);
    if (std::string(kBehaviorId[behavior_of(*m)]) != d[i][1].as_str()) bad++;
    CHECK(body_radius(m->dex) == doctest::Approx(d[i][2].as_num()).epsilon(1e-5));
    CHECK(body_height(m->dex) == doctest::Approx(d[i][3].as_num()).epsilon(1e-5));
    CHECK(move_start_for(m->dex) == d[i][4].as_int());
  }
  CHECK(bad == 0);
  // Físico/especial, tamaño de los grandes y movimientos para empezar.
  const Json& st = golden()["stats"];
  REQUIRE(int(st.size()) == kDexCount);
  int bad_choices = 0;
  for (size_t i = 0; i < st.size(); ++i) {
    const Json& r = st[i];
    const Mon* m = mon_by_dex(int(r[0].as_int()));
    CHECK(stats_taken(m, CAT_PHYSICAL) == doctest::Approx(r[1].as_num()).epsilon(1e-5));
    CHECK(stats_taken(m, CAT_SPECIAL) == doctest::Approx(r[2].as_num()).epsilon(1e-5));
    CHECK(stats_side_of(m) == (r[3].as_int() ? CAT_SPECIAL : CAT_PHYSICAL));
    CHECK(stats_fit(m, CAT_PHYSICAL) == doctest::Approx(r[4].as_num()).epsilon(1e-5));
    CHECK(stats_fit(m, CAT_SPECIAL) == doctest::Approx(r[5].as_num()).epsilon(1e-5));
    CHECK(player_scale(m->dex) == doctest::Approx(r[6].as_num()).epsilon(1e-6));
    CHECK(player_radius(m->dex) == doctest::Approx(r[7].as_num()).epsilon(1e-5));
    int16_t ch[kMaxMoves];
    const int n = move_start_choices(m->dex, ch, kMaxMoves);
    bool same = size_t(n) == r[8].size();
    for (int k = 0; same && k < n; ++k) same = ch[k] == r[8][k].as_int();
    if (!same) bad_choices++;
  }
  CHECK(bad_choices == 0);
  const Json& f = golden()["defFor"];
  for (size_t i = 0; i < f.size(); ++i) {
    const Json& r = f[i];
    const EnemyDef e = enemy_def_for(*mon_by_dex(int(r[1].as_int())), int(r[0].as_int()));
    CHECK(std::string(kBehaviorId[e.behavior]) == r[2].as_str());
    CHECK(e.hp == float(r[3].as_num()));
    CHECK(e.spd == float(r[4].as_num()));
    CHECK(e.dmg == float(r[5].as_num()));
    CHECK(e.xp == float(r[6].as_num()));
    CHECK(e.shot_dmg == float(r[7].as_num()));
    CHECK(e.shot_cd == doctest::Approx(r[8].as_num()).epsilon(1e-6));
    CHECK(e.range == float(r[9].as_num()));
    CHECK(e.minion == r[10].as_int());
    CHECK(e.fan_n == r[11].as_int());
    CHECK(e.zones == r[12].as_int());
  }
}

TEST_CASE("dorado: movimientos (5 niveles y evolución)") {
  const Json& mv = golden()["moves"];
  for (size_t i = 0; i < mv.size(); ++i) {
    const Json& r = mv[i];
    const int def = move_index(r[0].as_str().c_str());
    REQUIRE(def >= 0);
    MoveInst m = move_instance(def, int(r[1].as_int()));
    if (r[2].as_int()) REQUIRE(move_evolve(m));
    const Json& v = r[3];
    for (int s = 0; s < MS_NUM; ++s) {
      if (v[s].is_null()) { CHECK_MESSAGE(!m.has_(s), r[0].as_str() << " " << kMoveStatId[s]); continue; }
      CHECK_MESSAGE(m.has_(s), r[0].as_str() << " " << kMoveStatId[s]);
      CHECK_MESSAGE(close_rel(m.v[s], v[s].as_num()), r[0].as_str() << " nv" << r[1].as_int() << " " << kMoveStatId[s]);
    }
  }
  const Json& pool = golden()["pool"];
  for (size_t i = 0; i < pool.size(); ++i) {
    int8_t types[2] = {int8_t(pool[i][0][0].as_int()), int8_t(pool[i][0].size() > 1 ? pool[i][0][1].as_int() : -1)};
    int16_t out[kMaxMoves];
    const int n = move_pool_for(types, out, kMaxMoves);
    REQUIRE(size_t(n) == pool[i][1].size());
    for (int k = 0; k < n; ++k) CHECK(out[k] == pool[i][1][k].as_int());
  }
}

TEST_CASE("dorado: mejoras permanentes") {
  const Json& c = golden()["upCost"];
  for (size_t i = 0; i < c.size(); ++i) {
    int id = -1;
    for (int u = 0; u < UP_COUNT; ++u) if (c[i][0].as_str() == kUpgrades[u].id) id = u;
    REQUIRE(id >= 0);
    CHECK(upgrade_cost(id, int(c[i][1].as_int())) == c[i][2].as_int());
  }
  const Json& p = golden()["perks"];
  for (size_t i = 0; i < p.size(); ++i) {
    uint8_t lv[UP_COUNT];
    for (int u = 0; u < UP_COUNT; ++u) lv[u] = uint8_t(p[i][0][u].as_int());
    const Perks k = upgrade_perks(lv);
    const Json& e = p[i][1];
    CHECK(k.hp == float(e[0].as_num()));
    CHECK(close_rel(k.atk_mul, e[1].as_num()));
    CHECK(close_rel(k.spd_mul, e[2].as_num()));
    CHECK(close_rel(k.cd_mul, e[3].as_num()));
    CHECK(close_rel(k.dmg_reduce, e[4].as_num()));
    CHECK(close_rel(k.regen, e[5].as_num()));
    CHECK(close_rel(k.magnet_mul, e[6].as_num()));
    CHECK(close_rel(k.xp_mul, e[7].as_num()));
    CHECK(close_rel(k.coin_mul, e[8].as_num()));
    CHECK(k.start_moves == e[9].as_int());
    CHECK(close_rel(k.crit, e[10].as_num()));
    CHECK(k.rerolls == e[11].as_int());
    CHECK(k.bans == e[12].as_int());
  }
}

TEST_CASE("dorado: huecos con atajos y fusiones de vínculos") {
  const Json& cs = golden()["slots"];
  REQUIRE(cs.size() > 0);
  static Game g;   // (grande: estático)
  for (size_t c = 0; c < cs.size(); ++c) {
    const Json& C = cs[c];
    Player& p = g.players[0];
    RunConfig cfg;
    player_init(g, p, int(C["dex"].as_int()), false, cfg);
    for (const auto& kv : C["bind"].items()) p.bind[move_index(kv.first.c_str())] = int8_t(kv.second.as_int());
    size_t step = 0;
    auto check = [&](const Json& want) {
      if (want.has("none")) return;
      REQUIRE(int(want["slots"].size()) == p.nslots);
      for (int s = 0; s < p.nslots; ++s) {
        REQUIRE(int(want["slots"][s].size()) == p.slot_n[s]);
        for (int j = 0; j < p.slot_n[s]; ++j)
          CHECK_MESSAGE(std::string(p.moves[p.slots[s][j]].d().id) == want["slots"][s][j].as_str(), "caso " << c << " paso " << step);
      }
      CHECK_MESSAGE(p.active_slot == want["active"].as_int(), "caso " << c << " paso " << step);
    };
    for (size_t m = 0; m < C["moves"].size(); ++m) {
      player_add_move(g, p, move_index(C["moves"][m].as_str().c_str()));
      check(C["steps"][step++]);
    }
    for (size_t f = 0; f < C["fuse"].size(); ++f) {
      p.bonds = 1;
      int8_t loose[4]; int nl, grow;
      const int mode = player_bond_options(p, loose, nl, grow);
      if (!mode) { step++; continue; }
      int idx[4], n = 0;
      for (size_t k = 0; k < C["fuse"][f].size(); ++k) idx[n++] = int(C["fuse"][f][k].as_int());
      player_fuse(g, p, idx, n, mode == 1 ? grow : -1);
      check(C["steps"][step++]);
    }
  }
}

// ---------------- partida de la web (db.js) ----------------
#include "core/websave.h"

namespace {
// Primera ruta en la que dos JSON difieren (para entender el fallo).
std::string json_diff(const Json& a, const Json& b, const std::string& path = "") {
  if (a.kind() != b.kind()) return path + " (tipo " + std::to_string(int(a.kind())) + " vs " + std::to_string(int(b.kind())) + ": " + a.dump().substr(0, 60) + " / " + b.dump().substr(0, 60) + ")";
  if (a.is_obj()) {
    for (const auto& kv : a.items()) {
      if (!b.has(kv.first.c_str())) return path + "." + kv.first + " (sólo en la 3DS)";
      const std::string d = json_diff(kv.second, b[kv.first.c_str()], path + "." + kv.first);
      if (!d.empty()) return d;
    }
    for (const auto& kv : b.items()) if (!a.has(kv.first.c_str())) return path + "." + kv.first + " (sólo en la web)";
    return "";
  }
  if (a.is_arr()) {
    if (a.size() != b.size()) return path + " (largo)";
    for (size_t i = 0; i < a.size(); ++i) { const std::string d = json_diff(a[i], b[i], path + "[" + std::to_string(i) + "]"); if (!d.empty()) return d; }
    return "";
  }
  return ps::web::same(a, b) ? "" : path + " (" + a.dump() + " vs " + b.dump() + ")";
}
}  // namespace

TEST_CASE("dorado: partida de la web — migrate, libreta y fusión (db.js)") {
  const Json& cs = golden()["websave"];
  REQUIRE(cs.size() == 5);
  const int64_t NOW = 1700000000000LL;
  for (size_t c = 0; c < cs.size(); ++c) {
    CAPTURE(c);
    const Json& C = cs[c];
    const Json ma = web::migrate(C["a"], "", NOW), mb = web::migrate(C["b"], "", NOW);
    CHECK_MESSAGE(json_diff(ma, C["ma"]) == "", "migrate a");
    CHECK_MESSAGE(json_diff(mb, C["mb"]) == "", "migrate b");
    CHECK_MESSAGE(json_diff(web::led_of(ma, "ctr3ds01"), C["led"]) == "", "ledOf");
    Json fa = ma;
    web::fold(fa, "ctr3ds01");
    CHECK_MESSAGE(json_diff(web::merge(fa, mb, NOW), C["m1"]) == "", "merge 3DS+web");
    CHECK_MESSAGE(json_diff(web::merge(mb, ma, NOW), C["m2"]) == "", "merge web+3DS");
  }
}

TEST_CASE("partida de la web: Firestore ida y vuelta y superposición") {
  Json s = web::new_save("Ana", 1700000000000LL);
  s.ref("owned").set("25", Json::object()).set("at", Json::num(1700000000123.0));
  s.set("ratio", Json::num(0.5));
  Json arr = Json::array(); arr.push(Json()); arr.push(Json::str("x")); arr.push(Json::boolean(true));
  s.set("lista", arr);
  const Json fv = web::to_firestore(s);
  CHECK(fv["mapValue"]["fields"]["coins"]["integerValue"].as_str() == "500");
  CHECK(fv["mapValue"]["fields"]["ratio"]["doubleValue"].as_num() == 0.5);
  CHECK(fv["mapValue"]["fields"]["owned"]["mapValue"]["fields"]["25"]["mapValue"]["fields"]["at"]["integerValue"].as_str() == "1700000000123");
  CHECK(web::same(web::from_firestore(fv), s));
  // Lo que la 3DS no conoce (de la web) se queda al escribir encima.
  Json raw = s;
  raw.set("led", Json::object());
  raw.ref("stats").set("cosaNueva", Json::num(3));
  Json top = Json::object();
  top.set("coins", Json::num(10));
  top.ref("stats").set("runs", Json::num(4));
  const Json o = web::overlay(raw, top);
  CHECK(o["coins"].as_num() == 10);
  CHECK(o["stats"]["runs"].as_num() == 4);
  CHECK(o["stats"]["cosaNueva"].as_num() == 3);
  CHECK(o.has("led"));
}
