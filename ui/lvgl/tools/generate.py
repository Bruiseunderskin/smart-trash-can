#!/usr/bin/env python3
"""Compile Figma-exported geometry, tokens, existing SVGs and OFL fonts to LVGL C."""
import hashlib, json, os, re, subprocess
from pathlib import Path
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont
from PIL import Image

ROOT=Path(__file__).resolve().parents[3]
UI=ROOT/'ui/lvgl'; OUT=UI/'generated'; CACHE=UI/'tools/.cache'
OUT.mkdir(exist_ok=True); CACHE.mkdir(exist_ok=True)
tokens=json.loads((ROOT/'design/tokens.json').read_text()); layout=json.loads((ROOT/'design/layout.json').read_text())
labels=re.findall(r'X\((\d), "([^"]+)"\)',(UI/'include/labels.h').read_text())
assert [int(i) for i,_ in labels]==list(range(4))
def walk(n):
    yield n
    for c in n.get('children',[]):yield from walk(c)
content=json.loads((ROOT/'design/content.json').read_text())
texts=[n.get('text','') for s in layout['screens'] for n in walk(s)] + [c[k] for c in content['categories'] for k in ['example','tip','why','action']]
extra='等待相機畫面連對例子相機未就緒連線中斷請檢查網絡裝置再按重試桶蓋已開請投放等待開蓋請勿推動桶蓋信心值連續答對次上一輪尚無結果目前分數不會自動開蓋'
corpus=''.join(texts)+''.join(x[1] for x in labels)+extra
cjk=''.join(sorted(c for c in set(corpus) if ord(c)>126))
(CACHE/'corpus.txt').write_text(corpus)
converter=UI/'tools/node_modules/lv_font_conv/lv_font_conv.js'
fontspec=[('tc','NotoSansTC.ttf',sorted({tokens['font'][k] for k in ['caption_px','body_px','title_px','cat_px']} | {n['font'] for sc in layout['screens'] for n in walk(sc) if n['kind']=='text' and not n.get('numeric')})),('num','Inter.ttf',sorted({tokens['font']['body_px'],tokens['font']['score_px']} | {n['font'] for sc in layout['screens'] for n in walk(sc) if n['kind']=='text' and n.get('numeric')}))]
for family,file,sizes in fontspec:
    source=ROOT/'design/assets/fonts'/file; font=TTFont(source)
    if 'fvar'in font:font=instantiateVariableFont(font,{'wght':700},inplace=True)
    static=CACHE/(family+'-bold.ttf');font.save(static)
    required=cjk if family=='tc' else ''
    missing=set(map(ord,required))-set(font.getBestCmap());assert not missing, missing
    for size in sizes:
        name=f'stc_font_{family}_{size}';args=['node',str(converter),'--font',str(static),'--size',str(size),'--bpp','4','--format','lvgl','--lv-include','lvgl.h','--lv-font-name',name,'--range','0x20-0x7e','--no-compress','-o',str(OUT/(name+'.c'))]
        # Category font needs category labels only; other sizes include the fixed corpus.
        chars=''.join(x[1] for x in labels) if size==tokens['font']['cat_px'] else required
        if chars:args+=['--symbols',chars]
        subprocess.run(args,check=True,capture_output=True)
        generated=OUT/(name+'.c')
        generated.write_text(generated.read_text().replace(str(ROOT)+'/', '').rstrip()+'\n')

def image_c(name,path,size):
    png=CACHE/(name+'.png');subprocess.run(['node',str(UI/'tools/rasterize.cjs'),str(path),str(png),str(size)],check=True)
    im=Image.open(png).convert('RGBA'); raw=bytes(v for r,g,b,a in im.get_flattened_data() for v in (b,g,r,a))
    lines=[', '.join(f'0x{b:02x}' for b in raw[i:i+24]) for i in range(0,len(raw),24)]
    return f'static const uint8_t {name}_pixels[] = {{\n'+',\n'.join(lines)+f'\n}};\nconst lv_image_dsc_t {name} = {{.header={{.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_ARGB8888,.w={size},.h={size},.stride={size*4}}},.data_size=sizeof({name}_pixels),.data={name}_pixels}};\n'
