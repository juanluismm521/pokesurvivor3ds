// websave.cpp — ver websave.h. Port línea a línea de db.js (newSave,
// migrate, sanitize, ledOf, fold, merge); comprobado con casos dorados.
#include "core/websave.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#include "core/gamedata.h"
#include "core/moves.h"
#include "core/upgrades.h"

namespace ps {
namespace web {

namespace {
const double kNaN = std::numeric_limits<double>::quiet_NaN();
// CUR de db.js, en su orden. Desde el 20-10-2026 la libreta lleva también la
// Mochila ("b:<objeto>", save.bag) y las MT ("m:<movimiento>", save.tms): así una
// Menta gastada en un aparato no vuelve a aparecer al fusionar con el otro.
constexpr int kNCur = 3;
const char* const kCur[kNCur] = {"c", "t1", "t10"};

bool is_obj(const Json& v) { return v.is_obj(); }

// +v de JavaScript.
double plus(const Json& v) {
  switch (v.kind()) {
    case Json::NUM: return v.as_num();
    case Json::BOOL: return v.as_bool() ? 1 : 0;
    case Json::NUL: return 0;
    case Json::STR: {
      const std::string& s = v.as_str();
      size_t a = 0, b = s.size();
      while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
      while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
      if (a == b) return 0;
      const std::string t = s.substr(a, b - a);
      char* end = nullptr;
      const double x = std::strtod(t.c_str(), &end);
      return end && *end == 0 ? x : kNaN;
    }
    case Json::ARR: return v.size() == 0 ? 0 : v.size() == 1 ? plus(v[0]) : kNaN;
    default: return kNaN;
  }
}
bool truthy(const Json& v) {
  switch (v.kind()) {
    case Json::NUL: return false;
    case Json::BOOL: return v.as_bool();
    case Json::NUM: return v.as_num() != 0 && !std::isnan(v.as_num());
    case Json::STR: return !v.as_str().empty();
    default: return true;
  }
}
// v || 0 (como número para Math.max)
double or0(const Json& v) { return truthy(v) ? plus(v) : 0; }
double jsmax(double a, double b) { return std::isnan(a) || std::isnan(b) ? kNaN : (a > b ? a : b); }
// num() de db.js: entero positivo o 0.
// `+v || 0`
double plus0(const Json& v) { const double x = plus(v); return std::isnan(x) || x == 0 ? 0 : x; }
double num(const Json& v) { const double x = plus(v); return std::isfinite(x) && x > 0 ? std::floor(x) : 0; }
Json pair(double a, double b) { Json p = Json::array(); p.push(Json::num(a)); p.push(Json::num(b)); return p; }

std::string key_of(int n) { char b[16]; std::snprintf(b, sizeof(b), "%d", n); return b; }
bool dex_ok(const std::string& k) {
  const int d = std::atoi(k.c_str());
  return key_of(d) == k && mon_by_dex(d) != nullptr;
}
int upgrade_by_id(const std::string& id) {
  for (int k = 0; k < UP_COUNT; ++k) if (id == kUpgrades[k].id) return k;
  return -1;
}

// ---- libreta ----
// isCurKey de db.js: c, t1, t10, b:<objeto> y m:<movimiento>.
bool is_cur_key(const std::string& k) {
  for (const char* c : kCur) if (k == c) return true;
  if (k.size() < 3 || k[1] != ':' || (k[0] != 'b' && k[0] != 'm')) return false;
  const size_t n = k.size() - 2;
  if (n > (k[0] == 'b' ? 12u : 40u)) return false;
  for (size_t i = 2; i < k.size(); ++i) {
    const char ch = k[i];
    if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || (k[0] == 'm' && ch == '-'))) return false;
  }
  return true;
}
const char* sub_of(const std::string& k) { return k[0] == 'b' ? "bag" : "tms"; }
double cur_get(const Json& s, const std::string& k) {
  if (k == "c") return num(s["coins"]);
  if (k == "t1" || k == "t10") return num(s["tickets"][k.c_str()]);
  return num(s[sub_of(k)][k.c_str() + 2]);
}
void cur_set(Json& r, const std::string& k, double v) {
  if (k == "c") { r.set("coins", Json::num(v)); return; }
  if (k == "t1" || k == "t10") { r.ref("tickets").set(k, Json::num(v)); return; }
  Json& o = r.ref(sub_of(k));
  if (!o.is_obj()) o = Json::object();
  if (v > 0) o.set(k.substr(2), Json::num(std::fmin(999.0, v)));
  else o.remove(k.substr(2));
}
// curKeys de db.js: las cosas con saldo de una partida (y de su libreta), en su orden.
std::vector<std::string> cur_keys(const Json& s, const Json* led) {
  std::vector<std::string> ks;
  auto add = [&](const std::string& k) { for (const auto& q : ks) if (q == k) return; ks.push_back(k); };
  for (const char* c : kCur) add(c);
  if (s["bag"].is_obj()) for (const auto& kv : s["bag"].items()) add("b:" + kv.first);
  if (s["tms"].is_obj()) for (const auto& kv : s["tms"].items()) add("m:" + kv.first);
  if (led && led->is_obj())
    for (const auto& dv : led->items())
      if (dv.second.is_obj()) for (const auto& kv : dv.second.items()) if (is_cur_key(kv.first)) add(kv.first);
  return ks;
}
double led_sum(const Json& led, const std::string& kk) {
  const char* k = kk.c_str();
  double t = 0;
  for (const auto& kv : led.items()) {
    const Json& e = kv.second[k];
    if (truthy(kv.second) && truthy(e)) t += num(e[0]) - num(e[1]);
  }
  return t;
}
void add_to(Json& led, const std::string& dev, const std::string& k, double d) {
  if (d == 0 || std::isnan(d)) return;
  Json& e = led.ref(dev);
  if (!truthy(e)) e = Json::object();
  Json& p = e.ref(k);
  if (!truthy(p)) p = pair(0, 0);
  std::vector<Json>& a = p.elems();
  while (a.size() < 2) a.push_back(Json::num(0));
  if (d > 0) a[0] = Json::num(num(a[0]) + d);
  else a[1] = Json::num(num(a[1]) - d);
}
}  // namespace

