// app_news.cpp — «Novedades» (SELECT en el menú; se abre sola una vez cuando
// hay novedades nuevas): arriba, un desfile de megas; abajo, la lista de lo
// nuevo, para el jugador. Y las megapiedras de tu compañero en el menú.
#include <3ds.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/mega.h"
#include "core/orbs.h"
#include "core/settings.h"
#include "core/version.h"
#include "platform/3ds/app.h"
#include "platform/3ds/app_ui.h"
#include "platform/3ds/audio3ds.h"

namespace ps {

namespace {

// Ícono de cada novedad.
enum NewsIcon : int8_t { NI_STONE, NI_Y, NI_UP, NI_STAR, NI_HEART, NI_HOME, NI_CLOUD, NI_ITEM, NI_WEB, NI_COIN, NI_BOND, NI_FAST,
                         NI_ORB, NI_TICKET, NI_BOOK, NI_FRIENDS,
                         // (Halloween y lo de la 0.25-0.26: cada novedad con su dibujo)
                         NI_CANDY, NI_PUMPKIN, NI_GENGAR, NI_KECLEON, NI_COSTUME, NI_CARDS, NI_HEALORB, NI_LUMORB };

struct NewsItem {
  const char* head;    // si no es null: título de sección (versión)
  int8_t icon;
  const char* title;
  const char* text;
};

const NewsItem kNews[] = {
    {"Versión 0.26.1 · Grupo más fluido", 0, nullptr, nullptr},
    {nullptr, NI_FRIENDS, "Sin retraso de invitado",
     "Por internet, ahora el servidor junta los botones de todos: van y vuelven por el camino corto, igual para cada jugador. Los que se unen notan mucho menos retraso."},
    {nullptr, NI_HEART, "Si el anfitrión se va, seguís",
     "Por internet, la partida sigue para los demás aunque el anfitrión salga. Y en la sala, el modo rápido se ve bien claro."},
    {"Versión 0.26 · ¡Halloween!", 0, nullptr, nullptr},
    {nullptr, NI_PUMPKIN, "Noche de Halloween",
     "Del 30 de octubre al 2 de noviembre: mapa de noche con calabazas que se rompen, más Pokémon Fantasma y música tenebrosa."},
    {nullptr, NI_CANDY, "Caramelos",
     "Los sueltan los Pokémon Fantasma y las calabazas, al ritmo del tiempo que aguantas. Llegar hasta el gran Gengar da 60 de golpe."},
    {nullptr, NI_KECLEON, "Tienda Kecleon",
     "Arriba a la derecha del menú, con R. En Halloween vende disfraces por caramelos. Próximamente, más cosas en la tienda."},
    {nullptr, NI_COSTUME, "Disfraces de bruja",
     "Gengar, Pikachu y Umbreon con sombrero y capa, para siempre. Se ponen en la tienda o en la Colección (START) y se ven en la partida y en grupo."},
    {nullptr, NI_GENGAR, "El gran Gengar bruja",
     "En la Sentencia llega el gran Gengar en vez de Arceus. Y en el gacha, la Noche Espectral: los Fantasma de todas las regiones, shiny al 2 %."},
    {nullptr, NI_CARDS, "Más pasivas a niveles altos",
     "Mejora nueva (2500): desde el nivel 25 (o con 8 movimientos) eliges entre 4 cartas, y al menos 2 son pasivas; desde el 40, al menos 3."},
    {nullptr, NI_LUMORB, "Tesoro a contrarreloj",
     "El de la Luminosfera brilla más lejos y sólo 40 s: hay que correr. Trae lo de un cofre grande y monedas extra (y en Halloween, caramelo)."},
    {"Versión 0.25 · ¡Región Todas!", 0, nullptr, nullptr},
    {nullptr, NI_HEALORB, "Curaesfera", "Nuevo objeto utilizable, cura 30% al equipo."},
    {nullptr, NI_TICKET, "Región Todas", "Se añade la región Todas al gacha: todas las regiones juntas."},
    {nullptr, NI_STAR, "Gacha", "Se arreglan ciertos bugs del gacha que no aparecían Pokémon representativos."},
    {nullptr, NI_COIN, "Cofres", "Los cofres pequeños se abren al pasar. Los grandes se abren desde un poco más lejos y ya no se llevan tus golpes."},
    {"Versión 0.16 · ¡Derrota a Arceus!", 0, nullptr, nullptr},
    {nullptr, NI_STAR, "Arceus cae en el modo rápido",
     "Sólo en el modo Rápido: tumba a sus 8 legendarios y Arceus baja la guardia. Tiene mucha vida y se enfurece, pero si lo derrotas, la partida acaba en victoria."},
    {nullptr, NI_COIN, "Premio y Medalla de Arceus",
     "Ganar da 2500 Pokémonedas y 3 tickets ×10. Tu Pokémon luce la medalla en la Colección y junto a su nombre en el menú. El modo Normal sigue igual."},
    {nullptr, NI_BOOK, "El juego, en inglés de verdad",
     "Menús, avisos, novedades, créditos, megapiedras y rasgos: todo traducido (antes había textos a medias). Cámbialo en Ajustes → Idioma."},
    {"Versión 0.15 · ¡Esferas y tesoros!", 0, nullptr, nullptr},
    {nullptr, NI_ORB, "Esferas de Mundo Misterioso",
     "Salen en cofres, jefes y a veces al derrotar a un Pokémon. A usa la elegida, B cambia (o tócala abajo): duermen, frenan o empujan a los enemigos, o te dan crítico o velocidad."},
    {nullptr, NI_TICKET, "La Luminosfera y su tesoro",
     "Hace brillar un tesoro escondido cerca. Sigue la flecha dorada (y el punto del minimapa): al llegar suelta tickets y monedas."},
    {nullptr, NI_HOME, "Actualizaciones a la vista",
     "El juego busca la versión nueva al abrirlo y la baja mientras juegas. En el menú verás la barra; START te lleva a Actualizaciones."},
    {nullptr, NI_STAR, "Arceus ya se puede golpear",
     "En la Sentencia lleva una Tabla al azar que le da su tipo. Recibe poco daño y sólo le apuntas sin legendarios cerca. Ellos crecen ahora un 25 %, no el doble."},
    {nullptr, NI_TICKET, "Gacha ×100 y Amuletos Iris",
     "Tira de 100 en 100 (o de 10 en 10). Completa la Pokédex de una región y su Amuleto Iris te da shiny al 2 % en su banner."},
    {nullptr, NI_FRIENDS, "Modo rápido y salas con clave",
     "Jugar: Normal (Sentencia a los 30 min) o Rápido (a los 20). En grupo, la sala elige su modo y puede llevar contraseña (candado en la lista)."},
    {nullptr, NI_UP, "Resumen al acabar",
     "Arriba, el daño y el daño por segundo de cada ataque, el total y tu objeto. En grupo, el de cada compañero con L/R."},
    {nullptr, NI_STONE, "Megas Z y recarga de 40 s",
     "Las megas Z duran 15 s pero empujan más, como en Z-A. Mega-Lucario Z estrena sprite. La recarga de todas baja a 40 s."},
    {nullptr, NI_BOOK, "Ataques en la Colección",
     "SELECT en la Colección: los ataques de cada Pokémon en grande, con su tipo y qué hacen. Darkrai aprende ahora 8 más."},
    {"Versión 0.5 · ¡Ya tenemos web!", 0, nullptr, nullptr},
    {nullptr, NI_WEB, "pokesurvivor3ds.com",
     "La web oficial del juego: descarga el .cia y el .3dsx (con QR para FBI), mira el tráiler y las capturas y aprende a instalarlo."},
    {nullptr, NI_COIN, "Apoya el proyecto",
     "Si te gusta, considera donar en ko-fi.com/juanluismm521: ayuda a mantener los servidores (grupo por internet, sincronización) y las actualizaciones."},
    {nullptr, NI_UP, "Elige tu segundo movimiento",
     "Con la mejora «Repertorio» comprada, pulsa A sobre ella en Mejoras y elige con qué segundo ataque empiezas (o al azar)."},
    {nullptr, NI_BOND, "Vínculos más claros",
     "Al unir dos movimientos, el primero que eliges queda marcado. Púlsalo otra vez (o B) para quitar la marca."},
    {nullptr, NI_FAST, "Arreglado el bajón del minuto 8",
     "Tras unos minutos rompiendo rocas y abriendo cofres por el mapa, el juego se podía hundir (hasta 4 FPS). Arreglado, también en grupo."},
    {nullptr, NI_HOME, "El .cia ya se actualiza bien",
     "La versión nueva ya se instala bien al salir del juego (antes podía dar error al instalarla)."},
    {"Versión 0.3 · ¡Megaevoluciones!", 0, nullptr, nullptr},
    {nullptr, NI_STONE, "¡Llegan las megaevoluciones!",
     "91 megas, también las nuevas de Pokémon Leyendas Z-A: Mega-Raichu X e Y, Mega-Garchomp Z, Mega-Lucario Z, Mega-Absol Z, "
     "Mega-Greninja, Mega-Zeraora…"},
    {nullptr, NI_STONE, "Consigue su megapiedra",
     "Sale en cofres y jefes, siempre la de tu Pokémon y cada una con su dibujo. Si tiene dos megas, puede salir cualquiera."},
    {nullptr, NI_Y, "Pulsa Y para megaevolucionar",
     "20 segundos con su forma y su tipo: más ataque, velocidad y recarga, recuperas vida y aguantas más. Luego, a descansar."},
    {nullptr, NI_UP, "Mejora «Megapiedra»",
     "En Mejoras, para los Pokémon con mega: la megapiedra sale cada vez más a menudo (hasta un 40 %)."},
    {nullptr, NI_STAR, "Shinies y en grupo",
     "Tus shinies megaevolucionan en shiny. Y en grupo cada uno lleva su mega: ¡a ver quién brilla más!"},
    {"Antes", 0, nullptr, nullptr},
    {nullptr, NI_HOME, "Como un juego más",
     "Instálalo en el menú HOME con su icono y su banner. Se actualiza solo, y si algo va mal, vuelves a la versión de antes."},
    {nullptr, NI_CLOUD, "Tu partida de la web",
     "Entra con tu usuario o con Google (con un QR) y juega con la misma partida en la web y en la 3DS."},
    {nullptr, NI_HEART, "Juega en grupo",
     "Partidas con tus amigos, cerca o por internet. Si caes, mira a los demás con L y R."},
    {nullptr, NI_ITEM, "Tu objeto, a la vista",
     "Decide si te quedas el primer objeto y mira en la pantalla de abajo qué hace el que llevas."},
};
constexpr int kNewsN = int(sizeof(kNews) / sizeof(kNews[0]));
constexpr float kRowHead = 20, kRowItem = 56;
constexpr float kTop = 26, kBottom = 214;   // zona de la lista (pantalla de abajo)

float row_h(int i) { return kNews[i].head ? kRowHead : kRowItem; }

// Megas del desfile (las que tienen dibujo propio; primero las de Z-A).
const char* const kParade[] = {"Mega-Garchomp Z", "Mega-Raichu X", "Mega-Charizard X", "Mega-Lucario Z", "Mega-Gengar",
                               "Mega-Absol Z", "Mega-Raichu Y", "Mega-Rayquaza", "Mega-Greninja", "Mega-Charizard Y",
                               "Mega-Mewtwo Y", "Mega-Zeraora", "Mega-Lucario", "Mega-Gardevoir", "Mega-Tyranitar"};
bool is_za(const char* name) {
  static const char* const kZA[] = {"Raichu", " Z", "Chesnaught", "Delphox", "Greninja", "Floette", "Meowstic", "Malamar",
                                    "Barbaracle", "Dragalge", "Hawlucha", "Zygarde", "Crabominable", "Golisopod", "Drampa",
                                    "Magearna", "Zeraora", "Scovillain", "Glimmora", "Tatsugiri", "Baxcalibur", "Darkrai"};
  for (const char* k : kZA) if (std::strstr(name, k)) return true;
  return false;
}
int parade(int16_t* out) {
  int n = 0;
  for (const char* name : kParade)
    for (int k = 0; k < kMegaCount; ++k)
      if (kMegas[k].own_sprite && !std::strcmp(kMegas[k].mon.name, name)) { out[n++] = int16_t(k); break; }
  return n;
}

// Las novedades de ahora: si no se han visto, se abren solas una vez.
constexpr const char* kNewsVersion = "0.26.1";

const uint32_t kRainbow[6] = {0xff5f6d, 0xffd23f, 0x5fe08a, 0x5fc8ff, 0xc47bff, 0xff8ad8};

}  // namespace

bool App::news_unseen() const { return std::strcmp(st_->news_seen, kNewsVersion) != 0; }

void App::open_news() {
  if (news_unseen()) {
    std::snprintf(st_->news_seen, sizeof(st_->news_seen), "%s", kNewsVersion);
    settings_write(dir_, *st_);
  }
  go(SC_NEWS);
}

void App::in_news(const UiIn& in) {
  if (in.rep & KEY_DDOWN) cur_ = std::min(cur_ + 1, kNewsN - 1);
  if (in.rep & KEY_DUP) cur_ = std::max(cur_ - 1, 0);
  if (in.cy < -0.5f && int(t_ * 8) != int((t_ - 1.0f / 60) * 8)) cur_ = std::min(cur_ + 1, kNewsN - 1);
  if (in.cy > 0.5f && int(t_ * 8) != int((t_ - 1.0f / 60) * 8)) cur_ = std::max(cur_ - 1, 0);
  // Que la elegida se vea entera.
  float y = 0, ys = 0;
  for (int i = 0; i < cur_; ++i) y += row_h(i);
  for (int i = 0; i < scroll_; ++i) ys += row_h(i);
  while (scroll_ < cur_ && y + row_h(cur_) - ys > kBottom - kTop) ys += row_h(scroll_++);
  if (cur_ < scroll_) scroll_ = cur_;
  if (cur_ > 0 && kNews[cur_ - 1].head && scroll_ == cur_) scroll_--;   // (con su título)
  if (in.tap && in.ty < 22) { go(SC_MENU); return; }
  if (in.down & (KEY_B | KEY_SELECT | KEY_A | KEY_START)) go(SC_MENU);
}

void App::bottom_news() {
  header("Novedades");
  float y = kTop;
  for (int i = scroll_; i < kNewsN && y + row_h(i) <= kBottom + 4; ++i) {   // (sólo las que caben enteras)
    const NewsItem& n = kNews[i];
    if (n.head) {
      // Título de sección con una raya de colores (la de la mega, sólo arriba).
      r_->text(n.head, 10, y + 3, 0.4f, i == 0 ? C_GOLD : C_MUTED);
      const float x0 = 14 + r_->text_width(n.head, 0.4f), w = 306 - x0;
      if (i == 0) for (int s = 0; s < 6; ++s) C2D_DrawRectSolid(x0 + w * s / 6, y + 10, 0, w / 6, 2, rgb(kRainbow[(s + int(t_ * 4)) % 6]));
      else C2D_DrawRectSolid(x0, y + 10, 0, w, 1, C2D_Color32(60, 70, 96, 255));
      y += kRowHead;
      continue;
    }
    const bool sel = i == cur_;
    const float h = kRowItem - 4;
    C2D_DrawRectSolid(8, y, 0, 304, h, sel ? C_SEL : C_PANEL);
    if (sel) C2D_DrawRectSolid(8, y, 0, 3, h, C_GOLD);
    // Ícono
    const float ix = 28, iy = y + h / 2;
    const float bob = sel ? std::sin(t_ * 4) * 1.5f : 0;
    switch (n.icon) {
      case NI_STONE:
        C2D_DrawCircleSolid(ix, iy, 0, 14 + std::sin(t_ * 3 + i) * 1.5f, rgbf(kRainbow[(i + int(t_ * 2)) % 6], 0.25f));
        // (van pasando todas las megapiedras, cada tarjeta por su lado)
        r_->draw_mega_stone((int(t_ / 1.2f) + i * 37) % kMegaCount, ix - 12, iy - 12 + bob, 24);
        break;
      case NI_Y:
        C2D_DrawCircleSolid(ix, iy + bob, 0, 11, C2D_Color32(40, 50, 70, 255));
        C2D_DrawCircleSolid(ix, iy - 1 + bob, 0, 10, C2D_Color32(95, 200, 120, 255));
        r_->text("Y", ix, iy - 8 + bob, 0.45f, C_TEXT, 1);
        break;
      case NI_ITEM: r_->draw_item_icon(IT_LEFTOVERS, ix - 10, iy - 10 + bob, 20); break;
      case NI_ORB: r_->draw_orb(int(t_ / 1.2f + i) % ORB_COUNT, ix, iy + bob, 10); break;
      case NI_HEALORB: r_->draw_orb(ORB_HEAL, ix, iy + bob, 10); break;
      case NI_LUMORB: r_->draw_orb(ORB_LUMINOUS, ix, iy + bob, 10); break;
      case NI_CANDY: case NI_PUMPKIN: case NI_CARDS: {
        const int ic = r_->ui_icon[n.icon == NI_CANDY ? 16 : n.icon == NI_PUMPKIN ? 17 : 13];
        if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), ix - 12, iy - 12 + bob, 0, nullptr, 2, 2);
        break;
      }
      case NI_GENGAR: case NI_KECLEON: case NI_COSTUME: {
        // (su sprite en pequeño: el gran Gengar bruja, Kecleon con sombrero, Pikachu bruja)
        const char* id = n.icon == NI_GENGAR ? "bruja-gengar" : n.icon == NI_KECLEON ? "hat" : "bruja-pikachu";
        for (int k = 0; k < kSkinCount; ++k)
          if (!std::strcmp(kSkins[k].id, id)) draw_mon(kSkins[k].sprite, false, ix, iy + 13 + bob, n.icon == NI_GENGAR ? 0.9f : 1.1f);
        break;
      }
      default: {
        static const char* const kIc[] = {"", "", "up", "star", "heart", "clock", "", "", "", "coin", "bond", "clock", "", "ticket10", "book", "friends"};
        const int ic = r_->icons.find(kIc[n.icon]);
        if (n.icon == NI_HOME) {   // (la casa: un tejado y una puerta)
          C2D_DrawTriangle(ix - 11, iy - 1 + bob, C_GOLD, ix + 11, iy - 1 + bob, C_GOLD, ix, iy - 11 + bob, C_GOLD, 0);
          C2D_DrawRectSolid(ix - 8, iy - 1 + bob, 0, 16, 11, C_TEXT);
          C2D_DrawRectSolid(ix - 2, iy + 3 + bob, 0, 4, 7, C2D_Color32(120, 80, 50, 255));
        } else if (n.icon == NI_CLOUD) {   // (una nube)
          for (float dx : {-6.0f, 0.0f, 6.0f}) C2D_DrawCircleSolid(ix + dx, iy + 2 - (dx == 0 ? 4 : 0) + bob, 0, dx == 0 ? 7 : 5.5f, C2D_Color32(154, 230, 255, 255));
          C2D_DrawRectSolid(ix - 6, iy + 2 + bob, 0, 12, 5.5f, C2D_Color32(154, 230, 255, 255));
        } else if (n.icon == NI_WEB) {   // (un globo terráqueo)
          C2D_DrawCircleSolid(ix, iy + bob, 0, 11, C2D_Color32(95, 200, 255, 255));
          C2D_DrawCircleSolid(ix, iy + bob, 0, 9, C2D_Color32(40, 120, 200, 255));
          C2D_DrawRectSolid(ix - 11, iy - 0.5f + bob, 0, 22, 1.5f, C2D_Color32(95, 200, 255, 255));
          C2D_DrawRectSolid(ix - 0.75f, iy - 11 + bob, 0, 1.5f, 22, C2D_Color32(95, 200, 255, 255));
          C2D_DrawRectSolid(ix - 9, iy - 5.5f + bob, 0, 18, 1, C2D_Color32(95, 200, 255, 255));
          C2D_DrawRectSolid(ix - 9, iy + 4.5f + bob, 0, 18, 1, C2D_Color32(95, 200, 255, 255));
        } else if (ic >= 0) {
          C2D_DrawImageAt(r_->icons.image(ic), ix - 12, iy - 12 + bob, 0, nullptr, 1.5f, 1.5f);
        }
        break;
      }
    }
    r_->text_fit(n.title, 50, y + 3, 254, 0.42f, n.icon == NI_STONE || n.icon == NI_Y ? C_GOLD : C_TEXT, true);
    r_->text_wrap(n.text, 50, y + 19, 0.3f, C_MUTED, 256);
    y += kRowItem;
  }
  // Barra de desplazamiento
  float total = 0, before = 0;
  for (int i = 0; i < kNewsN; ++i) { total += row_h(i); if (i < scroll_) before += row_h(i); }
  if (total > kBottom - kTop) {
    const float H = kBottom - kTop, bh = H * H / total;
    C2D_DrawRectSolid(314, kTop + H * before / total, 0, 3, bh, C2D_Color32(110, 125, 155, 255));
  }
  r_->text("Arriba/abajo: mover · B o SELECT: volver", 160, 226, 0.32f, C_MUTED, 1);
}

