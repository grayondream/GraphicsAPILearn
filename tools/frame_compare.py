#!/usr/bin/env python3
import sys, struct
import numpy as np

def read_ppm(path):
    with open(path, 'rb') as f:
        data = f.read()
    if data[:2] != b'P6':
        raise ValueError(f"{path}: not P6")
    parts = []
    idx = 2
    while len(parts) < 3:
        while idx < len(data) and data[idx:idx+1].isspace():
            idx += 1
        if data[idx:idx+1] == b'#':
            while data[idx:idx+1] != b'\n':
                idx += 1
            continue
        start = idx
        while not data[idx:idx+1].isspace():
            idx += 1
        parts.append(int(data[start:idx]))
    idx += 1
    w, h, maxv = parts
    pix = np.frombuffer(data[idx:idx+w*h*3], dtype=np.uint8).reshape(h, w, 3)
    return pix.astype(np.int32)

def compare(a_path, b_path):
    a, b = read_ppm(a_path), read_ppm(b_path)
    if a.shape != b.shape:
        h = min(a.shape[0], b.shape[0]); w = min(a.shape[1], b.shape[1])
        a, b = a[:h,:w], b[:h,:w]
        print(f"  size mismatch, crop to {w}x{h}")
    diff = np.abs(a - b).mean(axis=2)
    mean_diff = diff.mean()
    pct20 = (diff > 20).mean() * 100
    ok = mean_diff < 10 and pct20 < 5
    print(f"  {'PASS' if ok else 'FAIL'} mean={mean_diff:.2f} diff>20={pct20:.2f}%")
    return ok

if __name__ == '__main__':
    dumps = sys.argv[1]
    apps = ["Triangle","Cube","SimpleLight_Diffuse","PBR_Base","Shadow","Defer","SSAO","SkyBox"]
    total = passed = 0
    for app in apps:
        for x, y in [("gl","dx12"),("vulkan","dx12")]:
            pa = f"{dumps}/{app}_{x}.ppm"; pb = f"{dumps}/{app}_{y}.ppm"
            print(f"{app}: {x} vs {y}")
            try:
                total += 1; passed += compare(pa, pb)
            except Exception as e:
                print(f"  ERROR {e}")
    print(f"\n{passed}/{total} PASS")
