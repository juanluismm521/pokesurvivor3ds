// Megaevoluciones (core/mega, propias del port): datos, la transformación
// entera (y que se deshace bien), las megapiedras que salen, partidas enteras
// usándolas, el guardado y la sala del juego en grupo.
#include <doctest/doctest.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "core/anim.h"
#include "core/lockstep.h"
#include "core/mega.h"
#include "core/save.h"
#include "core/save_io.h"
#include "core/websave.h"
#include "sim.h"

using namespace ps;

TEST_CASE("megas: los datos") {
  CHECK(kMegaCount >= 90);
  int own = 0;
  for (int i = 0; i < kMegaCount; ++i) {
    const MegaDef& m = kMegas[i];
    REQUIRE(mon_by_dex(m.base) != nullptr);
    CHECK(m.mon.dex == m.base);
    CHECK(std::strncmp(m.mon.name, "Mega-", 5) == 0);
    CHECK(m.stone[0] != 0);
    CHECK(m.mon.hp > 0); CHECK(m.mon.spd > 0); CHECK(m.mon.atk > 0.5f);
    CHECK(mega_index(m.base, m.slot) == i);
    if (m.own_sprite) {
      own++;
      CHECK(m.sprite == kMegaSpriteBase + i);
      CHECK(anim_meta(m.sprite, A_IDLE) != nullptr);
      CHECK(anim_meta(m.sprite, A_WALK) != nullptr);
      CHECK(body_height(m.sprite, 1) > 0);
    } else {
      CHECK(m.sprite == m.base);
    }
  }
  CHECK(own >= 30);
  CHECK(std::string(kMegas[mega_index(94, 0)].mon.name) == "Mega-Gengar");
  CHECK(kMegas[mega_index(94, 0)].own_sprite);
  CHECK(mega_count(6) == 2);            // Charizard X e Y
  CHECK(mega_count(491) == 1);          // Mega-Darkrai (Z-A)
  CHECK(kMegas[mega_index(491, 0)].own_sprite);
  CHECK(!has_mega(25));                 // Pikachu no tiene
  // Cada una con su megapiedra de verdad (nombre del juego y su dibujo).
  for (int i = 0; i < kMegaCount; ++i) CHECK_MESSAGE(std::strncmp(kMegas[i].stone, "Megapiedra de", 13) != 0, kMegas[i].mon.name);
  CHECK(std::string(kMegas[mega_index(94, 0)].stone) == "Gengarita");
  CHECK(std::string(kMegas[mega_index(384, 0)].stone) == "Meteorito");   // (Rayquaza no tiene piedra)
  if (FILE* f = std::fopen(PS_SOURCE_DIR "/romfs/gfx/icons.idx", "r")) {   // (si ya se ha generado el romfs)
    std::string idx;
    char line[128];
    while (std::fgets(line, sizeof(line), f)) idx += line;
    std::fclose(f);
    for (int i = 0; i < kMegaCount; ++i) CHECK_MESSAGE(idx.find("stone:" + std::to_string(i) + " ") != std::string::npos, kMegas[i].mon.name);
  }
}

