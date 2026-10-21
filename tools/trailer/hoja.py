# hoja.py VIDEO INICIO FIN PASO [salida] — hoja de fotogramas (pantalla de arriba)
import sys, subprocess, os
from PIL import Image
v, a, b, st = sys.argv[1], float(sys.argv[2]), float(sys.argv[3]), float(sys.argv[4])
out = sys.argv[5] if len(sys.argv) > 5 else '/tmp/hoja.png'
full = len(sys.argv) > 6
ts = []; t = a
while t <= b + 1e-6: ts.append(t); t += st
ims = []
for t in ts:
    p = subprocess.run(['ffmpeg', '-v', 'error', '-ss', str(t), '-i', v, '-frames:v', '1', '-f', 'image2pipe', '-vcodec', 'png', '-'], capture_output=True).stdout
    import io
    im = Image.open(io.BytesIO(p)).convert('RGB')
    ims.append(im if full else im.crop((0, 0, 400, 240)))
W, H = ims[0].size; cols = 4
sheet = Image.new('RGB', (W * cols, H * ((len(ims) + cols - 1) // cols)))
from PIL import ImageDraw
d = ImageDraw.Draw(sheet)
for i, (im, t) in enumerate(zip(ims, ts)):
    x, y = (i % cols) * W, (i // cols) * H
    sheet.paste(im, (x, y)); d.text((x + 4, y + 4), '%.1f' % t, fill=(255, 255, 0))
sheet.save(out)
