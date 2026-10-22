# detectar.py NOMBRE… — segundo del vídeo en que empieza la megaevolución (el nombre de abajo se alarga con «Mega-»)
import sys, subprocess, numpy as np
for n in sys.argv[1:]:
    p = subprocess.run(['ffmpeg', '-v', 'error', '-i', '/root/docker/pokesurvivor/trailer/raw/%s.mkv' % n, '-vf', 'crop=200:16:44:246',
                        '-f', 'rawvideo', '-pix_fmt', 'gray', '-'], capture_output=True)
    a = np.frombuffer(p.stdout, np.uint8).reshape(-1, 16, 200)
    bright = a > 180
    cols = bright.any(1)
    width = np.array([np.nonzero(c)[0].max() if c.any() else 0 for c in cols])
    base = np.median(width[60:300])
    hit = np.nonzero(width > base + 25)[0]
    hit = hit[hit > 300]
    print(n, 'ancho normal %d' % base, '-> mega en %.3f s' % (hit[0] / 60) if len(hit) else 'sin mega')