TEST_CASE("megas: megaevolucionar, recarga y volver a la normal") {
  std::unique_ptr<Game> g(new Game());
  RunConfig cfg;
  cfg.dex = 94; cfg.seed = 7; cfg.judged = 1;
  g->start_run(cfg);
  Player& p = g->players[0];
  const float hp0 = p.max_hp, spd0 = p.base_spd, atk0 = p.base_atk;
  CHECK(!mega_ready(p));                // sin la piedra, nada
  player_equip(*g, p, IT_MEGA_A);
  const float am = p.atk_mul, sm = p.spd_mul, cm = p.cd_mul, dr = p.dmg_reduce, rf = p.regen_flat;
  CHECK(mega_ready(p));
  mega_start(*g, p);
  const MegaDef& d = kMegas[mega_index(94, 0)];
  CHECK(p.mega == mega_index(94, 0));
  CHECK(p.mon->name == d.mon.name);   // (su copia, con su naturaleza: Player::mega_buf)
  CHECK(p.base_spd == d.mon.spd);
  CHECK(p.base_atk == doctest::Approx(d.mon.atk));
  CHECK(p.max_hp == doctest::Approx(hp0 + (d.mon.hp - mon_by_dex(94)->hp)));
  CHECK(p.anim.dex == d.sprite);
  CHECK(p.invuln >= kMegaTransform - 0.01f);
  CHECK(!mega_ready(p));                // ya está
  // El empujón de la mega: más ataque, velocidad, recarga, regeneración y defensa.
  CHECK(p.atk_mul == doctest::Approx(am * kMegaAtk));
  CHECK(p.spd_mul == doctest::Approx(sm * kMegaSpd));
  CHECK(p.cd_mul == doctest::Approx(cm * kMegaCd));
  CHECK(p.dmg_reduce == doctest::Approx(dr + kMegaArmor));
  CHECK(p.regen_flat == doctest::Approx(rf + p.max_hp * kMegaRegen));
  // Subir defensa durante la mega no la pierde al acabar (el tope es sin la mega).
  p.dmg_reduce = dr + 0.68f + kMegaArmor;
  Card c; c.type = CT_STAT; c.stat = LS_DEF;
  levelup_apply(*g, p, c);
  CHECK(p.dmg_reduce == doctest::Approx(0.5f + kMegaArmor));
  p.dmg_reduce = dr + kMegaArmor;
  // Pasa el tiempo: se acaba sola, vuelve todo y empieza la recarga.
  for (int i = 0; i < int(kMegaTime * 60) + 5; ++i) mega_update(*g, p, 1.0f / 60);
  CHECK(p.mega == -1);
  CHECK(p.mon->name == mon_by_dex(94)->name);
  CHECK(p.base_spd == spd0);
  CHECK(p.base_atk == doctest::Approx(atk0));
  CHECK(p.max_hp == doctest::Approx(hp0));
  CHECK(p.anim.dex == 94);
  CHECK(p.atk_mul == doctest::Approx(am));
  CHECK(p.spd_mul == doctest::Approx(sm));
  CHECK(p.cd_mul == doctest::Approx(cm));
  CHECK(p.dmg_reduce == doctest::Approx(dr));
  CHECK(p.regen_flat == doctest::Approx(rf));
  CHECK(p.mega_cd > kMegaCooldown - 1);
  mega_start(*g, p);
  CHECK(p.mega == -1);                  // en recarga, no
  for (int i = 0; i < int(kMegaCooldown * 60) + 5; ++i) mega_update(*g, p, 1.0f / 60);
  CHECK(mega_ready(p));
  // Acabar la mega nunca te deja KO aunque tengas poca vida.
  mega_start(*g, p);
  p.hp = 1;
  mega_end(*g, p);
  CHECK(p.hp >= 1);
  // Cambiar la piedra por otro objeto con la mega puesta la acaba.
  for (int i = 0; i < int(kMegaCooldown * 60) + 5; ++i) mega_update(*g, p, 1.0f / 60);
  mega_start(*g, p);
  REQUIRE(p.mega >= 0);
  p.item_cur = p.item; p.item_next = IT_LEFTOVERS; p.choice = CH_ITEM; g->state = ST_CHOICE;
  g->pick_item(p, true);
  CHECK(p.item == IT_LEFTOVERS);
  CHECK(p.mega == -1);
  CHECK(p.mon->name == mon_by_dex(94)->name);
}

