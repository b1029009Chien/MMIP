# eval.py rec.png orig.dcm
import numpy as np, pydicom, sys
from PIL import Image
def read_any(p):
    if p.endswith('.dcm'):
        ds=pydicom.dcmread(p); return ds.pixel_array.astype(np.float64)
    else:
        return np.array(Image.open(p)).astype(np.float64)
def get_bitdepth(p):
    if p.endswith('.dcm'):
        ds = pydicom.dcmread(p)
        return getattr(ds, 'BitsAllocated', 16)
    else:
        arr = np.array(Image.open(p))
        if arr.dtype == np.uint8:
            return 8
        elif arr.dtype == np.uint16:
            return 16
        else:
            return 16


import os
import matplotlib.pyplot as plt

def eval_one(orig_path, rec_path):
    orig = read_any(orig_path)
    rec = read_any(rec_path)
    B = get_bitdepth(orig_path)
    mse = ((orig - rec) ** 2).mean()
    rmse = np.sqrt(mse)
    psnr = 20 * np.log10((2 ** B - 1) / rmse)
    # --- Rate metrics ---
    if rec_path.endswith('.png'):
        bin_path = rec_path.replace('_rec.png', '.bin').replace('.png', '.bin')
    elif rec_path.endswith('.dcm'):
        bin_path = rec_path.replace('_rec.dcm', '.bin').replace('.dcm', '.bin')
    else:
        bin_path = rec_path + '.bin'
    if not os.path.exists(bin_path):
        bin_path = input(f"請輸入對應壓縮檔路徑（如 results/I0_q30.bin）: ")
    compressed_size = os.path.getsize(bin_path)
    orig_size = orig.size * (B // 8)
    bpp = compressed_size * 8 / orig.size
    compression_ratio = orig_size / compressed_size if compressed_size > 0 else float('inf')
    # --- Qualitative figure ---
    err_map = np.abs(orig - rec)
    err_map_disp = (255 * (err_map / (err_map.max() if err_map.max() > 0 else 1))).astype(np.uint8)
    plt.figure(figsize=(12,4))
    plt.subplot(1,3,1)
    plt.title('Original')
    plt.imshow(orig, cmap='gray')
    plt.axis('off')
    plt.subplot(1,3,2)
    plt.title('Reconstructed')
    plt.imshow(rec, cmap='gray')
    plt.axis('off')
    plt.subplot(1,3,3)
    plt.title('Abs Error (scaled)')
    plt.imshow(err_map_disp, cmap='hot')
    plt.axis('off')
    plt.tight_layout()
    fig_name = f'qualitative_{os.path.splitext(os.path.basename(rec_path))[0]}.png'
    plt.savefig(fig_name, dpi=150)
    plt.close()
    log_lines = []
    log_lines.append(f'質性圖已儲存為 {fig_name}')
    log_lines.append(f'BitDepth {B}  RMSE {rmse:.4f}  PSNR {psnr:.2f} dB')
    log_lines.append(f'Compressed size: {compressed_size} bytes')
    log_lines.append(f'bpp: {bpp:.4f}')
    log_lines.append(f'Compression ratio: {compression_ratio:.2f}')
    for line in log_lines:
        print(line)
    # 自動輸出 log 檔
    log_name = os.path.join('results', f'{os.path.splitext(os.path.basename(rec_path))[0]}_eval.log')
    with open(log_name, 'w', encoding='utf-8') as f:
        for line in log_lines:
            f.write(line + '\n')
    return dict(bitdepth=B, rmse=rmse, psnr=psnr, compressed_size=compressed_size, bpp=bpp, compression_ratio=compression_ratio)

if __name__ == '__main__':
    if len(sys.argv) == 3:
        eval_one(sys.argv[2], sys.argv[1])
    else:
        print('批次模式：請輸入多組 rec.png orig.dcm ...')
        for i in range(1, len(sys.argv)-1, 2):
            print(f'--- 評估 {sys.argv[i+1]} vs {sys.argv[i]} ---')
            eval_one(sys.argv[i+1], sys.argv[i])
