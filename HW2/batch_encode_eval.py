import os
import subprocess
import sys

# 設定資料夾與參數
input_dir = 'data/converted_dcm'
results_dir = 'results'
qualities = [10, 30, 60]  # 三組 operating points

os.makedirs(results_dir, exist_ok=True)

# 取得 venv 的 python 執行路徑
python_exec = sys.executable

# 取得所有 DICOM 檔案
files = [f for f in os.listdir(input_dir) if f.lower().endswith('.dcm')]
files.sort()

for q in qualities:
    print(f'=== Quality {q} ===')
    for fname in files:
        name = os.path.splitext(fname)[0]
        in_path = os.path.join(input_dir, fname)
        bin_path = os.path.join(results_dir, f'{name}_q{q}.bin')
        rec_path = os.path.join(results_dir, f'{name}_q{q}_rec.png')
        # encode
        subprocess.run([python_exec, 'encoder.py', '--input', in_path, '--output', bin_path, '--quality', str(q)], check=True)
        # decode
        subprocess.run([python_exec, 'decoder.py', '--input', bin_path, '--output', rec_path], check=True)
        # eval
        subprocess.run([python_exec, 'eval.py', rec_path, in_path], check=True)
