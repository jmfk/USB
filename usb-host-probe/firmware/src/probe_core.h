/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>

#define HP_RING 1024u
#define HP_MAX_PACKET 200u
#define HP_FIELDS 64u
enum { HP_IDLE, HP_ARMED, HP_ENUM, HP_RUNNING, HP_COMPLETE, HP_FAILED, HP_STOPPED, HP_READY };
enum { H_MAGIC, H_BUILD, H_RUN, H_RATE, H_DURATION, H_STATE, H_REASON, H_ELAPSED,
       H_SOF, H_MISSED, H_LATE, H_ADDR_SWITCHES, H_MAX_US, H_IO_ERRORS, H_ENUM_DEVICES,
       H_REJECTED, H_A_PACKETS, H_A_FRAMES, H_A_ERRORS, H_A_MIN, H_A_MAX, H_A_UNDER,
       H_A_OVER, H_A_PEAK_L, H_A_PEAK_R, H_A_HASH, H_A_FIRST, H_A_LAST,
       H_B_PACKETS, H_B_FRAMES, H_B_ERRORS, H_B_MIN, H_B_MAX, H_B_UNDER, H_B_OVER,
       H_B_PEAK_L, H_B_PEAK_R, H_B_HASH, H_B_FIRST, H_B_LAST, H_OUT_PACKETS,
       H_OUT_FRAMES, H_OUT_ERRORS, H_OUT_PEAK, H_OUT_HASH, H_A_ID, H_B_ID, H_OUT_ID, H_GAIN,
       H_MIDI_INPUTS,H_MIDI_OUTPUTS,H_MIDI_RX,H_MIDI_TX,H_MIDI_EVENTS,H_MIDI_FANOUT,
       H_MIDI_IDLE,H_MIDI_RETRIES,H_MIDI_INVALID,H_MIDI_PRESSURE,H_MIDI_FAULT,H_MIDI_CLEANUP,
       H_AUDIO_ENABLED,H_MIDI_ROUTE_HASH,H_MIDI_TOPOLOGY_HASH };
enum { HP_OK, HP_NO_TRIPLE, HP_IO, HP_DEADLINE, HP_TRANSFER, HP_NO_SOF,
       HP_BUFFER, HP_UNARMED, HP_ENUM_ERROR, HP_INCOMPLETE, HP_MIDI_ERROR, HP_NO_MIDI };

typedef struct {
    uint8_t addr, mps0, cfg, iface, alt, ep, sync, interval, rate_control, fixed_rate;
    uint16_t maxp, vid, pid, path;
} hp_stream_t;
typedef struct { hp_stream_t in, out; uint8_t addr, cfg, mps0; uint16_t vid, pid, path; } hp_device_t;
typedef struct {
    uint32_t w, r;
    int16_t pcm[HP_RING][2];
} hp_ring_t;
static hp_ring_t hp_ring[2];
static hp_device_t hp_devices[16];
static hp_stream_t hp_stream[3];
static uint32_t hp_v[HP_FIELDS];
static uint32_t hp_primed, hp_fraction, hp_gain = 0;
static uint8_t hp_audio_enabled=1,hp_defer_run;
#include "midi_router.h"

static uint32_t hp_abs(int32_t a) { return (uint32_t)(a < 0 ? -a : a); }
static uint32_t hp_hash(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }
static uint32_t hp_base(uint32_t i) { return i ? H_B_PACKETS : H_A_PACKETS; }
static uint32_t hp_stream_id(const hp_stream_t *s)
{ return (uint32_t)s->addr | (uint32_t)s->ep << 8 | (uint32_t)s->iface << 16 | (uint32_t)s->alt << 24; }

/* Only a complete, bounded UAC1 Type-I PCM16 stereo alternate with a 1ms data
 * endpoint is admitted. An async OUT is intentionally unsupported in revision 1. */