Json new_save(const std::string& name, int64_t now_ms) {
  Json s = Json::object();
  s.set("v", Json::num(kVersion));
  s.set("name", Json::str(name));
  s.set("created", Json::num(double(now_ms)));
  s.set("updated", Json::num(double(now_ms)));
  s.set("coins", Json::num(500));
  s.set("owned", Json::object());
  s.set("shiny", Json::object());
  s.set("partner", Json());
  s.set("partnerShiny", Json::boolean(false));
  s.set("starter", Json());
  s.set("personality", Json());
  s.set("upgrades", Json::object());
  s.set("pupg", Json::object());
  s.set("dfav", Json::object());      // dex -> fecha: favorito de la colección (negativa = quitado entonces)
  s.set("pused", Json::object());     // dex -> partidas jugadas con él
  s.set("pstart", Json::object());    // dex -> movimiento inicial elegido (Mejoras → Movimiento inicial)
  s.set("pstart2", Json::object());   // dex -> segundo movimiento inicial (la web; el port usa pmove2)
  // Tienda Kecleon (web, 20-10-2026): Mochila, MT sin usar, aprendidas, naturaleza y habilidad.
  s.set("bag", Json::object());
  s.set("tms", Json::object());
  s.set("ptm", Json::object());
  s.set("pnat", Json::object());
  s.set("pabil", Json::object());
  // Halloween (web, 25-10-2026): disfraces comprados y el puesto de cada Pokémon.
  s.set("skins", Json::object());
  s.set("pskin", Json::object());
  Json tk = Json::object(); tk.set("t1", Json::num(0)); tk.set("t10", Json::num(0));
  s.set("tickets", tk);
  s.set("pity", Json::object());
  s.set("legends", Json::object());
  s.set("arenas", Json::object());
  s.set("itemsFound", Json::object());
  s.set("evoMoves", Json::object());
  s.set("ach", Json::object());
  s.set("missions", Json::object());
  Json prof = Json::object(); prof.set("favs", Json::array()); prof.set("title", Json::str(""));
  s.set("profile", prof);
  Json st = Json::object();
  for (const char* k : {"runs", "bestTime", "bestLevel", "totalKills", "pulls", "coinsEarned", "shinies", "bossesTotal", "riftWins",
                        "evolutions", "missionsDone", "groupRuns", "bestGroupTime"})
    st.set(k, Json::num(0));
  s.set("stats", st);
  return s;
}