TEST_CASE("megas: la vida máxima vuelve a la de antes (aunque subas vida en medio)") {
  std::unique_ptr<Game> g(new Game());
  RunConfig cfg;
  cfg.dex = 6; cfg.seed = 5; cfg.judged = 1;   // (las megas tienen los PS de su forma normal, como en los juegos)
  g->start_run(cfg);
  Player& p = g->players[0];
  const float before = p.max_hp;
  player_equip(*g, p, IT_MEGA_A);
  mega_start(*g, p);
  const float dhp = float(kMegas[mega_index(6, 0)].mon.hp - mon_by_dex(6)->hp);
  CHECK(dhp == 0);
  CHECK(p.max_hp == doctest::Approx(before + dhp));
  player_add_max_hp(p, 20);                     // (una carta de vida en plena mega)
  for (int i = 0; i < int(kMegaTime * 60) + 5; ++i) mega_update(*g, p, 1.0f / 60);
  CHECK(p.mega == -1);
  CHECK(p.max_hp == doctest::Approx(before + 20));
  CHECK(p.hp <= p.max_hp);
  // Cambiar de la X a la Y con la X puesta: se acaba la X (y vuelve todo).
  for (int i = 0; i < int(kMegaCooldown * 60) + 5; ++i) mega_update(*g, p, 1.0f / 60);
  mega_start(*g, p);
  REQUIRE(p.mega == mega_index(6, 0));
  p.item_cur = p.item; p.item_next = IT_MEGA_B; p.choice = CH_ITEM; g->state = ST_CHOICE;
  g->pick_item(p, true);
  CHECK(p.item == IT_MEGA_B);
  CHECK(p.mega == -1);
  CHECK(p.max_hp == doctest::Approx(before + 20));
  for (int i = 0; i < int(kMegaCooldown * 60) + 5; ++i) mega_update(*g, p, 1.0f / 60);
  mega_start(*g, p);
  CHECK(p.mega == mega_index(6, 1));            // ahora, Mega-Charizard Y
  CHECK(std::string(p.mon->name) == "Mega-Charizard Y");
}

TEST_CASE("megas: los shinys megaevolucionados siguen siendo shiny") {
  std::unique_ptr<Game> g(new Game());
  RunConfig cfg;
  cfg.seed = 3; cfg.judged = 1; cfg.shiny = true;
  // Mega-Gengar tiene shiny en SpriteCollab: su sprite de mega, en shiny.
  cfg.dex = 94; g->start_run(cfg);
  Player& p = g->players[0];
  REQUIRE(p.shiny);
  player_equip(*g, p, IT_MEGA_A);
  mega_start(*g, p);
  const MegaDef& d = kMegas[mega_index(94, 0)];
  REQUIRE(has_shiny(d.sprite));
  CHECK(p.anim.dex == d.sprite);
  CHECK(p.anim.shiny);
  CHECK(anim_meta(d.sprite, A_IDLE, true) != nullptr);
  mega_end(*g, p);
  CHECK(p.anim.dex == 94);
  CHECK(p.anim.shiny);                  // y al volver, otra vez shiny
  // Mega-Darkrai no tiene shiny: se queda su sprite shiny (más grande, con aura).
  cfg.dex = 491; g->start_run(cfg);
  Player& q = g->players[0];
  REQUIRE(q.shiny);
  player_equip(*g, q, IT_MEGA_A);
  mega_start(*g, q);
  CHECK(q.mega >= 0);
  CHECK(q.anim.dex == 491);
  CHECK(q.anim.shiny);
  CHECK(q.anim.scale > player_scale(491));
  CHECK(q.mon->name == kMegas[mega_index(491, 0)].mon.name);   // (con las estadísticas de la mega)
  // Sin shiny, Mega-Darkrai sí con su sprite.
  cfg.shiny = false; g->start_run(cfg);
  Player& r = g->players[0];
  player_equip(*g, r, IT_MEGA_A);
  mega_start(*g, r);
  CHECK(r.anim.dex == kMegas[mega_index(491, 0)].sprite);
}

TEST_CASE("megas: qué megapiedra sale y para quién") {
  std::unique_ptr<Game> g(new Game());
  RunConfig cfg;
  cfg.dex = 94; cfg.seed = 11; cfg.judged = 1;
  for (int lvl : {0, 5}) {
    cfg.mega_up = uint8_t(lvl);
    g->start_run(cfg);
    int mega = 0;
    const int N = 20000;
    for (int i = 0; i < N; ++i) if (is_mega_stone(roll_item(*g))) mega++;
    MESSAGE("nivel " << lvl << ": " << 100.0 * mega / N << " % de megapiedras");
    CHECK(double(mega) / N == doctest::Approx(mega_chance(lvl)).epsilon(0.08));
  }
  // Sin mega (Pikachu): nunca, y la tirada es la de la web (ni se tira).
  cfg.dex = 25; cfg.mega_up = 5;
  g->start_run(cfg);
  for (int i = 0; i < 5000; ++i) CHECK(!is_mega_stone(roll_item(*g)));
  // Al cogerla: la de tu Pokémon.
  Player& pk = g->players[0];
  CHECK(!is_mega_stone(mega_stone_for(*g, pk, IT_MEGA_A)));   // Pikachu: otro objeto
  cfg.dex = 94; g->start_run(cfg);
  CHECK(mega_stone_for(*g, g->players[0], IT_MEGA_B) == IT_MEGA_A);   // Gengar sólo tiene una
  cfg.dex = 6; g->start_run(cfg);
  CHECK(mega_stone_for(*g, g->players[0], IT_MEGA_B) == IT_MEGA_B);   // Charizard: la Y
  CHECK(std::string(item_name(IT_MEGA_B, 6)) == kMegas[mega_index(6, 1)].stone);
  CHECK(std::string(item_desc(IT_MEGA_A, 94)).find("Mega-Gengar") != std::string::npos);
  CHECK(mega_up_cost(0) > 0); CHECK(mega_up_cost(kMegaUpMax) == 0);
}

