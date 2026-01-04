import os
import json
import csv
import re

def parse_eval_log(logfile):
    result = {}
    with open(logfile, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    for line in lines:
        if 'BitDepth' in line and 'RMSE' in line:
            m = re.search(r'BitDepth (\d+)  RMSE ([\d\.]+)  PSNR ([\d\.]+)', line)
            if m:
                result['bitdepth'] = int(m.group(1))
                result['rmse'] = float(m.group(2))
                result['psnr'] = float(m.group(3))
        if 'Compressed size:' in line:
            result['compressed_size'] = int(re.search(r'(\d+)', line).group(1))
        if 'bpp:' in line:
            result['bpp'] = float(re.search(r'([\d\.]+)', line).group(1))
        if 'Compression ratio:' in line:
            result['compression_ratio'] = float(re.search(r'([\d\.]+)', line).group(1))
        if 'SSIM:' in line:
            result['ssim'] = float(re.search(r'([\d\.]+)', line).group(1))
    return result

results_dir = 'results'
summary = []

for fname in os.listdir(results_dir):
    if fname.endswith('_eval.log'):
        name = fname.replace('_eval.log', '')
        metrics = parse_eval_log(os.path.join(results_dir, fname))
        metrics['name'] = name
        summary.append(metrics)

# 輸出 CSV
with open(os.path.join(results_dir, 'summary.csv'), 'w', newline='', encoding='utf-8') as f:
    writer = csv.DictWriter(f, fieldnames=['name','bitdepth','rmse','psnr','compressed_size','bpp','compression_ratio','ssim'])
    writer.writeheader()
    for row in summary:
        writer.writerow(row)

# 輸出 JSON
with open(os.path.join(results_dir, 'summary.json'), 'w', encoding='utf-8') as f:
    json.dump(summary, f, indent=2, ensure_ascii=False)

print('已產生 summary.csv 與 summary.json')