void sanitize(Json& s, bool strict) {
  s.set("coins", Json::num(num(s["coins"])));
  Json& tk = s.ref("tickets");
  tk.set("t1", Json::num(num(tk["t1"])));
  tk.set("t10", Json::num(num(tk["t10"])));
  if (Json* led = s.get("led"); led && is_obj(*led)) {
    for (size_t i = led->items().size(); i-- > 0;) {
      auto& kv = led->items_mut()[i];
      Json& e = kv.second;
      if (!is_obj(e)) { led->items_mut().erase(led->items_mut().begin() + long(i)); continue; }
      for (size_t j = e.items().size(); j-- > 0;) {
        auto& ek = e.items_mut()[j];
        const bool cur = is_cur_key(ek.first);
        // (sin strict, las monedas nuevas de una web más nueva se quedan)
        if ((strict && !cur) || !ek.second.is_arr()) { e.items_mut().erase(e.items_mut().begin() + long(j)); continue; }
        ek.second = pair(num(ek.second[0]), num(ek.second[1]));
      }
    }
  } else {
    s.remove("led");
  }
  for (const char* coll : {"owned", "shiny", "dfav", "pused"}) {
    Json* c = s.get(coll);
    if (!strict || !c || !c->is_obj()) continue;
    auto& it = c->items_mut();
    for (size_t i = it.size(); i-- > 0;) if (!dex_ok(it[i].first)) it.erase(it.begin() + long(i));
  }
  // Favoritos y partidas por Pokémon: números (lo demás fuera).
  for (const char* coll : {"dfav", "pused"}) {
    Json* c = s.get(coll);
    if (!c || !c->is_obj()) { s.set(coll, Json::object()); continue; }
    auto& it = c->items_mut();
    for (size_t i = it.size(); i-- > 0;) if (!std::isfinite(plus(it[i].second))) it.erase(it.begin() + long(i));
  }
  // Movimiento inicial (y el segundo de la web): un movimiento que exista de un Pokémon que exista.
  for (const char* key : {"pstart", "pstart2"}) {
    Json* ps = s.get(key);
    if (!ps || !ps->is_obj()) { s.set(key, Json::object()); ps = s.get(key); }
    auto& it = ps->items_mut();
    for (size_t i = it.size(); i-- > 0;)
      if (!it[i].second.is_str() || (strict && (!dex_ok(it[i].first) || move_index(it[i].second.as_str().c_str()) < 0)))
        it.erase(it.begin() + long(i));
  }
  // Mochila y MT: cuántas de cada cosa que exista (tope 999). (Sin strict, lo que
  // traiga una web más nueva se queda.)
  for (const char* key : {"bag", "tms"}) {
    Json* o = s.get(key);
    if (!o || !o->is_obj()) { s.set(key, Json::object()); continue; }
    auto& it = o->items_mut();
    for (size_t i = it.size(); i-- > 0;) {
      const double v = std::fmin(999.0, num(it[i].second));
      bool ok = true;
      if (strict && key[0] == 'b') {
        ok = it[i].first == "candy";
        for (const char* id : {"mint", "cap", "caph"}) if (it[i].first == id) ok = true;
      } else if (strict) {
        ok = move_index(it[i].first.c_str()) >= 0;
      }
      if (!v || !ok) it.erase(it.begin() + long(i));
      else it[i].second = Json::num(v);
    }
  }
  // MT aprendidas: movimientos que existen, sin repetir.
  {
    Json* o = s.get("ptm");
    if (!o || !o->is_obj()) { s.set("ptm", Json::object()); o = s.get("ptm"); }
    auto& it = o->items_mut();
    for (size_t i = it.size(); i-- > 0;) {
      Json l = Json::array();
      if (it[i].second.is_arr())
        for (const Json& id : it[i].second.elems()) {
          if (!id.is_str() || (strict && move_index(id.as_str().c_str()) < 0)) continue;
          bool dup = false;
          for (const Json& q : l.elems()) if (q.as_str() == id.as_str()) dup = true;
          if (!dup) l.push(id);
        }
      if (!l.size() || (strict && !dex_ok(it[i].first))) { it.erase(it.begin() + long(i)); continue; }
      it[i].second = l;
    }
  }
  // Naturaleza y habilidad elegidas: [valor, fecha].
  for (const char* key : {"pnat", "pabil"}) {
    Json* o = s.get(key);
    if (!o || !o->is_obj()) { s.set(key, Json::object()); continue; }
    auto& it = o->items_mut();
    for (size_t i = it.size(); i-- > 0;) {
      const Json e = it[i].second;
      bool ok = e.is_arr();
      if (ok && key[1] == 'n') {
        ok = e[0].is_str();
        if (ok && strict) { ok = false; for (const NatureDef& n : kNatures) if (e[0].as_str() == n.id) ok = true; }
      } else if (ok) {
        ok = e[0].is_num() && (e[0].as_num() == 0 || e[0].as_num() == 1 || e[0].as_num() == 2);
      }
      if (!ok || (strict && !dex_ok(it[i].first))) { it.erase(it.begin() + long(i)); continue; }
      Json q = Json::array();
      q.push(e[0]);
      q.push(Json::num(num(e[1])));
      it[i].second = q;
    }
  }
  // Disfraces: los que existen; el puesto, de ese Pokémon ('' = ninguno) y con su fecha.
  {
    Json* o = s.get("skins");
    if (!o || !o->is_obj()) { s.set("skins", Json::object()); o = s.get("skins"); }
    auto& it = o->items_mut();
    auto skin_ok = [](const std::string& id, int dex) {
      for (int k = 0; k < kSkinCount; ++k) if (kSkins[k].sell && id == kSkins[k].id) return dex < 0 || kSkins[k].dex == dex;
      return false;
    };
    for (size_t i = it.size(); i-- > 0;)
      if ((strict && !skin_ok(it[i].first, -1)) || !std::isfinite(plus(it[i].second))) it.erase(it.begin() + long(i));
    Json* p = s.get("pskin");
    if (!p || !p->is_obj()) { s.set("pskin", Json::object()); p = s.get("pskin"); }
    auto& pt = p->items_mut();
    for (size_t i = pt.size(); i-- > 0;) {
      const Json& e = pt[i].second;
      const Json id = e.is_arr() ? e[0] : Json();
      if (!id.is_str() || (strict && !id.as_str().empty() && !skin_ok(id.as_str(), std::atoi(pt[i].first.c_str())))) { pt.erase(pt.begin() + long(i)); continue; }
      Json q = Json::array();
      q.push(id);
      q.push(Json::num(num(e[1])));
      pt[i].second = q;
    }
  }
  // Atajos: 4 huecos con un movimiento que exista (o null), sin repetir.
  Json* pb = s.get("pbind");
  if (pb && pb->kind() == Json::OBJ) {
    for (auto& kv : pb->items_mut()) {
      const Json l = kv.second;
      Json out = Json::array();
      std::vector<std::string> seen;
      for (int i = 0; i < 4; ++i) {
        const Json& id = l[i];   // (si no es una lista, null)
        bool ok = id.is_str() && (!strict || move_index(id.as_str().c_str()) >= 0);
        for (const auto& q : seen) if (ok && q == id.as_str()) ok = false;
        if (ok) { seen.push_back(id.as_str()); out.push(Json::str(id.as_str())); }
        else out.push(Json());
      }
      kv.second = out;
    }
  } else {
    s.set("pbind", Json::object());
  }
  // Veto: movimientos que existen, sin repetir, como mucho 3.
  Json* pn = s.get("pban");
  if (pn && pn->kind() == Json::OBJ) {
    for (auto& kv : pn->items_mut()) {
      Json out = Json::array();
      std::vector<std::string> seen;
      if (kv.second.is_arr())
        for (const Json& id : kv.second.elems()) {
          if (!id.is_str() || (strict && move_index(id.as_str().c_str()) < 0)) continue;
          bool dup = false;
          for (const auto& q : seen) if (q == id.as_str()) dup = true;
          if (dup || seen.size() >= 3) continue;
          seen.push_back(id.as_str());
          out.push(Json::str(id.as_str()));
        }
      kv.second = out;
    }
  } else {
    s.set("pban", Json::object());
  }
  if (Json* pu = s.get("pupg"); pu && pu->is_obj()) {
    auto& it = pu->items_mut();
    for (size_t i = it.size(); i-- > 0;) {
      Json& u = it[i].second;
      if (!truthy(u) || !(u.is_obj() || u.is_arr())) { it.erase(it.begin() + long(i)); continue; }
      if (!u.is_obj()) continue;
      auto& ui = u.items_mut();
      for (size_t j = ui.size(); j-- > 0;) {
        const int k = upgrade_by_id(ui[j].first);
        if (!strict) ui[j].second = Json::num(num(ui[j].second));   // (sin recortar: la web pudo subir el máximo)
        else if (k < 0) ui.erase(ui.begin() + long(j));
        else ui[j].second = Json::num(std::fmin(double(kUpgrades[k].max), num(ui[j].second)));
      }
    }
  }
}

