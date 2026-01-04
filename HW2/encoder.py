#!/usr/bin/env python3
# encode.py --input <dcm_or_png> --output out.bin --quality <q>
import argparse, struct, zlib
import numpy as np
import pydicom
from PIL import Image
import sys

def read_image(path):
    if path.lower().endswith('.dcm'):
        ds = pydicom.dcmread(path)
        arr = ds.pixel_array.astype(np.int32)
        bitdepth = getattr(ds, 'BitsAllocated', 16)
        return arr, bitdepth
    else:
        im = Image.open(path)
        arr = np.array(im).astype(np.int32)
        return arr, 8 if arr.dtype == np.uint8 else 16

# create DCT matrix for N
def dct_matrix(N):
    M = np.zeros((N,N), dtype=np.float64)
    for k in range(N):
        for n in range(N):
            alpha = np.sqrt(1.0/N) if k==0 else np.sqrt(2.0/N)
            M[k,n] = alpha * np.cos(np.pi*(2*n+1)*k/(2*N))
    return M

def block_process(arr, B, mat, quant_step):
    h,w = arr.shape
    # pad
    H = ((h + B -1)//B)*B
    W = ((w + B -1)//B)*B
    pad = np.zeros((H,W), dtype=np.float64)
    pad[:h,:w] = arr
    blocks = []
    for i in range(0,H,B):
        for j in range(0,W,B):
            blk = pad[i:i+B, j:j+B].astype(np.float64)
            coeff = mat @ blk @ mat.T
            q = np.round(coeff / quant_step).astype(np.int16)
            blocks.append(q.flatten())
    return np.concatenate(blocks).astype(np.int16), h, w

def rle_encode(int16arr):
    # simple RLE on bytes of int16 stream
    b = int16arr.tobytes()
    out = bytearray()
    prev = None
    cnt = 0
    for x in b:
        if prev is None:
            prev = x; cnt = 1
        elif x==prev and cnt < 255:
            cnt += 1
        else:
            out += bytes([cnt, prev])
            prev = x; cnt = 1
    if prev is not None:
        out += bytes([cnt, prev])
    return bytes(out)

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--input', required=True)
    p.add_argument('--output', required=True)
    p.add_argument('--quality', type=int, default=30)
    args=p.parse_args()

    arr, bitdepth = read_image(args.input)
    B = 8
    N = B
    mat = dct_matrix(N)
    # map quality -> quant_step (simple mapping)
    quant_step = max(1, int(args.quality))
    coeffs_flat, h, w = block_process(arr, B, mat, quant_step)
    rle = rle_encode(coeffs_flat)
    compressed = zlib.compress(rle, level=6)

    # header: magic(4), ver(1), w(2), h(2), bitdepth(1), block(1), quant_step(2), payload_len(4)
    header = b'MMPC' + struct.pack('>BHHB B H I', 1, w, h, bitdepth, B, quant_step, len(compressed))
    with open(args.output, 'wb') as f:
        f.write(header)
        f.write(compressed)
    print('Wrote', args.output)

if __name__=='__main__':
    main()
