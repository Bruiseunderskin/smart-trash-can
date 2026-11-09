#!/usr/bin/env python3
"""Create deterministic card simulations; keep card identities split before augmentation."""
import argparse,csv,hashlib,random,shutil
from pathlib import Path
from PIL import Image,ImageEnhance,ImageFilter,ImageOps

def build(root: Path,out: Path,count: int,seed: int,physical_data: Path | None = None,physical_repeats: int = 1):
    if out.exists(): raise FileExistsError(f'output exists: {out}')
    with (root/'cards.csv').open() as f: rows=list(csv.DictReader(f))
    generated=[]
    identities=set();hash_splits={}
    for row in rows:
        card_id=row['card_id'];split=row['split'];digest=row['sha256']
        if card_id in identities: raise ValueError(f'duplicate card identity: {card_id}')
        if split not in ('train','val') or row['class'] not in ('food','hazardous','other','recyclable'): raise ValueError(f'invalid card metadata: {card_id}')
        if digest in hash_splits and hash_splits[digest]!=split: raise ValueError(f'identical card image across splits: {card_id}')
        identities.add(card_id);hash_splits[digest]=split
    for row in rows:
        card=root/'cards'/f"{row['card_id']}.png"
        if hashlib.sha256(card.read_bytes()).hexdigest()!=row['sha256']: raise ValueError(f'card hash mismatch: {card}')
        with Image.open(card) as source: img=source.convert('RGB')
        split=row['split'];n=count if split=='train' else 1
        for index in range(n):
            rng=random.Random(f"{seed}:{row['card_id']}:{index}")
            frame=ImageOps.fit(img,(224,224),method=Image.Resampling.LANCZOS)
            if split=='train':
                frame=frame.rotate(rng.uniform(-18,18),Image.Resampling.BICUBIC,fillcolor=(255,255,255))
                scale=rng.uniform(.82,1.0); frame=frame.resize((round(224*scale),round(224*scale)),Image.Resampling.LANCZOS)
                paper=Image.new('RGB',(224,224),(255,255,255));maxpos=224-frame.width;paper.paste(frame,(rng.randint(0,maxpos),rng.randint(0,maxpos)));frame=paper
                frame=ImageEnhance.Brightness(frame).enhance(rng.uniform(.82,1.12));frame=ImageEnhance.Contrast(frame).enhance(rng.uniform(.88,1.12))
                if rng.random()<.20:frame=frame.filter(ImageFilter.GaussianBlur(rng.uniform(.1,.7)))
            name=f"{row['card_id']}_{index:03d}.jpg";dest=out/split/row['class']/name;dest.parent.mkdir(parents=True,exist_ok=True);frame.save(dest,'JPEG',quality=93)
            generated.append({'split':split,'class':row['class'],'card_id':row['card_id'],'source_sha256':row['sha256'],'index':index,'seed':seed,'path':str(dest.relative_to(out)),'sha256':hashlib.sha256(dest.read_bytes()).hexdigest(),'kind':'simulated_card' if split=='train' else 'digital_card'})
    with (out/'manifest.csv').open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(generated[0]),lineterminator='\n');w.writeheader();w.writerows(generated)
    if physical_data is not None:
        with (physical_data/'inventory.csv').open() as f: inventory=list(csv.DictReader(f))
        selected=[r for r in inventory if r['split']=='train' and r['domain']=='physical' and r['availability']=='available']
        with (out/'physical-train.csv').open('w',newline='') as f:
            w=csv.DictWriter(f,fieldnames=['label','dataset_path','sha256'],lineterminator='\n');w.writeheader()
            for r in selected:
                src=physical_data/r['dataset_path'];digest=hashlib.sha256(src.read_bytes()).hexdigest()
                if digest!=r['sha256']:raise ValueError(f'physical hash mismatch: {src}')
                w.writerow({'label':r['label'],'dataset_path':r['dataset_path'],'sha256':digest})
                dst=out/'train'/r['label'];dst.mkdir(parents=True,exist_ok=True)
                for i in range(physical_repeats):
                    prefix='' if i==0 else f'sample{i}_'
                    shutil.copyfile(src,dst/f'{prefix}physical_{digest}.jpg')
        print(f'physical training images: {len(selected)}; repetitions: {physical_repeats}')
    print(f'generated {len(generated)} images; train and validation identities are disjoint')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--cards',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--count',type=int,default=80);p.add_argument('--seed',type=int,default=20261109);p.add_argument('--physical-data',type=Path);p.add_argument('--physical-repeats',type=int,default=1);a=p.parse_args()
    if a.count<1 or a.physical_repeats<1: p.error('counts must be positive')
    build(a.cards,a.out,a.count,a.seed,a.physical_data,a.physical_repeats)
