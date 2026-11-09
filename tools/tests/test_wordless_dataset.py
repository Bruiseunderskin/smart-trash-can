"""Guard dataset lineage, deterministic generation, and split isolation."""
import csv,hashlib,importlib.util,tempfile,unittest
from pathlib import Path
from PIL import Image

SPEC=importlib.util.spec_from_file_location('build_wordless',Path(__file__).resolve().parents[1]/'build_wordless_card_dataset.py')
MODULE=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(MODULE)

class WordlessDatasetTests(unittest.TestCase):
    def prepare(self,root):
        (root/'cards').mkdir();rows=[]
        for card,split,color in [('A-01','train',(180,30,30)),('B-01','val',(20,130,30))]:
            path=root/'cards'/f'{card}.png';Image.new('RGB',(100,100),color).save(path);rows.append({'card_id':card,'class':'food','split':split,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
        return rows
    def write_rows(self,root,rows):
        with (root/'cards.csv').open('w',newline='') as f:
            w=csv.DictWriter(f,fieldnames=['card_id','class','split','sha256'],lineterminator='\n');w.writeheader();w.writerows(rows)
    def test_reproducible_and_identity_split(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td);root=p/'cards-source';root.mkdir();rows=self.prepare(root);self.write_rows(root,rows)
            MODULE.build(root,p/'first',2,42);MODULE.build(root,p/'second',2,42)
            first=(p/'first/manifest.csv').read_bytes();self.assertEqual(first,(p/'second/manifest.csv').read_bytes())
            manifest=list(csv.DictReader(first.decode().splitlines()));self.assertEqual(len(manifest),3)
            self.assertEqual({r['card_id'] for r in manifest if r['split']=='train'},{'A-01'});self.assertEqual({r['card_id'] for r in manifest if r['split']=='val'},{'B-01'})
    def test_rejects_cross_split_duplicate(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td);root=p/'cards-source';root.mkdir();rows=self.prepare(root);rows[1]['sha256']=rows[0]['sha256'];self.write_rows(root,rows)
            with self.assertRaisesRegex(ValueError,'across splits'):MODULE.build(root,p/'out',1,42)
if __name__=='__main__':unittest.main()
