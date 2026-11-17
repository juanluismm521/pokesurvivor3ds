#!/usr/bin/env bash
# Las mismas tomas de tanda_v1.sh + tanda_v1b.sh, con el juego en inglés (idioma.txt en la SD).
cd /root/trailer
G=/root/trailer/game_v1.3dsx
D=~/.local/share/azahar-emu/sdmc/3ds/pokesurvivor-3ds
touch $D/idioma.txt
./grabar.sh en_v1_eterna "1300 6 1 1188 nivel=45 marca nohalloween" 200 $G
./grabar.sh en_v1_dinamax "95 94 1 60 nivel=35 poder=dinamax poderen=7 marca nohalloween" 120 $G
./grabar.sh en_v1_tera "95 448 1 60 nivel=35 poder=tera poderen=7 marca nohalloween" 120 $G
./grabar.sh en_v1_z "95 25 1 60 nivel=35 poder=z poderen=7 marca nohalloween" 120 $G
./grabar.sh en_v1_mega "95 150 1 60 nivel=35 poder=mega poderen=7 marca nohalloween" 120 $G
./grabar.sh en_v1_pesadilla "960 445 1 900 nivel=40 pesadilla marca nohalloween" 120 $G
./grabar.sh en_v1_semanal "70 25 1 0 semanal marca nohalloween" 120 $G
./grabar.sh en_v1_tienda "tienda nohalloween" 60 $G
./grabar.sh en_v1_gacha "gacha100 nohalloween" 70 $G
./grabar.sh en_v1_novedades "novedades 6 nohalloween" 50 $G
./grabar.sh en_v1_menu "menu nohalloween" 40 $G
rm -f $D/idioma.txt $D/idioma.log
echo TANDA V1 EN HECHA
