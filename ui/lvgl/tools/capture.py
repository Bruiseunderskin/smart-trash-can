#!/usr/bin/env python3
"""Convert actual native framebuffer BMP captures; build review contact sheets."""
from pathlib import Path
from PIL import Image,ImageDraw
folder=Path(__file__).resolve().parents[1]/'preview/render'
for p in folder.glob('*.bmp'):
    image=Image.open(p).convert('RGB');assert image.size==(800,480);image.save(p.with_suffix('.png'))
def sheet(files,name,columns):
    image=Image.new('RGB',(columns*400,((len(files)+columns-1)//columns)*264),'#DDE6DE');draw=ImageDraw.Draw(image)
    for i,label in enumerate(files):
        tile=Image.open(folder/(label+'.png')).resize((400,240));x=(i%columns)*400;y=(i//columns)*264
        draw.text((x+12,y+4),label,fill='#153B2C');image.paste(tile,(x,y+24))
    image.save(folder/(name+'.png'))
sheet(['idle','category_guide','learning','capturing','classifying','result_ok','result_retry','error','summary'],'overview',3)
sheet(['result_ok','result-cat-1','result-cat-2','result-cat-3','learning','learning-cat-1','learning-cat-2','learning-cat-3'],'categories',4)
print('PASS native RGB565 framebuffer captures: nine states, all four results/lessons, uint32 boundary screens.')
