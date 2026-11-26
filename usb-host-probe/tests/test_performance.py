#!/usr/bin/env python3
import copy
from pathlib import Path
import struct
import subprocess
import signal
import sys
import tempfile
import unittest
from unittest.mock import patch
import zlib
sys.path.insert(0,str(Path(__file__).resolve().parent.parent))
import performance as p


def fixture():
    w=[0]*89
    w[:8]=[0x32445048,77,1,10000,6,9,123,456]
    w[8:12]=[100,200,1000,2000]
    w[19:25]=[42,1000,42000,2,0,0]
    w[25:29]=[1,2,3,4]
    w[29:37]=[0,1,1,1,10000,4,6,9]
    raw=struct.pack('<89I',*w)
    return raw+struct.pack('<I',zlib.crc32(raw))

def profile_fixture(raw=None):
    d=p.decode(raw or fixture());v=d['metrics'];w=[0]*64
    w[:8]=[0x31525048,1,v['build'],v['sequence'],*d['nonce']]
    w[8]=240000;w[10:14]=[24000,0,100,2400];w[14:18]=[12000,0,10,1200];w[18:22]=[2400,0,1,2400]
    w[34:42]=[2400,0,12000,10,0,48000,192,144];w[56:62]=[1,24,v['state'],v['reason'],v['topology_hash'],v['route_hash']]
    b=struct.pack('<64I',*w);return b+struct.pack('<I',zlib.crc32(b))

class Link:
    def __init__(self,raw):self.raw=raw;self.valid=1;self.reads=0;self.acks=0;self.lost=False;self.pending=[0,0,0,0];self.lost_arm=False
    def request(self,cmd,data=()):
        v=p.decode(self.raw)
        if cmd==11:return struct.pack('<8I',77,self.valid,v['metrics']['sequence'],v['crc32'],*(v['nonce'] if self.valid else self.pending))
        if cmd==12:
            self.pending=[sum(data[4*i+j]<<(7*j) for j in range(4)) for i in range(4)]
            if self.lost_arm:raise TimeoutError('arm reply lost')
            return b''
        if cmd==9:self.reads+=1;return self.raw
        if cmd==13:return self.profile if hasattr(self,'profile') else profile_fixture(self.raw)
        if cmd==10:
            self.acks+=1;self.valid=0
            if self.lost:raise TimeoutError('ACK reply lost')
            return b''
        raise AssertionError(cmd)