static int hp_parse(const uint8_t *p, uint32_t n, uint32_t rate, hp_device_t *dev)
{
    hp_stream_t cur = {0};
    uint32_t at = 0, in_as = 0, pcm = 0, format = 0, good_rate = 0;
    if (n < 9 || p[1] != 2 || p[0] != 9 || (uint32_t)(p[2] | p[3] << 8) != n) return 0;
    dev->cfg = p[5];
    while (at < n) {
        const uint8_t *d = p + at;
        uint32_t l = d[0], j;
        if (l < 2 || l > n - at) return 0;
        if (d[1] == 4 && l >= 9) {
            cur = (hp_stream_t){.addr=dev->addr,.mps0=dev->mps0,.cfg=dev->cfg,
                .iface=d[2],.alt=d[3],.vid=dev->vid,.pid=dev->pid,.path=dev->path};
            in_as = d[3] && d[5] == 1 && d[6] == 2 && d[7] == 0;
            pcm = format = good_rate = 0;
        } else if (in_as && d[1] == 0x24 && l >= 3) {
            if (d[2] == 1 && l >= 7) pcm = d[5] == 1 && d[6] == 0;
            if (d[2] == 2 && l >= 8) {
                format = d[3] == 1 && d[4] == 2 && d[5] == 2 && d[6] == 16;
                if (!d[7] && l >= 14) {
                    uint32_t lo=d[8]|(uint32_t)d[9]<<8|(uint32_t)d[10]<<16;
                    uint32_t hi=d[11]|(uint32_t)d[12]<<8|(uint32_t)d[13]<<16;
                    good_rate = rate >= lo && rate <= hi;cur.fixed_rate=lo==hi;
                } else if (d[7] && l >= 8u + 3u*d[7]) {
                    cur.fixed_rate=d[7]==1;
                    for (j=0; j<d[7]; j++) {
                        const uint8_t *f=d+8+3*j;
                        if ((f[0]|(uint32_t)f[1]<<8|(uint32_t)f[2]<<16) == rate) good_rate=1;
                    }
                }
            }
        } else if(in_as && d[1]==0x25 && l>=4 && d[2]==1) {
            hp_stream_t *s=(cur.ep&128)?&dev->in:&dev->out;
            if(s->ep==cur.ep && s->iface==cur.iface && s->alt==cur.alt) s->rate_control=d[3]&1;
        } else if (in_as && pcm && format && good_rate && d[1] == 5 && l >= 7) {
            uint32_t maxp=d[4]|(uint32_t)d[5]<<8, sync=(d[3]>>2)&3;
            if ((d[3]&3) == 1 && !(d[3]&0x30) && (d[2]&15) && !(d[2]&0x70) &&
                d[6] == 1 && maxp >= ((rate+999)/1000)*4 && maxp <= HP_MAX_PACKET && !(maxp&3)) {
                cur.ep=d[2];cur.sync=(uint8_t)sync;cur.maxp=(uint16_t)maxp;cur.interval=1;
                if ((d[2]&128) && !dev->in.ep) dev->in=cur;
                if (!(d[2]&128) && sync >= 2 && (l<9 || !d[8]) && !dev->out.ep) dev->out=cur;
            }
        }
        at += l;
    }
    if(dev->in.ep && !dev->in.rate_control && !dev->in.fixed_rate)dev->in.ep=0;
    if(dev->out.ep && !dev->out.rate_control && !dev->out.fixed_rate)dev->out.ep=0;
    return dev->in.ep || dev->out.ep;
}

static int hp_choose(uint32_t count)
{
    uint32_t a,b,c;
    for(a=0;a<count;a++) for(b=a+1;b<count;b++) for(c=0;c<count;c++)
        if(c!=a && c!=b && hp_devices[a].in.ep && hp_devices[b].in.ep && hp_devices[c].out.ep &&
           hp_devices[a].addr && hp_devices[b].addr && hp_devices[c].addr &&
           hp_devices[a].addr!=hp_devices[b].addr && hp_devices[a].addr!=hp_devices[c].addr &&
           hp_devices[b].addr!=hp_devices[c].addr) {
            hp_stream[0]=hp_devices[a].in;hp_stream[1]=hp_devices[b].in;hp_stream[2]=hp_devices[c].out;
            hp_v[H_A_ID]=hp_stream_id(&hp_stream[0]);hp_v[H_B_ID]=hp_stream_id(&hp_stream[1]);
            hp_v[H_OUT_ID]=hp_stream_id(&hp_stream[2]);return 1;
        }
    return 0;
}

