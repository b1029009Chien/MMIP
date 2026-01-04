#!/usr/bin/env python3
# decode.py --input out.bin --output rec.png
import argparse, struct, zlib, numpy as np
from PIL import Image

def dct_matrix(N):
    M = np.zeros((N,N), dtype=np.float64)
    for k in range(N):
        for n in range(N):
            alpha = np.sqrt(1.0/N) if k==0 else np.sqrt(2.0/N)
            M[k,n] = alpha * np.cos(np.pi*(2*n+1)*k/(2*N))
    return M

def rle_decode(rle_bytes):
    out = bytearray()
    i=0
    L = len(rle_bytes)
    while i+1 < L:
        cnt = rle_bytes[i]; val = rle_bytes[i+1]
        out += bytes([val])*cnt
        i += 2
    return bytes(out)

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--input', required=True)
    p.add_argument('--output', required=True)
    args=p.parse_args()

    with open(args.input,'rb') as f:
        magic = f.read(4)
        if magic != b'MMPC':
            raise SystemExit('Bad file')
        hdr = f.read(13)  # BHHB B H I total 13 by previous struct
        ver, w, h, bitdepth, B, quant_step, payload_len = struct.unpack('>BHHB B H I', hdr)
        comp = f.read(payload_len)
    rle = zlib.decompress(comp)
    flatbytes = rle_decode(rle)
    import numpy as np
    coeffs = np.frombuffer(flatbytes, dtype=np.int16).astype(np.float64)
    # rebuild image by inverse block DCT
    N=B
    mat = dct_matrix(N)
    nblocks = coeffs.size // (B*B)
    H = ((h + B -1)//B)*B
    W = ((w + B -1)//B)*B
    img = np.zeros((H,W), dtype=np.float64)
    idx = 0
    for bi in range(0,H,B):
        for bj in range(0,W,B):
            q = coeffs[idx:idx+B*B].reshape((B,B))
            idx += B*B
            coeff = q * quant_step
            block = mat.T @ coeff @ mat
            img[bi:bi+B, bj:bj+B] = block
    rec = img[:h,:w]
    # clip to valid range
    maxval = 2**bitdepth - 1
    rec = np.clip(np.round(rec), 0, maxval).astype(np.uint16 if bitdepth>8 else np.uint8)
    Image.fromarray(rec).save(args.output)
    print('Wrote', args.output)

if __name__=='__main__':
    main()
