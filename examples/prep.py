import numpy as np
from PIL import Image
from scipy.fft import dctn

a = np.asarray(Image.open("tst.png").convert("L"), dtype=np.float32)

with open("tst.dat", "wb") as f:
    for y in range(0, 1024, 32):
        for x in range(0, 1024, 32):
            b = dctn(a[y:y+32, x:x+32], type=2)
            b = np.clip(np.rint(b/1190 + 128), 0, 255).astype(np.uint8)
            f.write(b.tobytes())
