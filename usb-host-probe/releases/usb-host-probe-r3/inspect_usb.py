#!/usr/bin/env python3
"""Read USB device/config/other-speed descriptors only. No claim/detach/reset,
SET_CONFIGURATION, audio streaming, MIDI messages, serial strings or flashing.
"""
import argparse
import ctypes as C
import ctypes.util
import json
from pathlib import Path
import sys


class DeviceDescriptor(C.Structure):
    _fields_=[('length',C.c_uint8),('type',C.c_uint8),('usb',C.c_uint16),
              ('cls',C.c_uint8),('sub',C.c_uint8),('proto',C.c_uint8),('mps0',C.c_uint8),
              ('vid',C.c_uint16),('pid',C.c_uint16),('version',C.c_uint16),
              ('manufacturer',C.c_uint8),('product',C.c_uint8),('serial_index',C.c_uint8),('configs',C.c_uint8)]


def parse_config(raw):
    streams=[];at=0;current=None
    if len(raw)<9 or raw[1] not in (2,7) or int.from_bytes(raw[2:4],'little')!=len(raw):
        raise ValueError('Incomplete configuration descriptor')
    while at<len(raw):
        size=raw[at]
        if size<2 or size>len(raw)-at:raise ValueError('Malformed descriptor length')
        d=raw[at:at+size]
        if d[1]==4 and size>=9:
            current=None
            if d[5]==1 and d[6]==2 and d[3]:
                current=dict(interface=d[2],alternate=d[3],protocol=d[7],endpoints=[])
                streams.append(current)
        elif current and d[1]==0x24 and size>=3:
            if current['protocol']==0 and d[2]==2 and size>=8:
                current.update(channels=d[4],subslot_bytes=d[5],bits=d[6])
                if d[7] and size>=8+3*d[7]:
                    current['rates']=[int.from_bytes(d[i:i+3],'little') for i in range(8,8+3*d[7],3)]
                elif not d[7] and size>=14:current['rate_range']=[int.from_bytes(d[8:11],'little'),int.from_bytes(d[11:14],'little')]
            elif current['protocol']==0x20:
                if d[2]==1 and size>=16:current['channels']=d[10]
                if d[2]==2 and size>=6:current.update(subslot_bytes=d[4],bits=d[5])
        elif current and d[1]==5 and size>=7:
            current['endpoints'].append(dict(address=d[2],direction='IN' if d[2]&128 else 'OUT',
                transfer=d[3]&3,sync=(d[3]>>2)&3,usage=(d[3]>>4)&3,
                max_packet=int.from_bytes(d[4:6],'little'),interval=d[6]))
        at+=size
    return streams


def library():
    path=ctypes.util.find_library('usb-1.0')
    if not path:raise RuntimeError('libusb is unavailable; nothing was opened')
    lib=C.CDLL(path)
    signatures={
        'libusb_init':([C.POINTER(C.c_void_p)],C.c_int),
        'libusb_exit':([C.c_void_p],None),
        'libusb_get_device_list':([C.c_void_p,C.POINTER(C.POINTER(C.c_void_p))],C.c_ssize_t),
        'libusb_free_device_list':([C.POINTER(C.c_void_p),C.c_int],None),
        'libusb_get_device_descriptor':([C.c_void_p,C.POINTER(DeviceDescriptor)],C.c_int),
        'libusb_get_device_speed':([C.c_void_p],C.c_int),
        'libusb_open':([C.c_void_p,C.POINTER(C.c_void_p)],C.c_int),
        'libusb_close':([C.c_void_p],None),
        'libusb_control_transfer':([C.c_void_p,C.c_uint8,C.c_uint8,C.c_uint16,C.c_uint16,C.POINTER(C.c_uint8),C.c_uint16,C.c_uint],C.c_int),
    }
    for name,(args,result) in signatures.items():
        fn=getattr(lib,name);fn.argtypes=args;fn.restype=result
    return lib


