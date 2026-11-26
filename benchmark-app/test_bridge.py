#!/usr/bin/env python3
"""Offline wire/bridge recovery fixtures; no MIDI imports."""
import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
HERE=Path(__file__).resolve().parent
sys.path[:0]=[str(HERE),str(HERE.parent/'usb-host-probe'),str(HERE.parent/'usb-host-probe/tests')]
import benchmark_bridge as b
import performance as p
import test_performance as fixtures
import test_collect as audio_fixtures

class Link(fixtures.Link):
    def __init__(self):
        super().__init__(fixtures.fixture());self.valid=0;self.audio=None;self.audio_pending=False;self.token=0;self.lost_audio=False
    def request(self,cmd,data=()):
        if cmd==4:return struct.pack('<4I',77,1 if self.token else 0,self.token,int(self.audio_pending))
        if cmd==1:
            self.token=sum(data[j]<<(7*j) for j in range(4));return b''
        if cmd==2:return self.audio
        if cmd==3:
            self.audio_pending=False
            if self.lost_audio:raise TimeoutError('lost audio ACK')
            return b''
        return super().request(cmd,data)
    def physical_stop(self):
        w=list(struct.unpack('<89I',self.raw[:-4]));w[25:29]=self.pending
        self.raw=struct.pack('<89I',*w);self.raw+=struct.pack('<I',p.zlib.crc32(self.raw));self.valid=1
        self.audio=audio_fixtures.fixture(dict(build=77,rate=48000,gain=25),token=self.token);self.audio_pending=True

class Tests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name);self.link=Link()
        self.ident=dict(build_tag=77,source_sha256='fixture',profile_schema=1)
        self.config=dict(audio=True,sampleRate=48000,duration=10)
    def tearDown(self):self.temp.cleanup()
    def test_arm_idempotent_then_fetch_lost_audio_ack(self):
        b.execute('arm',self.root,self.config,self.ident,self.link,p.c,p);token=self.link.token
        b.execute('arm',self.root,self.config,self.ident,self.link,p.c,p);self.assertEqual(self.link.token,token)
        self.link.physical_stop();self.link.lost_audio=True
        with self.assertRaises(TimeoutError):b.execute('fetch',self.root,self.config,self.ident,self.link,p.c,p)
        self.assertTrue((self.root/'device/COMPLETE').exists());self.assertTrue((self.root/'audio-result.json').exists())
        self.link.lost_audio=False;r=b.execute('fetch',self.root,self.config,self.ident,self.link,p.c,p)
        self.assertEqual(r['audio_snapshot']['run'],token);self.assertEqual(self.link.acks,1)
    def test_audio_arming_reply_lost_can_resume(self):
        original=self.link.request
        def request(cmd,data=()):
            result=original(cmd,data)
            if cmd==1:raise TimeoutError('lost ARM')
            return result
        with patch.object(self.link,'request',request):
            with self.assertRaises(TimeoutError):b.execute('arm',self.root,self.config,self.ident,self.link,p.c,p)
        token=self.link.token;b.execute('arm',self.root,self.config,self.ident,self.link,p.c,p);self.assertEqual(self.link.token,token)
    def test_live_progress_only_acknowledges_after_durable_files(self):
        events=[]
        def event(level,message):
            events.append(message)
            if 'Kvitterar den säkert' in message:
                self.assertTrue((self.root/'device/COMPLETE').exists())
                self.assertIn('profile_raw_hex',p.c.load(self.root/'device/result.json'))
            if 'Kvitterar det säkert' in message:self.assertTrue((self.root/'audio-result.json').exists())
        b.execute('arm',self.root,self.config,self.ident,self.link,p.c,p,event)
        self.link.physical_stop()
        b.execute('fetch',self.root,self.config,self.ident,self.link,p.c,p,event)
        self.assertTrue(any('CPU' in message for message in events))
        self.assertEqual(events[-1],'Profil och loggar är verifierade och sparade.')
    def test_changed_audio_config_refused(self):
        b.execute('arm',self.root,self.config,self.ident,self.link,p.c,p)
        with self.assertRaises(ValueError):b.execute('arm',self.root,dict(self.config,duration=60),self.ident,self.link,p.c,p)
if __name__=='__main__':unittest.main()
