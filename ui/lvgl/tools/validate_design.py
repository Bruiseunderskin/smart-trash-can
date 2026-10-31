#!/usr/bin/env python3
import hashlib,json,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3];UI=ROOT/'ui/lvgl'
t=json.loads((ROOT/'design/tokens.json').read_text());l=json.loads((ROOT/'design/layout.json').read_text());m=json.loads((UI/'generated/manifest.json').read_text())
expected=['idle','capturing','classifying','result_ok','result_retry','error','category_guide','learning','summary']
assert [s['name'][6:]for s in l['screens']]==expected
assert (l['width'],l['height'],l['rotation'],l['color_depth'])==(800,480,0,16)
assert len(t['color'])>=11 and t['font']['cat_px']>=112 and t['font']['body_px']>=32
assert 0<t['motion']['scan_ms']<=300
for path,digest in m['inputs'].items():assert hashlib.sha256((ROOT/path).read_bytes()).hexdigest()==digest, f'Stale generated input: {path}'
def lum(hex):
    rgb=[int(hex[i:i+2],16)/255 for i in [1,3,5]]
    rgb=[c/12.92 if c<=.04045 else((c+.055)/1.055)**2.4 for c in rgb]
    return sum(x*y for x,y in zip(rgb,[.2126,.7152,.0722]))
def contrast(a,b):
    a,b=sorted([lum(t['color'][a]),lum(t['color'][b])]);return(b+.05)/(a+.05)
def walk(n,bg='bg'):
    bg=n.get('background')or bg
    yield n,bg
    for c in n.get('children',[]):yield from walk(c,bg)
checks=[];buttons=0;texts=0
def absolute(n,x=0,y=0,root=False):
    x+=0 if root else n['x'];y+=0 if root else n['y']
    yield n,(x,y,x+n['w'],y+n['h'])
    for c in n.get('children',[]):yield from absolute(c,x,y)
for s in l['screens']:
    backgrounds=[]
    for n,bounds in absolute(s,root=True):
        if n.get('background'):backgrounds.append((n['background'],bounds))
        bg='bg'
        for candidate,area in backgrounds:
            if area[0]<=bounds[0] and area[1]<=bounds[1] and area[2]>=bounds[2] and area[3]>=bounds[3]:bg=candidate
        if n['kind']=='text' or n['kind']=='button':
            fg=n.get('color','fg');ratio=contrast(fg,bg);assert ratio>=4.5,(s['name'],n['name'],fg,bg,ratio)
            checks.append({'state':s['name'],'node':n['name'],'fg':fg,'bg':bg,'contrast':round(ratio,2)});texts+=1
        if n['kind']=='button':assert n['w']>=44 and n['h']>=44;buttons+=1
for i in range(4):assert contrast('cat'+str(i),'surface')>=3
for p in (ROOT/'design/assets/icon').glob('*.svg'):
    assert p.stat().st_size<=2*1024*1024
    assert '<image'not in p.read_text()
for p in (UI/'generated').glob('*.c'):
    binary_bytes=len(re.findall(r'0x[0-9a-fA-F]{2}\b',p.read_text()))
    assert binary_bytes<=2*1024*1024,(p,binary_bytes)
# User-requested shared spacing and post-completion navigation contract.
policy=json.loads((ROOT/'design/spacing-policy.json').read_text())
spacing_checks=0
screens={s['name'][6:]:s for s in l['screens']}
def nodes(s):return [n for n,_ in walk(s)]
def named(s,name):return next(n for n in nodes(s) if n['name']==name)
for state,screen in screens.items():
    for n in nodes(screen):
        if n['name']=='comp/instruction':
            assert (n['x'],n['y'],n['w'],n['h'])==(24,400,752,64);spacing_checks+=1
            text=n['children'][0];assert (text['x'],text['y'],text['w'])==(24,8,704);spacing_checks+=1
        if n['kind']=='button' and n['y']>=400:
            assert n['y']==400 and n['h']==64;spacing_checks+=1
for candidate in [named(screens['result_retry'],'candidate/0'),named(screens['result_retry'],'candidate/1')]:
    icon=next(n for n in candidate['children']if n['kind']=='icon');label=next(n for n in candidate['children']if n['name']=='category')
    assert icon['x']==16 and label['x']-(icon['x']+icon['w'])==24;spacing_checks+=1
for i in range(4):
    screen=screens['category_guide'];card=named(screen,'action/learn-'+str(i));icon=named(screen,'cat/'+str(i));label=named(screen,'cat-label/'+str(i))
    assert icon['x']-card['x']==16 and label['x']-(icon['x']+icon['w'])==24;spacing_checks+=1
for i in range(2):
    card=named(screens['summary'],'comp/summary-'+str(i));label=card['children'][0];value=card['children'][1]
    assert label['x']==24 and label['y']==24 and value['x']==24
    assert value['y']-(label['y']+label['h'])==8 and card['h']-(value['y']+value['h'])==24;spacing_checks+=1
assert not any(n['name']=='action/guide'for n in nodes(screens['idle']))
assert not any(n['name']=='action/start'for key in ['category_guide','learning']for n in nodes(screens[key]))
assert named(screens['learning'],'action/home')['text']=='返回主頁';spacing_checks+=3
report={'states':9,'text_contrast_checks':texts,'touch_targets':buttons,'minimum_contrast':min(c['contrast']for c in checks),'input_hashes':'PASS','svg_embedded_raster':'none','physical_hardware':'NOT_TESTED','contrast':checks,'spacing_checks':spacing_checks,'post_completion_learning':'PASS'}
(UI/'preview/render/design-checks.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print(f'PASS 9 states; {texts} text contrasts >=4.5; {buttons} touch targets >=44x44; {spacing_checks} spacing/navigation checks; generated hashes; asset limits.')
