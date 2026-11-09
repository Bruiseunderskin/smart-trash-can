#!/usr/bin/env python3
"""Evaluate a saved ESP-PPQ native graph, retaining one row per image."""
import argparse,csv,json
from pathlib import Path
import torch
from torch.utils.data import DataLoader
from ultralytics.data.augment import classify_transforms
from esp_ppq.parser.native import NativeImporter
from esp_ppq.executor.torch import TorchExecutor
from quantize_espdl import image_files,TestDataset

def main():
 p=argparse.ArgumentParser();p.add_argument('--graph',type=Path,required=True);p.add_argument('--data',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
 files,labels,names=image_files(a.data);loader=DataLoader(TestDataset(files,labels,classify_transforms(160)),batch_size=1,shuffle=False,num_workers=0);graph=NativeImporter().build(str(a.graph));executor=TorchExecutor(graph=graph,device='cpu');torch.set_num_threads(2);rows=[];matrix=[[0]*len(names) for _ in names]
 with torch.no_grad():
  for idx,(x,y) in enumerate(loader):
   result=executor(x);result=result[0] if isinstance(result,(list,tuple)) else result;values=result[0].float();pred=int(values.argmax());truth=int(y[0]);matrix[truth][pred]+=1;rows.append({'image':str(files[idx].relative_to(a.data)),'truth':names[truth],'prediction':names[pred],'confidence':float(values[pred]),'correct':int(truth==pred)})
 a.out.parent.mkdir(parents=True,exist_ok=True)
 with a.out.with_suffix('.csv').open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(rows[0]),lineterminator='\n');w.writeheader();w.writerows(rows)
 summary={'backend':'ESP-PPQ native graph CPU simulation (not device execution)','classes':names,'images':len(rows),'correct':sum(r['correct'] for r in rows),'confusion_matrix_rows_truth_columns_prediction':matrix};a.out.with_suffix('.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary))
if __name__=='__main__':main()
