#!/usr/bin/env python3
import copy
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zlib

HERE=Path(__file__).resolve().parent.parent
spec=importlib.util.spec_from_file_location('collect_probe',HERE/'collect.py')
c=importlib.util.module_from_spec(spec);spec.loader.exec_module(c)


def fixture(m, token=123, unit='canary'):
    v={k:0 for k in c.NAMES}
    v.update(magic=0x31504855,build=m['build'],run=token,rate=m['rate'],gain=m['gain'],
             duration=c.UNITS[unit],elapsed=c.UNITS[unit],state=4,audio_enabled=1,a_id=2,b_id=3,out_id=4,
             a_peak_l=100,a_peak_r=200,b_peak_l=300,b_peak_r=400,a_hash=123,b_hash=456)
    count=c.UNITS[unit]
    v.update(a_packets=count,b_packets=count,out_packets=count,addr_switches=3*count)
    raw=struct.pack('<64I',*(v[k] for k in c.NAMES))+struct.pack('<18I',*([0x1234,1,1,200,1,2,0x1234,2,2,200,1,3,0x1234,3,3,200,2,4]))
    return raw+struct.pack('<I',zlib.crc32(raw))


class ResumeTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)/'run'
        self.ident=dict(source_sha256='fixture',build_tag=77)
        self.m=c.manifest(self.root,self.ident,48000,25)
        self.active=dict(unit='canary',token=123,config_fingerprint=self.m['config_fingerprint'])
    def tearDown(self):self.temp.cleanup()
    def test_commit_before_ack_idempotent(self):
        raw=fixture(self.m)
        a=c.commit_snapshot(self.root,self.m,self.active,raw)
        before=(self.root/'results/canary.json').read_bytes()
        # Fresh process/module state reconstructs progress from the durable record.
        self.assertIn('canary',c.reconcile(self.root,self.m))
        b=c.commit_snapshot(self.root,self.m,self.active,raw)
        self.assertEqual(a,b);self.assertEqual(before,(self.root/'results/canary.json').read_bytes())
        self.assertFalse((self.root/'COMPLETE').exists())
    def test_reject_config_build_identity_and_crc(self):
        with self.assertRaises(ValueError):c.manifest(self.root,self.ident,44100,25)
        raw=fixture(self.m)
        with self.assertRaises(ValueError):c.decode(raw[:-1]+bytes([raw[-1]^1]))
        bad=copy.deepcopy(self.m);bad['build']=99
        with self.assertRaises(ValueError):c.commit_snapshot(self.root,bad,self.active,raw)
        badactive=dict(self.active,token=999)
        with self.assertRaises(ValueError):c.commit_snapshot(self.root,self.m,badactive,raw)
    def test_failed_attempt_does_not_count_as_completed(self):
        raw=bytearray(fixture(self.m));struct.pack_into('<I',raw,5*4,5)
        struct.pack_into('<I',raw,len(raw)-4,zlib.crc32(raw[:-4]))
        r=c.commit_snapshot(self.root,self.m,self.active,bytes(raw))
        self.assertFalse(r['assessment']['bus_pass']);self.assertEqual(c.committed(self.root),{})
        self.assertTrue((self.root/'attempts/123.json').exists())
    def test_interruption_before_summary_and_full_completion(self):
        # Simulate crash after result fsync, before state update; ignore stale state.
        c.commit_snapshot(self.root,self.m,self.active,fixture(self.m))
        c.atomic(self.root/'state.json',{'completed':[]})
        self.assertIn('canary',c.reconcile(self.root,self.m))
        for i,unit in enumerate(('stress','long'),124):
            active=dict(self.active,unit=unit,token=i)
            c.commit_snapshot(self.root,self.m,active,fixture(self.m,i,unit))
        final=c.load(self.root/'final.json');complete=c.load(self.root/'COMPLETE')
        self.assertEqual(complete['sha256'],c.hashlib.sha256(c.canonical(final)).hexdigest())
        self.assertEqual(len(c.committed(self.root)),3)
    def test_model_never_claims_analog_or_audio_integrity(self):
        a=c.assessment(c.decode(fixture(self.m)))
        self.assertTrue(a['bus_pass']);self.assertFalse(a['audio_integrity_verified'])
        self.assertFalse(a['analog_output_verified'])
        self.assertFalse(a['midi_hardware_verified'])
    def test_midi_route_changes_cannot_join_an_old_experiment(self):
        c.commit_snapshot(self.root,self.m,self.active,fixture(self.m))
        active=dict(self.active,unit='stress',token=124)
        for name in ('midi_route_hash','midi_topology_hash'):
            raw=bytearray(fixture(self.m,124,'stress'))
            struct.pack_into('<I',raw,c.NAMES.index(name)*4,1)
            struct.pack_into('<I',raw,len(raw)-4,zlib.crc32(raw[:-4]))
            with self.assertRaises(ValueError):c.commit_snapshot(self.root,self.m,active,bytes(raw))
        self.assertEqual(len(c.committed(self.root)),1)


if __name__=='__main__':
    if len(sys.argv)>1:
        actual=c.decode(Path(sys.argv.pop(1)).read_bytes())
        assert actual['build']==12345 and actual['run']==99 and actual['gain']==100
        assert c.assessment(actual)['bus_pass'] and c.assessment(actual)['input_activity']
        print('PASS: actual C wire snapshot decoded/CRC-checked by Python collector')
    unittest.main()