Json migrate(Json s, const std::string& name, int64_t now_ms, bool strict) {
  const Json base = new_save(name.empty() ? s["name"].as_str() : name, now_ms);
  for (const auto& kv : base.items()) {
    Json* v = s.get(kv.first);
    if (!v || v->is_null()) s.set(kv.first, kv.second);
  }
  if (Json* st = s.get("stats"); st && st->is_obj())
    for (const auto& kv : base["stats"].items()) {
      Json* v = st->get(kv.first);
      if (!v || v->is_null()) st->set(kv.first, kv.second);
    }
  if (s["pity"].is_num()) { Json p = Json::object(); p.set("1", s["pity"]); s.set("pity", p); }
  if (!truthy(s["pupgDone"])) {
    bool any = false;
    if (s["upgrades"].is_obj()) for (const auto& kv : s["upgrades"].items()) if (plus(kv.second) > 0) any = true;
    if (truthy(s["partner"]) && any) {
      char k[32];
      const Json& p = s["partner"];
      if (p.is_num()) std::snprintf(k, sizeof(k), "%lld", (long long)p.as_num()); else std::snprintf(k, sizeof(k), "%s", p.as_str().c_str());
      s.ref("pupg").set(k, s["upgrades"]);
    }
    s.set("pupgDone", Json::boolean(true));
  }
  Json tk = Json::object(); tk.set("t1", Json::num(0)); tk.set("t10", Json::num(0));
  if (s["tickets"].is_obj()) for (const auto& kv : s["tickets"].items()) tk.set(kv.first, kv.second);
  s.set("tickets", tk);
  sanitize(s, strict);
  // (una partida de un formato más nuevo conserva su versión: la 3DS no la baja)
  if (version_of(s) < kVersion) s.set("v", Json::num(kVersion));
  return s;
}

Json led_of(const Json& s, const std::string& dev) {
  if (!is_obj(s["led"])) {
    Json base = Json::object();
    for (const auto& k : cur_keys(s, nullptr)) base.set(k, pair(cur_get(s, k), 0));
    Json led = Json::object();
    led.set("base", base);
    return led;
  }
  Json led = s["led"];
  // Un saldo en negativo (lo mismo gastado en dos aparatos sin conexión) se ve como 0.
  for (const auto& k : cur_keys(s, &led)) add_to(led, dev, k, cur_get(s, k) - std::fmax(0.0, led_sum(led, k)));
  return led;
}

