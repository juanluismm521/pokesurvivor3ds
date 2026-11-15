// power.h — el poder que eliges al vencer a Eternatus Eternamax (web, Gran Actualización:
// G.Power y _5493e6e). Para el resto de la partida; se usa con Y (como la mega) y se recarga:
//   Megaevolución  40 s con su mega (sólo si tu Pokémon tiene), recarga 90 s; sin megapiedra.
//   Dinamax        12 s gigante: doble de vida, +40 % de daño, cada golpe salpica a los de
//                  alrededor y aplastas a los Pokémon normales que pisas. Recarga 80 s.
//   Pulsera Z      onda expansiva al usarlo; 15 s en que una gran Z de tu tipo rueda hacia los
//                  enemigos y, al acabar, el movimiento Z arrasa la pantalla. Recarga 60 s.
//   Teracristal    30 s con corona de cristal: tus ataques de tu tipo pegan un 80 % más (los
//                  de otros tipos, igual). Recarga 75 s.
// (Del port: el Teracristal es siempre de tu tipo principal y no hay Gigamax; la web deja
// elegir el tipo y tiene las formas Gigamax de 32 Pokémon.)
#pragma once
#include <cstdint>

namespace ps {

struct Game;
struct Player;
struct MoveInst;
struct Enemy;

enum PowerKind : int8_t { PWK_NONE = -1, PWK_MEGA, PWK_DYNA, PWK_Z, PWK_TERA, PWK_COUNT };
struct PowerInfo { const char* name; const char* desc; float dur, cd; uint32_t color; };
extern const PowerInfo kPowerInfo[PWK_COUNT];
extern const char* const kZMoveName[18];   // el movimiento Z de cada tipo

void power_offer(Game& g, Player& p);          // abre la elección (CH_POWER)
void power_choose(Game& g, Player& p, int pick);
bool power_ready(const Player& p);
void power_start(Game& g, Player& p);           // Y
void power_update(Game& g, Player& p, float dt);
float power_out_k(const Player& p, const MoveInst* m);          // × al daño de tus ataques
void power_after_hit(Game& g, Player& p, Enemy& e, float dmg);  // Dinamax: salpica

}  // namespace ps
