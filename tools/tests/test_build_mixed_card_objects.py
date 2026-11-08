import importlib.util
import tempfile
import unittest
from pathlib import Path

spec=importlib.util.spec_from_file_location('mixed',Path(__file__).parents[1]/'build_mixed_card_objects.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

class MixedDatasetTests(unittest.TestCase):
 def test_composes_physical_and_card_domains(self):
  with tempfile.TemporaryDirectory() as temp:
   root=Path(temp);pt=root/'physical_train';pv=root/'physical_val';cards=root/'cards';out=root/'out'
   for base,name,value in [(pt,'food',b'a'),(pv,'food',b'b'),(cards/'train','1_food',b'c'),(cards/'val','1_food',b'd')]:
    p=base/name;p.mkdir(parents=True);(p/'image.jpg').write_bytes(value)
   self.assertEqual(m.build(pt,pv,cards,out),4)
   self.assertTrue((out/'manifest.csv').exists())
   self.assertEqual(len(list((out/'train'/'food').iterdir())),2)
 def test_rejects_cross_split_identical_content(self):
  with tempfile.TemporaryDirectory() as temp:
   root=Path(temp);pt=root/'pt';pv=root/'pv';cards=root/'cards'
   for base,name in [(pt,'food'),(pv,'food'),(cards/'train','1_food'),(cards/'val','1_food')]:
    p=base/name;p.mkdir(parents=True);(p/'image.jpg').write_bytes(b'same')
   with self.assertRaises(ValueError):m.build(pt,pv,cards,root/'out')
if __name__=='__main__':unittest.main()