void fold(Json& s, const std::string& dev) {
  // Libreta en negativo (lo mismo gastado en dos aparatos): lo debido se perdona una
  // vez en led.fix (web, 25-10-2026); al juntar se toma el mayor de cada apunte, así
  // que dos aparatos que ven la misma deuda no la perdonan dos veces.
  if (Json* l = s.get("led"); l && is_obj(*l))
    for (const auto& k : cur_keys(s, l)) {
      const double owed = -led_sum(*l, k);
      if (owed > 0) add_to(*l, "fix", k, owed);
    }
  s.set("led", led_of(s, dev));
  // Y el saldo es siempre el de la libreta.
  const Json led = s["led"];
  for (const auto& k : cur_keys(s, &led)) cur_set(s, k, std::fmax(0.0, led_sum(led, k)));
}

Json merge(const Json& a, const Json& b, int64_t now_ms) {
  if (a.is_null()) return b;
  if (b.is_null()) return a;
  const bool a_newer = or0(a["updated"]) >= or0(b["updated"]);
  const Json& newer = a_newer ? a : b;
  const Json& older = a_newer ? b : a;
  Json r = newer;
  // Monedas y tickets: el mayor de cada apunte de cada aparato; el saldo es la suma.
  const Json la = led_of(a, "old"), lb = led_of(b, "old");
  Json led = Json::object();
  for (const Json* src : {&la, &lb})
    for (const auto& dv : src->items()) {
      if (!dv.second.is_obj()) continue;
      Json& ld = led.ref(dv.first);
      if (!ld.is_obj()) ld = Json::object();
      for (const auto& kv : dv.second.items()) {
        Json& p = ld.ref(kv.first);
        if (!p.is_arr()) p = pair(0, 0);
        const Json& q = kv.second;
        p.elems()[0] = Json::num(jsmax(p[0].as_num(), num(q[0])));
        p.elems()[1] = Json::num(jsmax(p[1].as_num(), num(q[1])));
      }
    }
  r.set("led", led);
  if (!truthy(r["tickets"])) { Json tk = Json::object(); tk.set("t1", Json::num(0)); tk.set("t10", Json::num(0)); r.set("tickets", tk); }
  // Monedas, tickets, Mochila y MT: el saldo de la libreta.
  r.set("bag", Json::object());
  r.set("tms", Json::object());
  {
    Json empty = Json::object();
    empty.set("bag", Json::object());
    empty.set("tms", Json::object());
    for (const auto& k : cur_keys(empty, &led)) cur_set(r, k, std::fmax(0.0, led_sum(led, k)));
  }
  // MT aprendidas: nunca se olvidan (unión). Naturaleza, habilidad y disfraz: lo elegido más tarde.
  {
    Json& rt = r.ref("ptm");
    if (!truthy(rt) || !rt.is_obj()) rt = Json::object();
    if (older["ptm"].is_obj())
      for (const auto& kv : older["ptm"].items()) {
        Json l = rt[kv.first.c_str()].is_arr() ? rt[kv.first.c_str()] : Json::array();
        auto has = [&](const Json& id) { for (const Json& q : l.elems()) if (same(q, id)) return true; return false; };
        if (kv.second.is_arr()) for (const Json& id : kv.second.elems()) if (!has(id)) l.push(id);
        rt.set(kv.first, l);
      }
  }
  for (const char* k : {"pnat", "pabil", "pskin"}) {
    Json& rp = r.ref(k);
    if (!truthy(rp) || !rp.is_obj()) rp = Json::object();
    if (older[k].is_obj())
      for (const auto& kv : older[k].items()) {
        const Json& mine = rp[kv.first.c_str()];
        if (!truthy(mine) || plus0(kv.second[1]) > plus0(mine[1])) rp.set(kv.first, kv.second);
      }
  }
  for (const char* coll : {"owned", "shiny"}) {
    Json& rc = r.ref(coll);
    if (!truthy(rc)) rc = Json::object();
    if (older[coll].is_obj())
      for (const auto& kv : older[coll].items()) {
        const Json& mine = rc[kv.first.c_str()];
        if (!truthy(mine) || or0(kv.second["at"]) < or0(mine["at"])) rc.set(kv.first, kv.second);
      }
  }
  if (older["upgrades"].is_obj())
    for (const auto& kv : older["upgrades"].items()) {
      Json& ru = r.ref("upgrades");
      if (!ru.is_obj()) ru = Json::object();
      ru.set(kv.first, Json::num(jsmax(or0(ru[kv.first.c_str()]), plus(kv.second))));
    }
  {
    Json& rp = r.ref("pupg");
    if (!truthy(rp)) rp = Json::object();
    if (older["pupg"].is_obj())
      for (const auto& dv : older["pupg"].items()) {
        Json& mine = rp.ref(dv.first);
        if (!truthy(mine)) mine = Json::object();
        if (dv.second.is_obj())
          for (const auto& kv : dv.second.items()) mine.set(kv.first, Json::num(jsmax(or0(mine[kv.first.c_str()]), plus(kv.second))));
      }
  }
  r.set("pupgDone", truthy(r["pupgDone"]) ? r["pupgDone"] : older["pupgDone"]);
  // Favoritos: gana lo último que se hizo con cada uno (poner o quitar), en
  // cualquier aparato. Partidas con cada Pokémon: el mayor de los dos.
  {
    Json& rf = r.ref("dfav");
    if (!truthy(rf)) rf = Json::object();
    if (older["dfav"].is_obj() && rf.is_obj())
      for (const auto& kv : older["dfav"].items())
        if (std::fabs(plus0(kv.second)) > std::fabs(plus0(rf[kv.first.c_str()]))) rf.set(kv.first, kv.second);
    Json& ru = r.ref("pused");
    if (!truthy(ru)) ru = Json::object();
    if (older["pused"].is_obj() && ru.is_obj())
      for (const auto& kv : older["pused"].items())
        ru.set(kv.first, Json::num(std::fmax(plus0(ru[kv.first.c_str()]), plus0(kv.second))));
    // Del port: la mejora «Megapiedra» (pmega), el mayor de los dos (la web no
    // la conoce y podría no traerla: así nunca se pierde lo comprado).
    if (older["pmega"].is_obj()) {
      Json& rm = r.ref("pmega");
      if (!rm.is_obj()) rm = Json::object();
      for (const auto& kv : older["pmega"].items())
        rm.set(kv.first, Json::num(std::fmax(plus0(rm[kv.first.c_str()]), plus0(kv.second))));
    }
    if (older["pfour"].is_obj()) {
      Json& rf4 = r.ref("pfour");
      if (!rf4.is_obj()) rf4 = Json::object();
      for (const auto& kv : older["pfour"].items())
        rf4.set(kv.first, Json::num(std::fmax(plus0(rf4[kv.first.c_str()]), plus0(kv.second))));
    }
    // Del port: medallas de Arceus (modo rápido). Se juntan las dos copias (la web no
    // las conoce: así nunca se pierde una).
    if (older["arceusWin"].is_obj()) {
      Json& ra = r.ref("arceusWin");
      if (!ra.is_obj()) ra = Json::object();
      for (const auto& kv : older["arceusWin"].items())
        if (!ra.has(kv.first.c_str()) || !ra[kv.first.c_str()].is_num()) ra.set(kv.first, kv.second);
    }
    // Del port: regalos recibidos (core/gift.h): se juntan, para que nunca se repitan.
    if (older["regalos"].is_obj()) {
      Json& rg = r.ref("regalos");
      if (!rg.is_obj()) rg = Json::object();
      for (const auto& kv : older["regalos"].items()) if (!rg.has(kv.first.c_str())) rg.set(kv.first, kv.second);
    }
    // Del port: segundo movimiento de «Repertorio», igual que el inicial.
    if (older["pmove2"].is_obj()) {
      Json& r2 = r.ref("pmove2");
      if (!r2.is_obj()) r2 = Json::object();
      for (const auto& kv : older["pmove2"].items()) if (!r2.has(kv.first.c_str())) r2.set(kv.first, kv.second);
    }
    // Movimiento inicial (y el segundo de la web): el de la versión más reciente; si no
    // lo tenía elegido, el de la otra.
    for (const char* k : {"pstart", "pstart2"}) {
      Json& rs = r.ref(k);
      if (!truthy(rs)) rs = Json::object();
      if (older[k].is_obj() && rs.is_obj())
        for (const auto& kv : older[k].items()) if (!rs.has(kv.first.c_str())) rs.set(kv.first, kv.second);
    }
  }
  // Lo conseguido nunca se pierde: unión (los disfraces comprados también).
  for (const char* coll : {"legends", "arenas", "itemsFound", "evoMoves", "ach", "skins"}) {
    Json& rc = r.ref(coll);
    if (!truthy(rc)) rc = Json::object();
    if (older[coll].is_obj())
      for (const auto& kv : older[coll].items()) if (!truthy(rc[kv.first.c_str()])) rc.set(kv.first, kv.second);
  }
  // Misiones del mismo día / semana: el mayor progreso y lo ya recogido.
  {
    const Json mo = truthy(older["missions"]) ? older["missions"] : Json::object();
    Json& mr = r.ref("missions");
    if (!truthy(mr)) mr = Json::object();
    const char* const PER[2][2] = {{"day", "daily"}, {"week", "weekly"}};
    for (const auto& pl : PER) {
      const Json& a1 = mr[pl[0]];
      const Json& b1 = mo[pl[0]];
      const bool eq = a1.kind() == b1.kind() && (a1.is_str() ? a1.as_str() == b1.as_str() : a1.is_num() ? a1.as_num() == b1.as_num() : a1.is_null() || b1.is_null() ? a1.is_null() && b1.is_null() : false);
      if (!eq || !truthy(mr[pl[1]]) || !truthy(mo[pl[1]])) continue;
      Json* list = mr.get(pl[1]);
      if (!list->is_arr()) continue;
      for (size_t i = 0; i < list->size(); ++i) {
        Json& m = list->elems()[i];
        const Json& o = mo[pl[1]][i];
        if (!truthy(o) || !m.is_obj()) continue;
        if (!same(o["id"], m["id"])) continue;
        m.set("prog", Json::num(jsmax(or0(m["prog"]), or0(o["prog"]))));
        m.set("claimed", truthy(m["claimed"]) ? m["claimed"] : o["claimed"]);
        m.set("told", truthy(m["told"]) ? m["told"] : o["told"]);
      }
    }
  }
  if (older["stats"].is_obj())
    for (const auto& kv : older["stats"].items()) {
      Json& rs = r.ref("stats");
      if (!rs.is_obj()) rs = Json::object();
      rs.set(kv.first, Json::num(jsmax(or0(rs[kv.first.c_str()]), or0(kv.second))));
    }
  if (r["starter"].is_null()) r.set("starter", older["starter"]);
  if (r["personality"].is_null()) r.set("personality", older["personality"]);
  {
    const Json& p = r["partner"];
    bool owned = false;
    if (!p.is_null()) {
      char k[32];
      if (p.is_num()) std::snprintf(k, sizeof(k), "%lld", (long long)p.as_num()); else std::snprintf(k, sizeof(k), "%s", p.as_str().c_str());
      owned = truthy(r["owned"][k]);
    }
    if (p.is_null() || !owned) { r.set("partner", older["partner"]); r.set("partnerShiny", older["partnerShiny"]); }
  }
  const double ca = truthy(a["created"]) ? plus(a["created"]) : double(now_ms), cb = truthy(b["created"]) ? plus(b["created"]) : double(now_ms);
  r.set("created", Json::num(std::fmin(ca, cb)));
  r.set("updated", Json::num(jsmax(or0(a["updated"]), or0(b["updated"]))));
  return r;
}

