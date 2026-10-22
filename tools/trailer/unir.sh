#!/usr/bin/env bash
# unir.sh — vídeo + banda sonora → PokeSurvivor3DS_trailer.mp4 (TikTok: 1080x1920, 60 fps, H.264 + AAC, -14 LUFS)
set -e
cd /root/docker/pokesurvivor/trailer
ffmpeg -v error -y -i video_sin_audio.mp4 -i banda.wav \
  -filter:a "acompressor=threshold=-16dB:ratio=3:attack=8:release=150:makeup=2,loudnorm=I=-14:TP=-1.2:LRA=9" \
  -c:v copy -c:a aac -b:a 192k -ar 48000 -shortest -movflags +faststart PokeSurvivor3DS_trailer.mp4
ffprobe -v error -show_entries format=duration,size -show_entries stream=codec_name,width,height,r_frame_rate PokeSurvivor3DS_trailer.mp4
