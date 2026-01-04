# Minimal DCT-based Medical Image Codec
**(Human_Skull_2 / example slice I0)**

A compact, reproducible DCT-based image codec for single-slice CT images.  
Implements 8×8 block DCT → scalar quantization (tunable) → RLE (optional) → zlib compression. Includes encoder/decoder scripts, experiment runner, error-visualization utilities and example RD outputs.

---

## Table of contents
- [Quick summary](#quick-summary)
- [Files in this repo](#files-in-this-repo)
- [Requirements & installation](#requirements--installation)
- [Bitstream specification](#bitstream-specification)
- [How to run — Examples](#how-to-run---examples)
- [Batch experiments & summary](#batch-experiments--summary)
- [Output files](#output-files)
- [Notes & troubleshooting](#notes--troubleshooting)
- [How to cite / grading notes](#how-to-cite--grading-notes)
- [License & contact](#license--contact)

---

## Quick summary
**What:** Minimal transform–quantize–entropy codec for 2D CT slices.  
**Why:** Pedagogical baseline to study rate–distortion trade-offs; reproducible scripts that let graders replicate experiments and run ablations. Example dataset: `Human_Skull_2`, example slice `I0`.

---

## Files in this repo
- `encoder.py` — encoder (input: DICOM/PNG; output: `.bin` bitstream).  
- `decoder.py` — decoder (input: `.bin`; output: PNG).   
- `eval.py` — compute rate/distortion metrics (RMSE, PSNR, bpp, compression ratio, SSIM).  
- `batch_encode_eval.py` — batch compress, decompress, and evaluate all images at multiple quality settings.  
- `make_summary.py` — aggregate all evaluation logs into summary CSV/JSON.   
- `README.md` — this file.  
- `eval_results/` — the compare of original and result
- `results/` — expected output location for experiment results (created by scripts). (include `summary.csv` and `summary.json`)

---

## Requirements & installation

### Recommended (Unix-like: Linux / macOS / Windows)
```bash
# create venv
python3 -m venv venv
source venv/bin/activate  # Windows: venv\Scripts\activate

# install python packages
pip install --upgrade pip
pip install numpy pillow matplotlib pydicom

# If your DICOM files use JPEG-Lossless or other compressed pixel formats:
pip install pylibjpeg pylibjpeg-libjpeg pylibjpeg-openjpeg
# Alternatively, for gdcm-based decompression (only if needed)
# pip install gdcm
```

---

## Bitstream specification

- **Header:**  
  - Magic: 4 bytes (`MMPC`)
  - Version: 1 byte
  - Width: 2 bytes
  - Height: 2 bytes
  - Bit depth: 1 byte
  - Block size (B): 1 byte
  - Quantization step: 2 bytes
  - Payload length: 4 bytes

- **Payload:**  
  - All quantized DCT coefficients (blockwise, flattened, int16)
  - RLE-encoded, then zlib-compressed

- **Decoding steps:**  
  1. Read header, extract image and codec parameters
  2. Decompress payload (zlib → RLE decode)
  3. For each block: inverse quantize, inverse DCT
  4. Reconstruct and crop image

---

## How to run — Examples

### Encode (compress)
```bash
python encoder.py --input data/converted_dcm/I0.dcm --output results/I0_q30.bin --quality 30
```

### Decode (decompress)
```bash
python decoder.py --input results/I0_q30.bin --output results/I0_q30_rec.png
```

### Evaluate (metrics)
```bash
python eval.py --input results/I0_q30_rec.png --ref data/converted_dcm/I0.dcm
```
- Will output RMSE, PSNR, bpp, compression ratio, SSIM, and save a log file.

---

## Batch experiments & summary

### Run all images and qualities (e.g., q=10,30,60)
```bash
python batch_encode_eval.py
```
- Will compress, decompress, and evaluate all images in `data/converted_dcm/` at multiple quality settings.
- Results and logs will be saved in `results/`.

### Aggregate results
```bash
python make_summary.py
```
- Collects all *_eval.log files and outputs `summary.csv` and `summary.json` for easy reporting.

---

## Output files

- `results/` — reconstructed images, error maps, and evaluation logs.
- `summary.csv` / `summary.json` — summary of all metrics for all images and qualities.
- `*_rec.png` — reconstructed images.
- `*_err.png` — absolute error maps (for visualization).
- `*_eval.log` — per-image evaluation metrics.

---

## Notes & troubleshooting

- All scripts are pure Python, no external codecs or black-box transforms.
- If you see `ModuleNotFoundError`, ensure you are using the correct venv and have installed all requirements.
- For DICOM decompression errors, try installing `pylibjpeg` or `gdcm` as above.

---

## How to cite / grading notes

- All core transform, quantization, and entropy coding are implemented from scratch.
- No external codec libraries (JPEG, JPEG2000, etc.) are used.
- Bitstream is fully self-contained; no side files required for decoding.
- See report.pdf for bitstream spec, design choices, ablation, and results.

---

## License & contact

MIT License.  
Contact: [chien11407.ai14@nycu.edu.tw]