Json overlay(const Json& base, const Json& top) {
  if (!base.is_obj() || !top.is_obj()) return top;
  Json r = base;
  for (const auto& kv : top.items()) {
    Json* cur = r.get(kv.first);
    if (cur && cur->is_obj() && kv.second.is_obj()) *cur = overlay(*cur, kv.second);
    else r.set(kv.first, kv.second);
  }
  return r;
}

int version_of(const Json& s) { return s["v"].is_num() ? int(s["v"].as_num()) : 0; }

Json overlay_changes(const Json& raw, const Json& was, const Json& now) {
  if (!raw.is_obj() || !now.is_obj()) return now;
  Json r = raw;
  for (const auto& kv : now.items()) {
    const char* k = kv.first.c_str();
    if (was.is_obj() && was.has(k) && same(was[k], kv.second) && raw.has(k)) continue;   // no ha cambiado: lo de la web
    Json* cur = r.get(kv.first);
    if (cur && cur->is_obj() && kv.second.is_obj()) *cur = overlay_changes(*cur, was[k], kv.second);
    else r.set(kv.first, kv.second);
  }
  return r;
}

namespace {
bool known_key(const std::string& k) {
  static const char* const K[] = {"v", "name", "created", "updated", "coins", "owned", "shiny", "partner", "partnerShiny", "starter",
                                  "personality", "upgrades", "pupg", "pupgDone", "tickets", "pity", "legends", "arenas", "itemsFound",
                                  "evoMoves", "ach", "missions", "profile", "stats", "led", "pbind", "pban", "dfav", "pused", "pstart", "pmega", "pfour", "pmove2", "arceusWin", "regalos",
                                  "skins", "pskin", "pstart2", "bag", "tms", "ptm", "pnat", "pabil"};
  for (const char* q : K) if (k == q) return true;
  return false;
}
// Añade a `dst` lo que tiene `src` y le falta (en profundidad).
void add_missing(Json& dst, const Json& src) {
  if (!dst.is_obj() || !src.is_obj()) return;
  for (const auto& kv : src.items()) {
    Json* d = dst.get(kv.first);
    if (!d) dst.set(kv.first, kv.second);
    else if (d->is_obj() && kv.second.is_obj()) add_missing(*d, kv.second);
  }
}
}  // namespace