static void hp_receive(uint32_t source,const uint8_t *p,uint32_t len)
{
    hp_ring_t *r=&hp_ring[source];uint32_t b=hp_base(source),i,frames=len/4;
    hp_v[b]++;hp_v[b+1]+=frames;
    if(len>HP_MAX_PACKET || (len&3)) { hp_v[b+2]++;return; }
    for(i=0;i<frames;i++) {
        uint32_t j;int16_t v[2];
        v[0]=(int16_t)(p[4*i]|(uint16_t)p[4*i+1]<<8);v[1]=(int16_t)(p[4*i+2]|(uint16_t)p[4*i+3]<<8);
        for(j=0;j<2;j++) {
            uint32_t peak=hp_abs(v[j]);if(peak>hp_v[b+7+j])hp_v[b+7+j]=peak;
            hp_v[b+9]=hp_hash(hp_v[b+9],(uint16_t)v[j]);
        }
        if(r->w-r->r == HP_RING) { hp_v[b+6]++;continue; }
        r->pcm[r->w&(HP_RING-1)][0]=v[0];r->pcm[r->w&(HP_RING-1)][1]=v[1];r->w++;
    }
    hp_v[b+11]=r->w-r->r;
}

/* Constant-rate consumption deliberately exposes clock drift. No disguised SRC,
 * frame insertion/deletion or claims of an independent-clock production mixer. */
static uint32_t hp_output(uint8_t *p)
{
    uint32_t n,i,j,k;
    hp_fraction+=hp_v[H_RATE];n=hp_fraction/1000;hp_fraction%=1000;
    if(!hp_primed && hp_ring[0].w-hp_ring[0].r>=192 && hp_ring[1].w-hp_ring[1].r>=192) hp_primed=1;
    for(i=0;i<n;i++) for(j=0;j<2;j++) {
        int32_t v=0;
        for(k=0;k<2;k++) if(hp_primed) {
            hp_ring_t *r=&hp_ring[k];
            if(r->r!=r->w) v+=r->pcm[r->r&(HP_RING-1)][j];
            else if(!j) hp_v[hp_base(k)+5]++;
        }
        v=(v*(int32_t)hp_gain)/400; /* each source <= -12dB at gain100 */
        if(v>32767)v=32767;
        if(v< -32768)v= -32768;
        p[4*i+2*j]=(uint8_t)v;p[4*i+2*j+1]=(uint8_t)((uint16_t)v>>8);
        if(hp_abs(v)>hp_v[H_OUT_PEAK]) hp_v[H_OUT_PEAK]=hp_abs(v);
        hp_v[H_OUT_HASH]=hp_hash(hp_v[H_OUT_HASH],(uint16_t)v);
        if(j && hp_primed) for(k=0;k<2;k++) if(hp_ring[k].r!=hp_ring[k].w) hp_ring[k].r++;
    }
    for(k=0;k<2;k++) {
        uint32_t fill=hp_ring[k].w-hp_ring[k].r,b=hp_base(k);
        if(hp_primed && hp_v[b+3]==HP_RING)hp_v[b+10]=fill;
        if(hp_primed && fill<hp_v[b+3])hp_v[b+3]=fill;
        if(fill>hp_v[b+4])hp_v[b+4]=fill;
        hp_v[b+11]=fill;
    }
    return n*4;
}

/* The shared-address invariant is independent of the SIE model. */
typedef struct { uint8_t pending, pipe, next; uint16_t frame; uint32_t start; } hp_schedule_t;
static hp_schedule_t hp_sched;
static int hp_next(uint32_t frame)
{
    if(hp_sched.pending || hp_sched.next>=3 || hp_sched.frame!=(uint16_t)frame) return -1;
    hp_sched.pipe=hp_sched.next++;hp_sched.pending=1;return hp_sched.pipe;
}
static int hp_new_frame(uint32_t frame)
{
    uint32_t delta=(frame-hp_sched.frame)&2047u;
    if(!delta)return 0;
    if(hp_sched.pending || hp_sched.next!=3) {hp_v[H_LATE]++;return -1;}
    hp_v[H_SOF]++;if(delta>1)hp_v[H_MISSED]+=delta-1;
    hp_sched.frame=(uint16_t)frame;hp_sched.next=0;return 1;
}