asset=['#include "stc_layout.h"\n']
for i in range(4):
    for size in [48,96]:asset.append(image_c(f'stc_cat_{i}_{size}',ROOT/f'design/assets/icon/cat-{i}.svg',size))
asset.append(image_c('stc_camera_icon',ROOT/'design/assets/icon/camera.svg',48))
asset.append(image_c('stc_trophy_icon',ROOT/'design/assets/icon/trophy.svg',96))
asset.append('const lv_image_dsc_t *stc_icons[4][2] = {'+','.join('{&stc_cat_%d_48,&stc_cat_%d_96}'%(i,i) for i in range(4))+'};\n')
(OUT/'stc_assets.c').write_text('\n'.join(asset))
header=['#ifndef STC_TOKENS_H','#define STC_TOKENS_H']
for group,values in tokens.items():
    for k,v in values.items():
        if isinstance(v,int):header.append(f'#define STC_{group.upper()}_{k.upper()} {v}')
        elif group=='color':header.append(f'#define STC_COLOR_{k.upper()} 0x{v[1:]}u')
header += [f"#define STC_FONT_BODY stc_font_tc_{tokens['font']['body_px']}",f"#define STC_FONT_CAPTION stc_font_tc_{tokens['font']['caption_px']}",f"#define STC_FONT_NUM_BODY stc_font_num_{tokens['font']['body_px']}",f"#define STC_FONT_SCORE stc_font_num_{tokens['font']['score_px']}"]
header+=['#endif'];(OUT/'stc_tokens.h').write_text('\n'.join(header)+'\n')
code=['/* Generated from design/layout.json exported from Figma. Do not edit. */','#include "stc_layout.h"','#include "stc_tokens.h"']
for family,_,sizes in fontspec:
    for size in sizes:code.append(f'LV_FONT_DECLARE(stc_font_{family}_{size});')
def quoted(s):return json.dumps(s,ensure_ascii=False)
for si,s in enumerate(layout['screens']):
    nodes=[]
    def flatten(n,parent):
        index=len(nodes);nodes.append((n,parent))
        for child in n.get('children',[]):flatten(child,index)
    flatten(s,-1);code.append(f'static const stc_layout_node_t state_{si}[] = {{')
    for n,parent in nodes:
        kind=n['kind'].upper(); bg=n.get('background');fg=n.get('color','fg');num=n.get('numeric',False)
        font=n.get('font',32);family='num'if num else'tc'; fontptr=f'&stc_font_{family}_{font}'if kind=='TEXT'else'&stc_font_tc_32'
        code.append('  {'+f'.kind=STC_NODE_{kind},.parent={parent},.x={n["x"] if parent>=0 else 0},.y={n["y"] if parent>=0 else 0},.w={n["w"]},.h={n["h"]},.radius={n.get("radius",16) if kind!="TEXT"else 0},.name={quoted(n["name"])},.text={quoted(n.get("text",""))},.font={fontptr},.font_px={font},.fg=STC_COLOR_{fg.upper()},.bg=STC_COLOR_{(bg or "bg").upper()},.has_bg={str(bool(bg)).lower()},.asset={int(n.get("asset","cat/-1").split("/")[-1])}'+ '},')
    code.append('};')
code.append('const stc_layout_t stc_layouts[9] = {'+','.join(f'{{state_{i},sizeof(state_{i})/sizeof(state_{i}[0])}}'for i in range(9))+'};')
for key in ['tip','example']:
    code.append('const char *stc_'+key+'s[4] = {'+','.join(quoted(c[key]) for c in content['categories'])+'};')
(OUT/'stc_layout.c').write_text('\n'.join(code)+'\n')
inputs=[ROOT/'design/tokens.json',ROOT/'design/layout.json',ROOT/'design/content.json',ROOT/'design/spacing-policy.json',UI/'include/labels.h']+list((ROOT/'design/assets').rglob('*.svg'))+list((ROOT/'design/assets/fonts').glob('*.ttf'))
manifest={'generator':'lv_font_conv 1.5.3 / FontTools 4.68.0 / resvg 2.6.2','inputs':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()for p in sorted(inputs)},'category_labels':labels,'cjk_glyphs':len(cjk)}
(OUT/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
print(f'Generated {len(layout["screens"])} Figma states, font subsets and 10 existing icon rasters; {len(cjk)} CJK characters.')