void App::top_news(float eye) {
  (void)eye;
  // Título
  const float ty = 10;
  r_->text("¡NOVEDADES!", 200, ty, 0.95f, C_GOLD, 1);
  const float tw = r_->text_width("¡NOVEDADES!", 0.95f);
  for (int s = 0; s < 6; ++s) C2D_DrawRectSolid(200 - tw / 2 + tw * s / 6, ty + 32, 0, tw / 6, 3, rgb(kRainbow[(s + int(t_ * 5)) % 6]));
  char b[64];
  std::snprintf(b, sizeof(b), "Versión %s · pokesurvivor3ds.com", kVersion);
  r_->text(b, 200, ty + 40, 0.42f, C_TEXT, 1);
  // Desfile de megas: la del centro con su aura y las de los lados en sombra.
  static int16_t list[16];
  static int n = parade(list);
  if (!n) return;
  const float period = 3.0f;
  const int c = int(t_ / period) % n;
  const float ph = std::fmod(t_, period) / period;   // 0..1 de esta
  const float pop = ph < 0.12f ? ph / 0.12f : 1;     // entra creciendo
  const MegaDef& d = kMegas[list[c]];
  const float cx = 200, cy = 196;
  // Aura: anillos de colores que giran y laten.
  for (int i = 0; i < 12; ++i) {
    const float a = t_ * 1.6f + i * 3.14159f / 6, rr = 58 + std::sin(t_ * 3 + i) * 4;
    C2D_DrawCircleSolid(cx + std::cos(a) * rr, cy - 34 + std::sin(a) * rr * 0.55f, 0, 3.5f, rgbf(kRainbow[i % 6], 0.85f));
  }
  C2D_DrawEllipseSolid(cx - 60, cy - 12, 0, 120, 24, rgbf(kRainbow[c % 6], 0.25f + 0.1f * std::sin(t_ * 4)));
  draw_mon(kMegas[list[(c + n - 1) % n]].sprite, false, 82, cy, 1.6f, 3);
  draw_mon(kMegas[list[(c + 1) % n]].sprite, false, 318, cy, 1.6f, 3);
  draw_mon(d.sprite, false, cx, cy, 2.2f * (0.6f + 0.4f * pop), 0, 0);
  if (ph < 0.12f) C2D_DrawCircleSolid(cx, cy - 34, 0, 70 * (1 - pop), C2D_Color32(255, 255, 255, int(180 * (1 - pop))));
  r_->text(d.mon.name, cx, cy + 6, 0.55f, C_TEXT, 1);
  if (is_za(d.mon.name)) {
    const char* za = "NUEVA · Leyendas Z-A";
    const float w = r_->text_width(za, 0.32f) + 10;
    C2D_DrawRectSolid(cx - w / 2, cy + 25, 0, w, 12, C2D_Color32(196, 123, 255, 230));
    r_->text(za, cx, cy + 25, 0.32f, C_TEXT, 1, false);
  }
}

