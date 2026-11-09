#!/usr/bin/env python3
"""Fine-tune YOLO11n-cls against the mixed-object wordless experiment."""
import argparse
from pathlib import Path
from ultralytics import YOLO

def main():
    p=argparse.ArgumentParser();p.add_argument('--data',type=Path,required=True);p.add_argument('--weights',type=Path,required=True);p.add_argument('--project',type=Path,required=True);p.add_argument('--name',default='wordless_balanced');p.add_argument('--device',default='mps');a=p.parse_args()
    model=YOLO(str(a.weights));model.train(data=str(a.data),epochs=10,imgsz=160,batch=32,device=a.device,workers=0,project=str(a.project),name=a.name,seed=42,pretrained=True,optimizer='AdamW',lr0=0.00001,warmup_epochs=0,save_period=1,plots=False,verbose=False)
if __name__=='__main__':main()
