/* firmar.c — firma de las versiones del juego que publica el VPS
 * (tools/publicar_programa.sh). Ed25519 de Monocypher (con BLAKE2b), la misma
 * que comprueba la consola (core/update). La clave privada NUNCA va al repo:
 * está en /root/.config/pokesurvivor/firma.key.
 *
 *   firmar genera <clave>          crea la clave (32 bytes al azar) y escribe la pública en hex
 *   firmar firma <clave> <texto>   firma el texto (hex de 64 bytes)
 *   firmar huella <archivo>        BLAKE2b de 32 bytes del archivo (hex)
 *   firmar publica <clave>         la clave pública (hex)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "monocypher.h"

static void hex(const uint8_t* b, size_t n) { for (size_t i = 0; i < n; ++i) printf("%02x", b[i]); printf("\n"); }

static int leer_clave(const char* path, uint8_t sk[64], uint8_t pk[32]) {
  FILE* f = fopen(path, "rb");
  if (!f) { fprintf(stderr, "no se puede leer %s\n", path); return 0; }
  uint8_t seed[32];
  const size_t n = fread(seed, 1, 32, f);
  fclose(f);
  if (n != 32) { fprintf(stderr, "clave rota\n"); return 0; }
  crypto_eddsa_key_pair(sk, pk, seed);   /* (borra seed) */
  return 1;
}

int main(int argc, char** argv) {
  if (argc >= 3 && !strcmp(argv[1], "genera")) {
    uint8_t seed[32], sk[64], pk[32];
    FILE* r = fopen("/dev/urandom", "rb");
    if (!r || fread(seed, 1, 32, r) != 32) return 1;
    fclose(r);
    FILE* f = fopen(argv[2], "wb");
    if (!f) return 1;
    fwrite(seed, 1, 32, f);
    fclose(f);
    crypto_eddsa_key_pair(sk, pk, seed);
    hex(pk, 32);
    return 0;
  }
  if (argc >= 3 && !strcmp(argv[1], "publica")) {
    uint8_t sk[64], pk[32];
    if (!leer_clave(argv[2], sk, pk)) return 1;
    hex(pk, 32);
    return 0;
  }
  if (argc >= 4 && !strcmp(argv[1], "firma")) {
    uint8_t sk[64], pk[32], sig[64];
    if (!leer_clave(argv[2], sk, pk)) return 1;
    crypto_eddsa_sign(sig, sk, (const uint8_t*)argv[3], strlen(argv[3]));
    crypto_wipe(sk, 64);
    hex(sig, 64);
    return 0;
  }
  if (argc >= 3 && !strcmp(argv[1], "huella")) {
    FILE* f = fopen(argv[2], "rb");
    if (!f) return 1;
    crypto_blake2b_ctx c;
    crypto_blake2b_init(&c, 32);
    uint8_t buf[65536], h[32];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) crypto_blake2b_update(&c, buf, n);
    fclose(f);
    crypto_blake2b_final(&c, h);
    hex(h, 32);
    return 0;
  }
  fprintf(stderr, "uso: firmar genera|publica|firma|huella ...\n");
  return 2;
}