TEST_CASE("megas: partidas enteras usándolas (sin errores)") {
  std::unique_ptr<Game> g(new Game());
  for (int dex : {94, 6, 491, 807}) {   // Gengar, Charizard, Darkrai (Z-A), Zeraora
    int megas = 0;
    bool was = false;
    SimHook hook = [&](Game& gg) {
      Player& p = gg.players[0];
      p.mega_up = kMegaUpMax;
      // Los objetos son raros (como en la web) y el bot no busca cofres: se le
      // da la piedra a los 20 s para ver muchas megaevoluciones seguidas.
      if (gg.time > 20 && !is_mega_stone(p.item) && p.mega < 0 && gg.state == ST_PLAYING) player_equip(gg, p, IT_MEGA_A);   // (y si la cambia, se le devuelve)
      if (p.mega >= 0 && !was) megas++;
      was = p.mega >= 0;
      CHECK(p.max_hp > 0);
    };
    const SimStats s = sim_run(*g, 900 + dex, dex, 600, true, &hook);
    MESSAGE("dex " << dex << ": " << megas << " megaevoluciones en " << s.seconds << " s, nivel " << s.level << ", " << s.kills << " derrotados");
    CHECK_FALSE(s.nan);
    CHECK(s.max_enemies <= kMaxEnemies);
    CHECK(megas >= 7);   // (20 s de mega + 40 de recarga: ~9 en 10 min)
  }
}

TEST_CASE("megas: la mejora se guarda y no se pierde al sincronizar") {
  Save a;
  save_from_json(a, "{}");
  a.pmega[94] = 3;
  Save b;
  REQUIRE(save_from_json(b, save_to_json(a)));
  CHECK(b.pmega[94] == 3);
  // La web no la conoce: aunque su copia no la traiga, el mayor de los dos.
  Json mine, web;
  REQUIRE(mine.parse(save_to_json(a)));
  REQUIRE(web.parse(save_to_json(a)));
  web.set("pmega", Json::object());
  web.set("updated", Json::num(mine["updated"].as_num() + 1000));   // (la de la web, más nueva)
  const Json m = web::merge_safe(mine, web, int64_t(mine["updated"].as_num()) + 2000);
  CHECK(m["pmega"]["94"].as_num() == 3);
  Json w2 = web;
  Json pm = Json::object(); pm.set("94", Json::num(5)); w2.set("pmega", pm);
  CHECK(web::merge_safe(mine, w2, int64_t(mine["updated"].as_num()) + 2000)["pmega"]["94"].as_num() == 5);
}

TEST_CASE("megas: en grupo, la mejora viaja con la sala") {
  RunConfig::Member m;
  m.dex = 94; m.mega_up = 4;
  std::snprintf(m.name, sizeof(m.name), "Ana");
  uint8_t buf[256];
  const int n = net::write_member(m, buf, sizeof(buf));
  RunConfig::Member r;
  REQUIRE(net::read_member(buf, n, r));
  CHECK(r.mega_up == 4);
}

