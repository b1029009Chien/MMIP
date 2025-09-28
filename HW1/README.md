# Minimal Image Toolkit (C++)

Pure standard-library C++ image toolbox for basic I/O and resampling.
Supports **BMP** (8-bit indexed & 24-bit BI_RGB) and **RAW (512×512, 8-bit gray)** by default.
Implements **negative**, **log**, **gamma** (via C-style LUTs), and **resize** (**nearest** / **bilinear**).
Output format is chosen by the **output filename extension** (e.g., `.bmp`,`.pgm`).

---

## Features

* **I/O**

  * **BMP** read/write (BI_RGB): 24-bit BGR (RGB in memory) and 8-bit paletted gray.
  * **RAW** read (assumed **512×512**, 8-bit, row-major, grayscale).
  * (Optional) **PNM (PGM/PPM)** write if you kept those functions.
* **Point ops**

  * Negative (`v → 255−v`)
  * Log transform (`s = (255/log 256) * log(1+v)`) via 256-entry LUT
  * Gamma (`s = 255 * (v/255)^γ`) via 256-entry LUT
* **Resampling**

  * Nearest-neighbor (very fast; blocky when upscaling)
  * Bilinear (smoother; slight blur)
  * **Pixel-centered mapping**: `fx = (x+0.5)*sx - 0.5`, `fy = (y+0.5)*sy - 0.5`
* **Output by extension**

  * `.bmp` → BMP writer
  * `.pgm/.ppm` → PNM writer

---

## Build

### Windows

```bash
g++ main.cpp -o main
```

### (Optional) Enable JPEG/PNG input

Standard C++ has no JPEG/PNG decoder. If you want them:

1. Add `stb_image.h` next to your source.
2. Wrap the decoder under a flag (e.g., `-DENABLE_STB`) as you already set up.
Otherwise, convert externally:

```bash
# ImageMagick
magick input.jpg -colorspace RGB output.bmp
```

---

## Usage

### Read / Convert

```bash
# BMP → BMP (no-op write)
./main read  input.bmp  out.bmp

# RAW(512×512 gray) → BMP
./main read  lena.raw   out.bmp
```

### Enhance (point operations)

```bash
# Negative
./main enhance neg    baboon.bmp neg_baboon.bmp

# Log transform
./main enhance log    baboon.bmp log_baboon.bmp

# Gamma (example γ=1.5)
./main enhance gamma  1.5 baboon.bmp gamma_baboon.bmp
```

### Resize (nearest / bilinear)

```bash
# Standard order: <mode> <in> <W> <H> <out>
./main resize nearest  baboon.bmp 256 256 out_nn_256.bmp
./main resize bilinear baboon.bmp 256 256 out_bl_256.bmp

# Also accepted: <mode> <W> <H> <in> <out>
./main resize bilinear 128 128 baboon.bmp out_bl_128.bmp
```
---

## Implementation Highlights

* **Memory layout**: row-major, interleaved channels.
  `offset(i,j,k) = ((i * w) + j) * c + k`
* **BMP row padding**: each scanline padded to 4 bytes.
  `rawBytes = ceil(bpp*w/8); pad = (4 - rawBytes%4)%4; rowSize = rawBytes + pad`
* **Bilinear weights** (per channel):
  Let `x0=floor(fx)`, `x1=x0+1`, `wx=fx-x0` (same for `y`).
  `v0=(1−wx)*F(x0,y0) + wx*F(x1,y0)`
  `v1=(1−wx)*F(x0,y1) + wx*F(x1,y1)`
  `v =(1−wy)*v0       + wy*v1`
* **LUTs**: 256-entry C-arrays for log/gamma; pointer loops to apply per byte.

---
## Limitations

* No built-in JPEG/PNG decoding in the “basic C++ only” build.
* RAW assumed **512×512**, 8-bit grayscale. Change in one place if your RAW differs.
* No color management (sRGB/linear), which is fine for this assignment.
---

**Quick start**

```bash
g++ main.cpp -o main (under the file location)
./main resize bilinear baboon.bmp 256 256 out_bl_256.bmp
./main enhance gamma 2.2 baboon.bmp out_gamma2p2.bmp
```
