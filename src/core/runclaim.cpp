// runclaim.cpp — ver runclaim.h.
#include "core/runclaim.h"

#include <cmath>

#include "core/game.h"
#include "core/gamedata.h"

namespace ps {

Json run_claim(const Game& g) {
  const RunSummary& r = g.summary;
  const Player& p = g.players[g.local];
  Json c = Json::object();
  c.set("time", Json::num(std::floor(double(r.time) * 1000) / 1000));
  c.set("kills", Json::num(p.kills));
  c.set("bosses", Json::num(p.bosses));
  c.set("picked", Json::num(p.coins));
  // Las monedas de la partida (con su multiplicador, la dificultad y el premio de Arceus) y las
  // que fueron directas a la partida guardada en grupo
  c.set("coins", Json::num(r.coins + g.bonus_coins));
  c.set("mul", Json::num(r.coin_mul));
  c.set("t1", Json::num(p.t1));
  c.set("t10", Json::num(p.t10 + r.win_t10 + (r.wk_prize ? 1 : 0)));
  c.set("candy", Json::num(p.candy));
  c.set("level", Json::num(p.level));
  c.set("dex", Json::num(form_web_id(p.dex)));
  c.set("sdex", Json::num(form_web_id(p.start_dex >= 1 ? p.start_dex : p.dex)));
  c.set("chal", Json::boolean(g.cfg.weekly != 0));
  c.set("shinies", Json::num(g.ncaught));
  c.set("coop", Json::boolean(g.nplayers > 1));
  c.set("host", Json::boolean(g.nplayers > 1 && g.local == 0));
  c.set("dead", Json::boolean(r.dead));
  // Los Pokémon conseguidos: rescatados (r) y shinies salvajes (w), con el número de la web
  if (g.ncaught > 0 || g.nrescued > 0) {
    Json got = Json::object(), rr = Json::array(), ww = Json::array();
    for (int i = 0; i < g.nrescued; ++i) rr.push(Json::num(form_web_id(g.rescued_dex[i])));
    for (int i = 0; i < g.ncaught; ++i) ww.push(Json::num(form_web_id(g.caught_dex[i])));
    got.set("r", rr);
    got.set("w", ww);
    c.set("got", got);
  }
  return c;
}

void runq_push(Json& queue, const Json& claim, int64_t end_ms) {
  if (!queue.is_arr()) queue = Json::array();
  Json e = Json::object();
  e.set("id", Json());
  e.set("end", Json::num(double(end_ms)));
  e.set("claim", claim);
  queue.push(e);
  while (queue.size() > size_t(kRunQueueMax)) {   // (las más viejas fuera)
    Json rest = Json::array();
    for (size_t i = 1; i < queue.size(); ++i) rest.push(queue[i]);
    queue = rest;
  }
}

}  // namespace ps
