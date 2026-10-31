import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import shutil
import unittest
from unittest.mock import patch, Mock
from contextlib import redirect_stdout
import io
from PIL import Image
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from evaluate_render import evaluate,digest
from validate_skill import validate
from preview import patch_main,archive_recorded,PreviewError,main as preview_main

class RenderEvalTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.root=Path(self.tmp.name).resolve()
        (self.root/'bin').mkdir();(self.root/'render').mkdir();(self.root/'bin/main').write_bytes(b'native-fixture')
        (self.root/'main.c').write_text('source')
        image=Image.new('RGB',(8,8),'white');image.putpixel((2,2),(0,0,0));image.save(self.root/'render/idle.png')
        self.state={'display':{'width':8,'height':8,'rotation':0,'color_depth':16},'runtime_gate':True,'executable':'bin/main','executable_sha256':digest(self.root/'bin/main'),'source_hashes':{'main.c':digest(self.root/'main.c')}}
        self.manifest={'display':self.state['display'],'required_states':['idle'],'required_frames':['idle'],'frames':[{'id':'idle','state':'idle','path':'render/idle.png','sha256':digest(self.root/'render/idle.png'),'evidence_kind':'framebuffer'}],'checks':{}}
        for name in ('build','layout','glyphs','contrast_touch','interaction','visual_review'):
            report={'executable_sha256':self.state['executable_sha256'],'checks':{name:{'status':'PASS'}},'frame_hashes':{'idle':digest(self.root/'render/idle.png')}}
            path=self.root/'render'/f'{name}.json';path.write_text(json.dumps(report));self.manifest['checks'][name]={'evidence':str(path.relative_to(self.root)),'sha256':digest(path)}
    def result(self):return evaluate(self.root,self.manifest,self.state)
    def test_complete_evidence_passes_without_human_decision(self):
        self.assertEqual(self.result()['result'],'PASS');self.assertNotIn('user_decision',self.result());self.assertFalse(self.result()['hardware_verified'])
    def test_missing_visual_review_is_insufficient(self):
        del self.manifest['checks']['visual_review'];self.assertEqual(self.result()['result'],'INSUFFICIENT_EVIDENCE')
    def test_blank_image_fails_even_with_reported_passes(self):
        Image.new('RGB',(8,8),'black').save(self.root/'render/idle.png');self.manifest['frames'][0]['sha256']=digest(self.root/'render/idle.png');self.assertEqual(self.result()['result'],'FAIL')
    def test_identical_pixels_can_reuse_review_across_png_encodings(self):
        path=self.root/'render/visual_review.json';report=json.loads(path.read_text());report['frame_pixel_hashes']={'idle':hashlib.sha256(Image.open(self.root/'render/idle.png').convert('RGBA').tobytes()).hexdigest()};path.write_text(json.dumps(report));self.manifest['checks']['visual_review']['sha256']=digest(path)
        image=Image.open(self.root/'render/idle.png');image.save(self.root/'render/reencoded.png',compress_level=0);(self.root/'render/idle.png').write_bytes((self.root/'render/reencoded.png').read_bytes());self.manifest['frames'][0]['sha256']=digest(self.root/'render/idle.png')
        self.assertEqual(self.result()['result'],'PASS')
    def test_wrong_dimensions_fail(self):
        self.manifest['display']={**self.state['display'],'width':9};self.assertEqual(self.result()['result'],'FAIL')
    def test_missing_state_fails(self):
        self.manifest['required_states'].append('result');self.assertEqual(self.result()['result'],'FAIL')
    def test_missing_variant_fails(self):
        self.manifest['required_frames'].append('result-max');self.assertEqual(self.result()['result'],'FAIL')
    def test_stale_native_executable_fails(self):
        (self.root/'bin/main').write_bytes(b'changed');self.assertEqual(self.result()['result'],'FAIL')
    def test_stale_configuration_fails(self):
        (self.root/'main.c').write_text('changed');self.assertEqual(self.result()['result'],'FAIL')
    def test_stale_image_fails(self):
        self.manifest['frames'][0]['sha256']='outdated';self.assertEqual(self.result()['result'],'FAIL')
    def test_stale_evidence_fails(self):
        (self.root/'render/layout.json').write_text('{}');self.assertEqual(self.result()['result'],'FAIL')
    def test_layout_failure_is_not_overridden_by_visual_pass(self):
        path=self.root/'render/layout.json';report=json.loads(path.read_text());report['checks']['layout']['status']='FAIL';path.write_text(json.dumps(report));self.manifest['checks']['layout']['sha256']=digest(path);self.assertEqual(self.result()['result'],'FAIL')
    def test_review_must_match_exact_frame(self):
        path=self.root/'render/visual_review.json';report=json.loads(path.read_text());report['frame_hashes']['idle']='old';path.write_text(json.dumps(report));self.manifest['checks']['visual_review']['sha256']=digest(path);self.assertEqual(self.result()['result'],'FAIL')
    def test_path_escape_is_rejected(self):
        self.manifest['frames'][0]['path']='../outside.png';self.assertEqual(self.result()['result'],'FAIL')
    def test_corrupt_png_does_not_pass(self):
        path=self.root/'render/idle.png';path.write_bytes(b'broken PNG');self.manifest['frames'][0]['sha256']=digest(path);self.assertEqual(self.result()['result'],'FAIL')
    def test_display_patch_is_idempotent(self):
        source='sdl_hal_init(800, 480);;\n';first=patch_main(source,800,480,0);self.assertEqual(first,patch_main(first,800,480,0));self.assertNotIn(';;',first)
    def test_replacing_run_never_signals_persisted_pid(self):
        with patch('preview.os.kill')as signal:
            archive_recorded(self.root,{'run_id':'old','process_id':123,'executable':'bin/main'})
            signal.assert_not_called()
            self.assertTrue((self.root/'.lvgl-preview/runs/old.json').is_file())
    def test_cli_completes_without_pending_gate(self):
        (self.root/'.lvgl-preview').mkdir();self.state.update({'run_id':'test','process_id':None,'user_decision':'pending'})
        (self.root/'.lvgl-preview/state.json').write_text(json.dumps(self.state));(self.root/'manifest.json').write_text(json.dumps(self.manifest))
        script=Path(__file__).resolve().parents[1]/'scripts/evaluate_render.py'
        result=subprocess.run([sys.executable,str(script),'--project-root',str(self.root),'--manifest','manifest.json'],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr);state=json.loads((self.root/'.lvgl-preview/state.json').read_text());self.assertEqual(state['completion_status'],'completed');self.assertNotIn('user_decision',state)
    def test_explicit_feedback_is_optional_and_restores_exact_backup(self):
        directory=self.root/'.lvgl-preview';backup=directory/'backups/run';backup.mkdir(parents=True);(backup/'main.c').write_bytes(b'before-preview')
        state={**self.state,'run_id':'test','process_id':None,'backup_dir':'.lvgl-preview/backups/run','touched_files':['main.c'],'evaluation':'PASS','completion_status':'completed'}
        (directory/'state.json').write_text(json.dumps(state))
        script=Path(__file__).resolve().parents[1]/'scripts/record_decision.py'
        result=subprocess.run([sys.executable,str(script),'--project-root',str(self.root),'--decision','NO-GO'],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.assertEqual((self.root/'main.c').read_bytes(),b'before-preview');self.assertEqual(json.loads((directory/'state.json').read_text())['user_feedback'],'NO-GO')
    def test_validator_reports_missing_runner_without_crashing(self):
        skill=self.root/'skill'
        shutil.copytree(Path(__file__).resolve().parents[1],skill,ignore=shutil.ignore_patterns('__pycache__'))
        (skill/'scripts/preview.py').unlink()
        self.assertIn('missing scripts/preview.py',validate(skill))
    def test_first_build_does_not_require_existing_executable(self):
        (self.root/'bin/main').unlink()
        (self.root/'CMakeLists.txt').write_text('project(fixture C)\n')
        (self.root/'main.c').write_text('sdl_hal_init(8, 8);\n')
        (self.root/'lv_conf.h').write_text('#define LV_COLOR_DEPTH 16\n')
        def build(*args, **kwargs):
            (self.root/'bin/main').write_bytes(b'new native build')
            return subprocess.CompletedProcess(args[0], 0, '', '')
        process=Mock(pid=123);process.poll.return_value=None
        argv=['preview.py','--project-root',str(self.root),'--width','8','--height','8']
        with patch('sys.argv',argv),patch('preview.subprocess.run',side_effect=build)as compiler,patch('preview.subprocess.Popen',return_value=process)as launch,patch('preview.time.sleep'),redirect_stdout(io.StringIO()):
            self.assertEqual(preview_main(),0)
        compiler.assert_called_once();launch.assert_called_once()
        state=json.loads((self.root/'.lvgl-preview/state.json').read_text())
        self.assertEqual(state['build_gate'],'PASS');self.assertEqual(state['executable_sha256'],digest(self.root/'bin/main'))
    def test_failed_build_restores_source_and_config(self):
        (self.root/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.20)\nproject(fixture C)\n')
        source='sdl_hal_init(8, 8);\n';config='#define LV_COLOR_DEPTH 16\n';(self.root/'main.c').write_text(source);(self.root/'lv_conf.h').write_text(config)
        script=Path(__file__).resolve().parents[1]/'scripts/preview.py'
        result=subprocess.run([sys.executable,str(script),'--project-root',str(self.root),'--width','16','--height','16','--color-depth','32'],capture_output=True,text=True)
        self.assertNotEqual(result.returncode,0);self.assertEqual((self.root/'main.c').read_text(),source);self.assertEqual((self.root/'lv_conf.h').read_text(),config)
if __name__=='__main__':unittest.main()
