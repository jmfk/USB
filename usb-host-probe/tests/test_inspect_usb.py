#!/usr/bin/env python3
import importlib.util
from pathlib import Path
import unittest

path=Path(__file__).resolve().parent.parent/'inspect_usb.py'
spec=importlib.util.spec_from_file_location('inspect_probe',path)
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)


class DescriptorTests(unittest.TestCase):
    def test_uac1_and_other_speed(self):
        raw=bytearray([9,2,43,0,1,1,0,128,50,9,4,1,1,1,1,2,0,0,
             7,36,1,1,1,1,0,11,36,2,1,2,2,16,1,128,187,0,7,5,129,5,200,0,1])
        self.assertEqual(len(raw),43)
        s=m.parse_config(raw)[0]
        self.assertEqual((s['channels'],s['bits'],s['rates']),(2,16,[48000]))
        raw[1]=7;self.assertEqual(m.parse_config(raw),[s])
        with self.assertRaises(ValueError):m.parse_config(raw[:-1])
    def test_uac2_24bit_distinguished(self):
        raw=bytearray([9,2,56,0,1,1,0,128,50,9,4,1,1,1,1,2,32,0,
             16,36,1,1,0,1,1,0,0,0,2,0,0,0,0,0,
             6,36,2,1,3,24,7,5,129,5,38,1,1,
             9,4,2,0,0,1,3,0,0])
        self.assertEqual(len(raw),56)
        s=m.parse_config(raw)[0]
        self.assertEqual((s['protocol'],s['channels'],s['subslot_bytes'],s['bits']),(32,2,3,24))
        self.assertEqual(s['endpoints'][0]['max_packet'],294)


if __name__=='__main__':unittest.main()