// Menú: las megapiedras de tu compañero (abajo a la derecha, una por mega).
void App::mega_badge(int dex) {
  const int nm = mega_count(dex);
  if (!nm) return;
  const float S = 26, gap = 14, w = std::max(92.0f, nm * S + (nm - 1) * gap + 24), h = 82;
  const float x0 = TW - 8 - w, y0 = 214 - h;
  C2D_DrawRectSolid(x0, y0, 0, w, h, C2D_Color32(14, 18, 32, 190));
  for (int s = 0; s < 6; ++s) C2D_DrawRectSolid(x0 + w * s / 6, y0, 0, w / 6, 2, rgb(kRainbow[(s + int(t_ * 3)) % 6]));
  r_->text(nm > 1 ? "MEGAPIEDRAS" : "MEGAPIEDRA", x0 + w / 2, y0 + 4, 0.3f, C_GOLD, 1);
  for (int i = 0; i < nm; ++i) {
    const int k = mega_index(dex, i);
    const float x = x0 + (w - (nm * S + (nm - 1) * gap)) / 2 + i * (S + gap), y = y0 + 18 + std::sin(t_ * 2.5f + i * 1.7f) * 2;
    C2D_DrawCircleSolid(x + S / 2, y + S / 2, 0, S / 2 + 3 + std::sin(t_ * 3 + i) * 1.5f, rgbf(kRainbow[(i * 3 + int(t_ * 2)) % 6], 0.3f));
    r_->draw_mega_stone(k, x, y, S);
    // Cuál es: X, Y, Z (o nada si sólo tiene una).
    const char* nm_ = k >= 0 ? kMegas[k].mon.name : "";
    const size_t L = std::strlen(nm_);
    if (nm > 1 && L > 2 && nm_[L - 2] == ' ') {
      char l[2] = {nm_[L - 1], 0};
      r_->text(l, x + S, y + S - 12, 0.5f, C_GOLD, 1);
    }
  }
  // La probabilidad de que un objeto sea su megapiedra y la mejora que la sube.
  const int lv = s_->pmega[dex];
  char b[48];
  std::snprintf(b, sizeof(b), "Probabilidad %d %%", int(mega_chance(lv) * 100 + 0.5f));
  r_->text(b, x0 + w / 2, y0 + 49, 0.3f, C_TEXT, 1, false);
  const float pw = kMegaUpMax * 7 - 2, px = x0 + w / 2 - (r_->text_width("Mejora", 0.28f) + 4 + pw) / 2;
  r_->text("Mejora", px, y0 + 63, 0.28f, C_MUTED, 0, false);
  const float qx = px + r_->text_width("Mejora", 0.28f) + 4;
  for (int p = 0; p < kMegaUpMax; ++p) C2D_DrawRectSolid(qx + p * 7, y0 + 67, 0, 5, 5, p < lv ? C_GOLD : C2D_Color32(60, 70, 96, 255));
}

}  // namespace ps