class Tests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)/'run';self.root.mkdir()
        self.ident=dict(build_tag=77,source_sha256='fixture');self.link=Link(fixture())
    def tearDown(self):self.temp.cleanup()
    def test_profile_metrics_and_identity(self):
        d=p.decode(fixture());v=p.decode_profile(profile_fixture(),d)
        self.assertEqual(v['instrumented_core0_percent'],16)
        self.assertEqual(v['audio']['observed_frame_margin_min_us'],500)
        self.assertEqual(v['audio']['buffer_a_min_us'],4000)
        bad=bytearray(profile_fixture());bad[16]^=1;bad[-4:]=struct.pack('<I',zlib.crc32(bad[:-4]))
        with self.assertRaises(ValueError):p.decode_profile(bytes(bad),d)
    def test_profile_required_before_ack_and_resume(self):
        self.ident['profile_schema']=1;self.link.profile=b'bad'
        with self.assertRaises(ValueError):p.fetch(self.link,self.root,self.ident)
        self.assertEqual(self.link.acks,0)
        self.link.profile=profile_fixture();self.link.lost=True
        with self.assertRaises(TimeoutError):p.fetch(self.link,self.root,self.ident)
        self.assertIn('profile',p.c.load(self.root/'result.json'))
        self.link.lost=False;p.fetch(self.link,self.root,self.ident)
        self.assertEqual(self.link.acks,1)
    def test_profile_crash_between_commit_and_completion(self):
        self.ident['profile_schema']=1;original=p.c.atomic
        def crash(path,data):
            if Path(path).name=='COMPLETE':raise KeyboardInterrupt()
            original(path,data)
        with patch.object(p.c,'atomic',crash):
            with self.assertRaises(KeyboardInterrupt):p.fetch(self.link,self.root,self.ident)
        self.assertEqual(self.link.acks,0)
        p.fetch(self.link,self.root,self.ident);self.assertEqual(self.link.reads,1)
        self.assertEqual(self.link.acks,1)
    def test_rates_and_crc(self):
        v=p.decode(fixture());self.assertEqual(v['rates']['events_per_second'],100);self.assertEqual(v['rates']['fanout_per_second'],200)
        self.assertFalse(v['hardware_verified'])
        with self.assertRaises(ValueError):p.decode(fixture()[:-1]+b'X')
    def test_ack_only_after_durable_completion_and_lost_reply(self):
        self.link.lost=True
        with self.assertRaises(TimeoutError):p.fetch(self.link,self.root,self.ident)
        self.assertTrue((self.root/'COMPLETE').exists());expected=p.validate(self.root)
        self.link.lost=False;self.assertEqual(p.fetch(self.link,self.root,self.ident),expected)
        self.assertEqual(self.link.reads,1);self.assertEqual(self.link.acks,1)
    def test_crash_between_result_and_completion_then_resume(self):
        original=p.c.atomic
        def crash(path,data):
            if Path(path).name=='COMPLETE':raise KeyboardInterrupt()
            original(path,data)
        with patch.object(p.c,'atomic',crash):
            with self.assertRaises(KeyboardInterrupt):p.fetch(self.link,self.root,self.ident)
        self.assertEqual(self.link.acks,0)
        p.fetch(self.link,self.root,self.ident);self.assertEqual(self.link.reads,1)
        p.fetch(self.link,self.root,self.ident);self.assertEqual(self.link.reads,1)
    def test_real_process_termination_after_result_fsync(self):
        for sig in (signal.SIGTERM,signal.SIGKILL):
            root=self.root/str(int(sig));root.mkdir()
            code="""import os,signal,sys;from pathlib import Path
sys.path.insert(0,sys.argv[1]);import performance as p
raw=bytes.fromhex(sys.argv[3]);original=p.c.atomic
def stop(path,data):
 if Path(path).name=='COMPLETE':os.kill(os.getpid(),int(sys.argv[4]))
 original(path,data)
p.c.atomic=stop
p.commit(Path(sys.argv[2]),dict(build_tag=77,source_sha256='fixture'),raw)
"""
            child=subprocess.run([sys.executable,'-c',code,str(Path(p.__file__).parent),str(root),fixture().hex(),str(int(sig))],capture_output=True)
            self.assertEqual(child.returncode,-int(sig));self.assertTrue((root/'result.json').exists())
            link=Link(fixture());p.fetch(link,root,self.ident)
            self.assertEqual(link.reads,0);self.assertEqual(link.acks,1)
            p.fetch(link,root,self.ident);self.assertEqual(link.acks,1)
    def test_changed_run_and_corrupt_local_result_refused(self):
        p.fetch(self.link,self.root,self.ident)
        w=list(struct.unpack('<89I',self.link.raw[:-4]));w[2]=2;raw=struct.pack('<89I',*w)
        self.link.raw=raw+struct.pack('<I',zlib.crc32(raw));self.link.valid=1
        with self.assertRaises(ValueError):p.fetch(self.link,self.root,self.ident)
        self.assertEqual(self.link.acks,1)
        rec=p.c.load(self.root/'result.json');rec['raw_hex']='00';p.c.atomic(self.root/'result.json',rec)
        with self.assertRaises(ValueError):p.validate(self.root)
    def test_arm_resume_readback_and_end_to_end(self):
        self.link.valid=0;self.link.lost_arm=True
        with self.assertRaises(TimeoutError):p.arm(self.link,self.root,self.ident)
        nonce=p.c.load(self.root/'manifest.json')['nonce'];self.assertEqual(nonce,self.link.pending)
        self.link.lost_arm=False;p.arm(self.link,self.root,self.ident)
        self.assertEqual(self.link.reads,0);self.assertEqual(self.link.acks,0)
        w=list(struct.unpack('<89I',self.link.raw[:-4]));w[25:29]=nonce
        raw=struct.pack('<89I',*w);self.link.raw=raw+struct.pack('<I',zlib.crc32(raw));self.link.valid=1
        report=p.fetch(self.link,self.root,self.ident);self.assertEqual(report['nonce'],nonce);self.assertEqual(self.link.acks,1)
    def test_reboot_sequence_collision_nonce_refused(self):
        p.fetch(self.link,self.root,self.ident)
        w=list(struct.unpack('<89I',self.link.raw[:-4]));w[25]+=1;raw=struct.pack('<89I',*w)
        self.link.raw=raw+struct.pack('<I',zlib.crc32(raw));self.link.valid=1
        with self.assertRaises(ValueError):p.fetch(self.link,self.root,self.ident)
        self.assertEqual(self.link.acks,1)
    def test_identity_and_comparison(self):
        with self.assertRaises(ValueError):p.commit(self.root,dict(self.ident,build_tag=99),fixture())
        a=p.decode(fixture());b=copy.deepcopy(a);b['metrics']['route_hash']=99
        with self.assertRaises(ValueError):p.compare(a,b)
        self.assertEqual(p.compare(a,a)['builds'],[77,77])

if __name__=='__main__':
    if len(sys.argv)>1:
        fixture_path=Path(sys.argv.pop(1));actual=p.decode(fixture_path.read_bytes())
        prof=p.decode_profile(Path(str(fixture_path).removesuffix('.performance')+'.profile').read_bytes(),actual)
        assert prof['build']==12345 and prof['schema_version']==1
        assert actual['metrics']['build']==12345 and actual['metrics']['log_dropped']>0
        assert actual['metrics']['service_max_us']==42
        print('PASS: actual C diagnostic SysEx fixture decoded/CRC-checked')
    unittest.main()