Json merge_safe(const Json& a, const Json& b, int64_t now_ms) {
  Json r = merge(a, b, now_ms);
  if (!a.is_obj() || !b.is_obj()) return r;
  for (const Json* side : {&a, &b})
    for (const auto& kv : side->items()) {
      if (known_key(kv.first)) continue;
      Json* d = r.get(kv.first);
      if (!d) r.set(kv.first, kv.second);
      else if (d->is_obj() && kv.second.is_obj()) add_missing(*d, kv.second);
    }
  return r;
}

bool loses_progress(const Json& prev, const Json& merged, std::string& what) {
  if (!prev.is_obj()) return false;
  for (const char* coll : {"owned", "shiny", "legends", "arenas", "itemsFound", "evoMoves", "ach"})
    for (const auto& kv : prev[coll].items())
      if (!truthy(merged[coll][kv.first.c_str()]) && truthy(kv.second)) { what = std::string(coll) + "." + kv.first; return true; }
  for (const auto& dv : prev["pupg"].items())
    for (const auto& kv : dv.second.items())
      if (plus(merged["pupg"][dv.first.c_str()][kv.first.c_str()]) < plus(kv.second)) { what = "pupg." + dv.first + "." + kv.first; return true; }
  for (const auto& kv : prev["stats"].items())
    if (plus(merged["stats"][kv.first.c_str()]) < plus(kv.second)) { what = "stats." + kv.first; return true; }
  // Las MT aprendidas no se olvidan.
  for (const auto& dv : prev["ptm"].items()) {
    if (!dv.second.is_arr()) continue;
    for (const Json& id : dv.second.elems()) {
      bool in = false;
      for (const Json& q : merged["ptm"][dv.first.c_str()].elems()) if (same(q, id)) in = true;
      if (!in) { what = "ptm." + dv.first; return true; }
    }
  }
  // La libreta: ningún apunte baja (lo ganado y gastado sólo crece).
  for (const auto& dv : prev["led"].items())
    for (const auto& kv : dv.second.items())
      for (int i = 0; i < 2; ++i)
        if (plus(merged["led"][dv.first.c_str()][kv.first.c_str()][i]) < plus(kv.second[i])) { what = "led." + dv.first + "." + kv.first; return true; }
  return false;
}