def inventory(vendors):
    lib=library();ctx=C.c_void_p();devices=C.POINTER(C.c_void_p)()
    if lib.libusb_init(C.byref(ctx)):raise RuntimeError('libusb initialization failed')
    reports=[]
    try:
        n=lib.libusb_get_device_list(ctx,C.byref(devices))
        if n<0:raise RuntimeError('USB inventory failed')
        try:
            for i in range(n):
                dev=devices[i];dd=DeviceDescriptor()
                if lib.libusb_get_device_descriptor(dev,C.byref(dd)) or dd.vid not in vendors:continue
                speed=lib.libusb_get_device_speed(dev)
                record=dict(vid=f'{dd.vid:04x}',pid=f'{dd.pid:04x}',
                            speed={1:'low',2:'full',3:'high',4:'super',5:'super-plus'}.get(speed,'unknown'),
                            usb_version=f'{dd.usb:04x}',configurations=[])
                reports.append(record);handle=C.c_void_p()
                rc=lib.libusb_open(dev,C.byref(handle))
                if rc:record['read_error']=rc;continue
                try:
                    def get(dtype,index,length):
                        if length>8192:raise ValueError('Descriptor exceeds inspection bound')
                        buf=(C.c_uint8*length)()
                        # The only USB request issued: standard device-to-host GET_DESCRIPTOR.
                        r=lib.libusb_control_transfer(handle,0x80,6,(dtype<<8)|index,0,buf,length,1000)
                        if r<0:raise ValueError('GET_DESCRIPTOR failed: '+str(r))
                        return bytes(buf[:r])
                    for kind in (2,7) if speed==3 else (2,):
                        for index in range(min(dd.configs,4)):
                            item=dict(kind='current' if kind==2 else 'other-speed-full',index=index)
                            try:
                                head=get(kind,index,9)
                                if len(head)!=9:raise ValueError('Short configuration header')
                                raw=get(kind,index,int.from_bytes(head[2:4],'little'))
                                item['streams']=parse_config(raw);item['descriptor_hex']=raw.hex()
                            except ValueError as error:item['error']=str(error)
                            record['configurations'].append(item)
                finally:lib.libusb_close(handle)
                fs=[x for x in record['configurations'] if x['kind']==('current' if speed==2 else 'other-speed-full')]
                candidates=[s for x in fs for s in x.get('streams',[])]
                record['full_speed_source_compatible_with_probe']=(any(
                    s.get('protocol')==0 and s.get('channels')==2 and s.get('bits')==16 and
                    s.get('subslot_bytes')==2 and any(e['direction']=='IN' and e['transfer']==1 and
                        e['usage']==0 and e['interval']==1 and 192<=e['max_packet']<=200 for e in s['endpoints']) and
                    (48000 in s.get('rates',[]) or s.get('rate_range',[1,0])[0]<=48000<=s.get('rate_range',[1,0])[1])
                    for s in candidates) if fs and all('error' not in x for x in fs) else None)
        finally:lib.libusb_free_device_list(devices,1)
    finally:lib.libusb_exit(ctx)
    return dict(schema_version=1,read_only=True,serial_strings_read=False,devices=reports)


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--vendor',action='append',type=lambda s:int(s,16))
    ap.add_argument('--out',type=Path)
    ap.add_argument('--check',action='store_true')
    args=ap.parse_args()
    if args.check:
        print('PASS: parser loaded. No USB library loaded or device accessed.');return
    result=inventory(set(args.vendor or (0x1935,0x1209,0x1397,0x08bb)))
    if args.out:
        if args.out.exists():raise ValueError('Output exists; choose a new path')
        from collect import atomic
        atomic(args.out,result)
    summary=json.loads(json.dumps(result))
    for device in summary['devices']:
        for config in device['configurations']:config.pop('descriptor_hex',None)
    print(json.dumps(summary,indent=2))


if __name__=='__main__':
    try:main()
    except (Exception,KeyboardInterrupt) as error:
        print('Stopped:',str(error),file=sys.stderr);sys.exit(1)
