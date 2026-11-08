#!/usr/bin/env python3
"""Compose local physical and fixed-card train/validation folders; never overwrite inputs."""
import argparse
import csv
import hashlib
import os
from pathlib import Path

LABELS=('food','hazardous','other','recyclable')
ALIASES={'0_recyclable':'recyclable','1_food':'food','2_hazardous':'hazardous','3_other':'other'}

def build(physical_train,physical_val,cards,out):
    if out.exists(): raise FileExistsError(out)
    inputs=[('train','physical',physical_train),('train','simulated_card',cards/'train'),('val','physical',physical_val),('val','simulated_card',cards/'val')]
    seen={}; rows=[]
    for split,domain,root in inputs:
        if not root.is_dir(): raise FileNotFoundError(root)
        for folder in sorted(root.iterdir()):
            if not folder.is_dir(): continue
            label=ALIASES.get(folder.name,folder.name)
            if label not in LABELS: continue
            for p in sorted(folder.iterdir()):
                if not p.is_file() or p.suffix.lower() not in ('.jpg','.jpeg','.png'):continue
                digest=hashlib.sha256(p.read_bytes()).hexdigest(); old=seen.get(digest)
                if old is not None and old!=(split,label):raise ValueError(f'cross-split or label duplicate: {p}')
                seen[digest]=(split,label)
                dest=out/split/label/(domain+'_'+digest[:20]+p.suffix.lower());dest.parent.mkdir(parents=True,exist_ok=True)
                if dest.exists():continue
                os.link(p,dest)
                rows.append({'split':split,'domain':domain,'label':label,'source_name':p.name,'sha256':digest,'file':str(dest.relative_to(out))})
    if not rows:raise ValueError('no input images')
    with (out/'manifest.csv').open('w',newline='') as f:
        writer=csv.DictWriter(f,fieldnames=rows[0]);writer.writeheader();writer.writerows(rows)
    return len(rows)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for option in ('physical-train','physical-val','cards','out'):p.add_argument('--'+option,type=Path,required=True)
    a=p.parse_args(); print(f'Composed {build(a.physical_train,a.physical_val,a.cards,a.out)} images')
if __name__=='__main__':main()