// ---------------- Firestore ----------------

Json to_firestore(const Json& v) {
  Json o = Json::object();
  switch (v.kind()) {
    case Json::NUL: o.set("nullValue", Json()); break;
    case Json::BOOL: o.set("booleanValue", Json::boolean(v.as_bool())); break;
    case Json::NUM: {
      const double x = v.as_num();
      if (std::isfinite(x) && std::floor(x) == x && std::fabs(x) < 9007199254740992.0) {
        char b[32];
        std::snprintf(b, sizeof(b), "%lld", (long long)x);
        o.set("integerValue", Json::str(b));
      } else {
        o.set("doubleValue", Json::num(x));
      }
      break;
    }
    case Json::STR: o.set("stringValue", v); break;
    case Json::ARR: {
      Json vals = Json::array();
      for (const Json& e : v.elems()) vals.push(to_firestore(e));
      Json av = Json::object();
      if (vals.size()) av.set("values", vals);
      o.set("arrayValue", av);
      break;
    }
    case Json::OBJ: {
      Json fields = Json::object();
      for (const auto& kv : v.items()) fields.set(kv.first, to_firestore(kv.second));
      Json mv = Json::object();
      if (fields.size()) mv.set("fields", fields);
      o.set("mapValue", mv);
      break;
    }
  }
  return o;
}

Json from_firestore(const Json& fv) {
  if (!fv.is_obj() || !fv.size()) return Json();
  const auto& kv = fv.items()[0];
  const std::string& t = kv.first;
  const Json& x = kv.second;
  if (t == "nullValue") return Json();
  if (t == "booleanValue") return Json::boolean(x.as_bool());
  if (t == "integerValue") return Json::num(x.is_str() ? std::strtod(x.as_str().c_str(), nullptr) : x.as_num());
  if (t == "doubleValue") return Json::num(x.is_str() ? std::strtod(x.as_str().c_str(), nullptr) : x.as_num());
  if (t == "stringValue" || t == "timestampValue" || t == "referenceValue") return Json::str(x.as_str());
  if (t == "arrayValue") {
    Json a = Json::array();
    for (const Json& e : x["values"].elems()) a.push(from_firestore(e));
    return a;
  }
  if (t == "mapValue") {
    Json o = Json::object();
    for (const auto& f : x["fields"].items()) o.set(f.first, from_firestore(f.second));
    return o;
  }
  return Json();
}

bool same(const Json& a, const Json& b) {
  if (a.kind() != b.kind()) {
    // (un número entero y el mismo como doble son iguales)
    return false;
  }
  switch (a.kind()) {
    case Json::NUL: return true;
    case Json::BOOL: return a.as_bool() == b.as_bool();
    case Json::NUM: return a.as_num() == b.as_num() || (std::isnan(a.as_num()) && std::isnan(b.as_num()));
    case Json::STR: return a.as_str() == b.as_str();
    case Json::ARR:
      if (a.size() != b.size()) return false;
      for (size_t i = 0; i < a.size(); ++i) if (!same(a[i], b[i])) return false;
      return true;
    case Json::OBJ:
      if (a.size() != b.size()) return false;
      for (const auto& kv : a.items()) { if (!b.has(kv.first.c_str()) || !same(kv.second, b[kv.first.c_str()])) return false; }
      return true;
  }
  return false;
}

}  // namespace web
}  // namespace ps