TEST_CASE("megas: las 91, en normal y en shiny, megaevolucionan y juegan sin errores") {
  std::unique_ptr<Game> g(new Game());
  int done = 0;
  for (int i = 0; i < kMegaCount; ++i) {
    const MegaDef& d = kMegas[i];
    for (int sh = 0; sh < 2; ++sh) {
      RunConfig cfg;
      cfg.dex = d.base; cfg.seed = uint32_t(100 + i); cfg.judged = 1; cfg.shiny = sh != 0;
      g->debug_immortal = true;
      g->start_run(cfg);
      Player& p = g->players[0];
      player_equip(*g, p, IT_MEGA_A + d.slot);
      mega_start(*g, p);
      REQUIRE_MESSAGE(p.mega == i, d.mon.name);
      CHECK(p.mon->name == d.mon.name);   // (su copia, con su naturaleza: Player::mega_buf)
      CHECK(anim_meta(p.anim.dex, A_WALK, p.anim.shiny) != nullptr);
      CHECK(p.body_h > 0);
      if (p.shiny) CHECK(p.anim.shiny);   // (el shiny nunca se pierde)
      Bot bot;
      bot.init(7 + i);
      for (int f = 0; f < 60 * 8; ++f) {   // 8 s de partida con la mega
        Input in;
        if (!bot.decide(*g, in)) continue;
        g->step(&in);
      }
      CHECK(finite_all(*g));
      CHECK(p.max_hp > 0);
      mega_end(*g, p);
      CHECK(p.mon->name == mon_by_dex(d.base)->name);
      CHECK(p.anim.dex == d.base);
      done++;
    }
  }
  CHECK(done == kMegaCount * 2);
}

TEST_CASE("megas Z: 5 s menos pero más fuertes; recarga de 40 s") {
  CHECK(kMegaCooldown == 40);
  int nz = 0;
  for (int k = 0; k < kMegaCount; ++k) {
    const std::string n = kMegas[k].mon.name;
    const bool z = n.size() > 2 && n.compare(n.size() - 2, 2, " Z") == 0;
    CHECK(mega_is_z(k) == z);
    nz += z;
  }
  CHECK(nz >= 3);   // (Lucario, Garchomp, Absol… las de Z-A)
  const int lz = mega_index(448, 1);
  REQUIRE(lz >= 0);
  REQUIRE(mega_is_z(lz));
  const MegaBoost z = mega_boost(lz), n = mega_boost(mega_index(448, 0));
  CHECK(z.time == n.time - 5);
  CHECK(z.atk > n.atk);
  CHECK(z.spd > n.spd);
  CHECK(z.cd < n.cd);
  CHECK(z.armor >= n.armor);
  // En la partida: dura 15 s, empuja más y al acabar todo vuelve exacto.
  std::unique_ptr<Game> g(new Game());
  RunConfig cfg; cfg.dex = 448; cfg.seed = 7; cfg.judged = 1;
  g->start_run(cfg);
  Player& p = g->players[0];
  const float am = p.atk_mul, sm = p.spd_mul, cm = p.cd_mul, dr = p.dmg_reduce, rf = p.regen_flat;
  player_equip(*g, p, IT_MEGA_B);
  mega_start(*g, p);
  REQUIRE(p.mega == lz);
  CHECK(p.mega_t == doctest::Approx(kMegaZTime));
  CHECK(p.atk_mul == doctest::Approx(am * kMegaZAtk));
  CHECK(p.spd_mul == doctest::Approx(sm * kMegaZSpd));
  CHECK(p.cd_mul == doctest::Approx(cm * kMegaZCd));
  CHECK(p.dmg_reduce == doctest::Approx(dr + kMegaZArmor));
  for (int i = 0; i < int(kMegaZTime * 60) - 30; ++i) mega_update(*g, p, 1.0f / 60);
  CHECK(p.mega == lz);   // (aún dura)
  for (int i = 0; i < 40; ++i) mega_update(*g, p, 1.0f / 60);
  CHECK(p.mega == -1);
  CHECK(p.atk_mul == doctest::Approx(am));
  CHECK(p.spd_mul == doctest::Approx(sm));
  CHECK(p.cd_mul == doctest::Approx(cm));
  CHECK(p.dmg_reduce == doctest::Approx(dr));
  CHECK(p.regen_flat == doctest::Approx(rf));
  CHECK(p.mega_cd == doctest::Approx(kMegaCooldown).epsilon(0.02));
  // La descripción de la piedra lo cuenta.
  CHECK(std::string(item_desc(IT_MEGA_B, 448)).find("15 s") != std::string::npos);
  CHECK(std::string(item_desc(IT_MEGA_A, 448)).find("20 s") != std::string::npos);
}
