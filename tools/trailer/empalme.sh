#!/usr/bin/env bash
# empalme.sh — cambia la escena del mosaico sin cargar el vídeo entero en memoria: 3 trozos y concat sin recodificar.
set -e
cd /root/trailer
E="-c:v libx264 -preset fast -threads 2 -x264-params rc-lookahead=10 -crf 16 -pix_fmt yuv420p -r 60 -an"
ffmpeg -v error -y -threads 2 -i video_v3_base.mp4 -frames:v 1466 $E t1.mp4
ffmpeg -v error -y -threads 2 -i more_v3.mp4 $E t2.mp4
ffmpeg -v error -y -threads 2 -ss 27.35 -i video_v3_base.mp4 $E t3.mp4
printf "file t1.mp4\nfile t2.mp4\nfile t3.mp4\n" > lista.txt
ffmpeg -v error -y -f concat -safe 0 -i lista.txt -c copy video_sin_audio.mp4
echo VIDEO_OK
