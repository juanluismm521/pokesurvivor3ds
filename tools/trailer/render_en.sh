#!/usr/bin/env bash
# El tráiler en inglés (mismos planos y música; textos de EN en trailer.py).
set -e
cd /root/docker/pokesurvivor/trailer
(cd py && TRAILER_LANG=en ../venv/bin/python trailer.py 0 68.601 ../video_en_sin_audio.mp4)
ffmpeg -v error -y -threads 2 -i video_en_sin_audio.mp4 -i banda.wav \
  -filter:a "acompressor=threshold=-16dB:ratio=3:attack=8:release=150:makeup=2,loudnorm=I=-14:TP=-1.2:LRA=9" \
  -c:v copy -c:a aac -b:a 192k -ar 48000 -shortest -movflags +faststart PokeSurvivor3DS_trailer_en.mp4
ffmpeg -v error -y -threads 2 -i PokeSurvivor3DS_trailer_en.mp4 -c:v libx264 -preset slow -threads 2 -b:v 3000k -pass 1 -passlogfile en2p -an -f null /dev/null
ffmpeg -v error -y -threads 2 -i PokeSurvivor3DS_trailer_en.mp4 -c:v libx264 -preset slow -threads 2 -b:v 3000k -pass 2 -passlogfile en2p -c:a aac -b:a 128k -movflags +faststart PokeSurvivor3DS_trailer_en_ligero.mp4
ls -la PokeSurvivor3DS_trailer_en*.mp4
