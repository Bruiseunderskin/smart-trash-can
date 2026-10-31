#!/usr/bin/env python3
"""Collect fresh native checks/captures; visual review is deliberately separate."""
import argparse,hashlib,json,os,re,subprocess,sys,tempfile,shutil
from pathlib import Path
UI=Path(__file__).resolve().parents[1];ROOT=UI.parents[1];PROJECT=UI/'preview';RENDER=PROJECT/'render'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--visual-review',default='render/visual-review.json',help='reviewed baseline evidence relative to native preview root')
args=parser.parse_args()
review_path=(PROJECT/args.visual_review).resolve()
if not review_path.is_relative_to(PROJECT.resolve()):raise SystemExit('Visual evidence must stay in preview root')
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def run(args,**kw):return subprocess.run([str(a)for a in args],capture_output=True,text=True,check=False,**kw)
state=json.loads((PROJECT/'.lvgl-preview/state.json').read_text());exe=PROJECT/'bin/main';exehash=sha(exe)
result=run([exe,'--self-test'],env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_RENDER_DRIVER':'software'})
(RENDER/'native-tests.log').write_text(result.stdout+result.stderr)
count=re.search(r'PASS (\d+) state/data',result.stdout)
passed=result.returncode==0 and count is not None
native={'executable_sha256':exehash,'checks':{
 'build':{'status':state.get('build_gate','NOT_TESTED'),'details':'native CMake build recorded by preview runner'},
 'layout':{'status':'PASS'if passed else'FAIL','details':'actual LVGL object/text bounds and parent containment for required states and variants'},
 'glyphs':{'status':'PASS'if passed else'FAIL','details':'actual LVGL label codepoints resolve without placeholder glyphs; deliberately missing glyph is rejected'},
 'interaction':{'status':'PASS'if passed else'FAIL','details':f'{count[1] if count else 0} native scenarios; state/actions, post-completion navigation, counters and lifecycle'}},'exit_code':result.returncode,'scenario_count':int(count[1])if count else 0}
(RENDER/'native-checks.json').write_text(json.dumps(native,indent=2)+'\n')
if not passed:raise SystemExit('Native checks failed; read render/native-tests.log')
result=run([sys.executable,UI/'tools/validate_design.py']);print(result.stdout.strip())
if result.returncode:raise SystemExit(result.stdout+result.stderr)
design_path=RENDER/'design-checks.json';design=json.loads(design_path.read_text());design['checks']={'contrast_touch':{'status':'PASS','details':f'{design["text_contrast_checks"]} contrast checks, {design["touch_targets"]} touch targets, {design["spacing_checks"]} spacing/navigation assertions'}};design_path.write_text(json.dumps(design,ensure_ascii=False,indent=2)+'\n')
with tempfile.TemporaryDirectory(prefix='.capture-',dir=RENDER) as temporary:
 result=run([exe,'--screenshots',temporary],env={**os.environ,'SDL_RENDER_DRIVER':'software'})
 if result.returncode:raise SystemExit(result.stdout+result.stderr)
 screenshots=list(Path(temporary).glob('*.bmp'))
 if len(screenshots)!=17:raise SystemExit(f'Expected 17 fresh captures, received {len(screenshots)}')
 for source in screenshots:shutil.copyfile(source,RENDER/source.name)
result=run([sys.executable,UI/'tools/capture.py'])
if result.returncode:raise SystemExit(result.stdout+result.stderr)
layout=json.loads((ROOT/'design/layout.json').read_text());states=[s['name'][6:]for s in layout['screens']]
names=states+[f'result-cat-{i}'for i in range(1,4)]+[f'learning-cat-{i}'for i in range(1,4)]+['result-max','summary-max']
frames=[]
for name in names:
 frame_state='result_ok'if name.startswith(('result-cat-','result-max'))else'learning'if name.startswith('learning-cat-')else'summary'if name=='summary-max'else name
 path=RENDER/(name+'.png');frames.append({'id':name,'state':frame_state,'path':str(path.relative_to(PROJECT)),'sha256':sha(path),'evidence_kind':'framebuffer'})
checks={}
for name in ['build','layout','glyphs','interaction','contrast_touch','visual_review']:
 path=RENDER/'native-checks.json'if name in ['build','layout','glyphs','interaction']else RENDER/'design-checks.json'if name=='contrast_touch'else review_path
 if path.is_file():checks[name]={'evidence':str(path.relative_to(PROJECT)),'sha256':sha(path)}
manifest={'display':state['display'],'required_states':states,'required_frames':names,'frames':frames,'checks':checks}
(RENDER/'render-manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
print(f'Prepared {len(frames)} native captures and {native["scenario_count"]} scenarios. Inspect images and record the selected visual review before evaluation.')
