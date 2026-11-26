/* SPDX-License-Identifier: GPL-3.0-only */
/* USB0 host enumeration/control sequences adapted from Melodee usb_host.c,
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio), with enumeration contributed
 * by recursivefieldinstruments. UAC probe and MIDI port router. */
#pragma once
#include "fm1_host_probe.h"
static uint8_t hp_desc[4096] __attribute__((aligned(32)));
static uint8_t hp_rx[2][224] __attribute__((aligned(32)));
static uint8_t hp_tx[224] __attribute__((aligned(32)));
static uint8_t hp_io_fault,hp_next_addr,hp_count;
static volatile uint8_t hp_running;
static uint32_t hp_begin_ms,hp_last_sof_ms,hp_out_pending;
static volatile uint8_t hp_finishing;
static uint32_t hp_finish_ms,hp_frame_ticks;
static void hp_w(uint32_t r,uint32_t v) {if(hp_io_fault)return;sie_wr(r,v);if(usb.timeouts)hp_io_fault=1;}
static uint32_t hp_r(uint32_t r) {uint32_t v;if(hp_io_fault)return 0;v=sie_rd(r);if(usb.timeouts)hp_io_fault=1;return v;}
static void hp_wait(uint32_t ms) {uint32_t t=fm1_ms;while(fm1_ms-t<ms)fm1_wdt_feed();}
static void hp_fail(uint32_t reason)
{ hp_diag_stop(fm1_ms,HP_FAILED,reason);hp_v[H_STATE]=HP_FAILED;hp_v[H_REASON]=reason;hp_running=0;
  if(hm_forward || hm_stopping){hm_forward=hm_stopping=0;if(hm_events)hm_cleanup_bad+=hm_nout;} }
static int hp_csr0(uint32_t set,uint32_t clear)
{
    uint32_t t=fm1_ms,v;
    do {
        v=hp_r(S_CSR0);if(hp_io_fault || (v&0x14))return -1;
        if((v&set)==set && !(v&clear))return (int)v;
        fm1_wdt_feed();
    } while(fm1_ms-t<500);
    return -1;
}
/* Complete control transfer, including the UAC sampling-frequency OUT data stage.
 * Called only with streaming stopped: EP0 and stream address ownership never overlap. */
static int hp_ctl(uint8_t addr,uint8_t rt,uint8_t rq,uint16_t value,uint16_t idx,
                  uint16_t len,uint8_t *data,uint8_t mps)
{
    uint32_t got=0,n,i;
    if(hp_running || (len && !data) || !(mps==8||mps==16||mps==32||mps==64))return -1;
    hp_w(S_FADDR,addr);hp_w(S_INDEX,0);hp_w(S_CSR0,0);
    ep0buf[0]=rt;ep0buf[1]=rq;ep0buf[2]=(uint8_t)value;ep0buf[3]=(uint8_t)(value>>8);
    ep0buf[4]=(uint8_t)idx;ep0buf[5]=(uint8_t)(idx>>8);ep0buf[6]=(uint8_t)len;ep0buf[7]=(uint8_t)(len>>8);
    fm1_usb_ep0_send(ep0buf,8);hp_w(S_CSR0,0x0a);
    if(hp_csr0(0,2)<0)return -1;
    if(rt&128) {
        while(got<len) {
            hp_w(S_CSR0,0x20);if(hp_csr0(1,0)<0)return -1;
            n=hp_r(S_COUNT0);if(n>mps || n>len-got)return -1;
            fm1_usb_rx_sync();for(i=0;i<n;i++)data[got+i]=ep0buf[i];
            got+=n;hp_w(S_CSR0,0);if(n<mps)break;
        }
        hp_w(S_CSR0,0x42);if(hp_csr0(0,2)<0)return -1;
    } else {
        while(got<len) {
            n=len-got;if(n>mps)n=mps;
            for(i=0;i<n;i++)ep0buf[i]=data[got+i];
            fm1_usb_ep0_send(ep0buf,n);hp_w(S_CSR0,2);
            if(hp_csr0(0,2)<0)return -1;got+=n;
        }
        hp_w(S_CSR0,0x60);if(hp_csr0(1,0)<0)return -1;hp_w(S_CSR0,0);
    }
    return hp_io_fault ? -1 : (int)got;
}

static int hp_enumerate(uint32_t depth,uint32_t path);
static int hp_hub(uint8_t addr,uint8_t mps,uint8_t cfg,uint32_t depth,uint32_t path)
{
    uint8_t d[16],st[4];uint32_t p,ports,t,good;
    if(depth>=2)return 0;
    if(hp_ctl(addr,0,9,cfg,0,0,0,mps)<0 || hp_ctl(addr,0xa0,6,0x2900,0,9,d,mps)<7)return -1;
    ports=d[2];if(ports>7)ports=7;good=(uint32_t)d[5]*2;
    for(p=1;p<=ports;p++)if(hp_ctl(addr,0x23,3,8,(uint16_t)p,0,0,mps)<0)return -1;
    hp_wait((good<100?100:good)+100);
    for(p=1;p<=ports;p++) {
        if(hp_ctl(addr,0xa3,0,0,(uint16_t)p,4,st,mps)!=4)return -1;
        if(!(st[0]&1))continue;
        if(st[1]&2) {hp_v[H_REJECTED]++;continue;} /* low-speed */
        if(hp_ctl(addr,0x23,3,4,(uint16_t)p,0,0,mps)<0)return -1;
        t=fm1_ms;
        do {
            if(hp_ctl(addr,0xa3,0,0,(uint16_t)p,4,st,mps)!=4)return -1;
            if(st[2]&16)break;
            hp_wait(10);
        } while(fm1_ms-t<500);
        if(!(st[2]&16) || !(st[0]&2)) {hp_v[H_REJECTED]++;continue;}
        if(hp_ctl(addr,0x23,1,20,(uint16_t)p,0,0,mps)<0)return -1;
        hp_wait(20);
        if(hp_enumerate(depth+1,(path<<4)|p)<0)return -1;
    }
    return 0;
}
static int hp_enumerate(uint32_t depth,uint32_t path)
{
    uint8_t d[18],c[9],addr,mps;uint32_t n;
    hp_device_t dev={.addr=0};
    if(hp_next_addr>=64)return -1;
    if(hp_ctl(0,0x80,6,0x100,0,8,d,8)!=8)return -1;
    mps=d[7];if(!(mps==8||mps==16||mps==32||mps==64))return -1;
    addr=hp_next_addr++;
    if(hp_ctl(0,0,5,addr,0,0,0,mps)<0)return -1;hp_wait(10);
    if(hp_ctl(addr,0x80,6,0x100,0,18,d,mps)!=18 ||
       hp_ctl(addr,0x80,6,0x200,0,9,c,mps)!=9)return -1;
    hp_v[H_ENUM_DEVICES]++;
    if(d[4]==9)return hp_hub(addr,mps,c[5],depth,path);
    n=c[2]|(uint32_t)c[3]<<8;
    if(n<9 || n>sizeof hp_desc || hp_count>=16) {hp_v[H_REJECTED]++;return 0;}
    if(hp_ctl(addr,0x80,6,0x200,0,(uint16_t)n,hp_desc,mps)!=(int)n)return -1;
    dev.addr=addr;dev.mps0=mps;dev.vid=(uint16_t)(d[8]|d[9]<<8);
    dev.pid=(uint16_t)(d[10]|d[11]<<8);dev.path=(uint16_t)path;
    int audio=hp_parse(hp_desc,n,hp_v[H_RATE],&dev),midi=hm_parse(hp_desc,n,&dev);
    if(midi<0)return -1;
    if(!audio && !midi) {hp_v[H_REJECTED]++;return 0;}
    hp_devices[hp_count++]=dev;return 0;
}

static int hp_stream_config(void)
{
    uint32_t i;uint8_t rate[3];
    rate[0]=(uint8_t)hp_v[H_RATE];rate[1]=(uint8_t)(hp_v[H_RATE]>>8);rate[2]=0;
    for(i=0;i<3;i++) {
        hp_stream_t *s=&hp_stream[i];
        if(hp_ctl(s->addr,0,9,s->cfg,0,0,0,s->mps0)<0 ||
           hp_ctl(s->addr,1,11,s->alt,s->iface,0,0,s->mps0)<0)return -1;
        /* SET_CUR can STALL on fixed-rate devices. GET_CUR must then prove rate. */
        if(s->rate_control && hp_ctl(s->addr,0x22,1,0x100,s->ep,3,rate,s->mps0)<0) {
            uint8_t actual[3];
            if(hp_io_fault || hp_ctl(s->addr,0xa2,0x81,0x100,s->ep,3,actual,s->mps0)!=3 ||
               (actual[0]|(uint32_t)actual[1]<<8|(uint32_t)actual[2]<<16)!=hp_v[H_RATE])return -1;
        }
    }
    for(i=0;i<2;i++) {
        hp_w(S_INDEX,3+i);hp_hal_rxbuf(3+i,hp_rx[i]);hp_w(S_RXMAXP,255);
        hp_w(S_RXCSR1,0x90);hp_w(S_RXCSR1,0);hp_w(S_RXCSR2,0);
        hp_w(26,0x10|(hp_stream[i].ep&15));hp_w(27,1);
    }
    hp_w(S_INDEX,3);fm1_usb_ep_txbuf(3,hp_tx);hp_w(S_TXMAXP,255);
    hp_w(S_TXCSR1,0x48);hp_w(S_TXCSR1,0);hp_w(S_TXCSR2,0x20);
    hp_w(24,0x10|(hp_stream[2].ep&15));hp_w(25,1);
    return hp_io_fault ? -1 : 0;
}
#include "midi_usb.h"
static void hp_midi_counters(void)
{
    hp_v[H_MIDI_INPUTS]=hm_nin;hp_v[H_MIDI_OUTPUTS]=hm_nout;
    hp_v[H_MIDI_RX]=hm_rx_packets;hp_v[H_MIDI_TX]=hm_tx_packets;
    hp_v[H_MIDI_EVENTS]=hm_events;hp_v[H_MIDI_FANOUT]=hm_fanout;
    hp_v[H_MIDI_IDLE]=hm_idle_polls;hp_v[H_MIDI_RETRIES]=hm_retries;
    hp_v[H_MIDI_INVALID]=hm_invalid;hp_v[H_MIDI_PRESSURE]=hm_backpressure;
    hp_v[H_MIDI_FAULT]=hm_fault;hp_v[H_MIDI_CLEANUP]=hm_cleanup_bad;
    hp_v[H_AUDIO_ENABLED]=hp_audio_enabled;hp_v[H_MIDI_ROUTE_HASH]=hm_fingerprint(1);
    hp_v[H_MIDI_TOPOLOGY_HASH]=hm_fingerprint(0);
}
static void hp_request_stop(uint32_t state,uint32_t reason)
{
    if(hp_finishing)return;
    hp_diag_stop(fm1_ms,state,reason);
    hm_forward=0;hp_v[H_STATE]=state;hp_v[H_REASON]=reason;
    hp_finishing=1;hp_finish_ms=fm1_ms;
}

static void hp_start_transaction(uint32_t i)
{
    uint32_t len;
    hp_w(S_FADDR,hp_stream[i].addr);hp_v[H_ADDR_SWITCHES]++;
    hp_w(S_INDEX,i<2?3+i:3);hp_sched.start=fm1_ticks();
    if(i<2) {
        hp_hal_rxbuf(3+i,hp_rx[i]);hp_w(26,0x10|(hp_stream[i].ep&15));hp_w(S_RXCSR1,0x20);
    } else {
        len=hp_output(hp_tx);hp_out_pending=len/4;
        if(len>hp_stream[2].maxp) {hp_fail(HP_TRANSFER);return;}
        fm1_usb_ep_send(3,hp_tx,len);hp_w(24,0x10|(hp_stream[2].ep&15));hp_w(S_TXCSR1,1);
    }
}
static int hp_transaction_done(void)
{
    uint32_t i=hp_sched.pipe,csr,n,us;
    hp_w(S_INDEX,i<2?3+i:3);
    csr=hp_r(i<2?S_RXCSR1:S_TXCSR1);
    if((i<2 && (csr&0x4c)) || (i==2 && (csr&0xa4))) {
        hp_v[i<2?hp_base(i)+2:H_OUT_ERRORS]++;hp_fail(HP_TRANSFER);return -1;
    }
    if(i<2) {
        if(!(csr&1))return 0;
        n=hp_r(S_RXCOUNT1)|(hp_r(S_RXCOUNT2)<<8);
        if(n<4 || n>hp_stream[i].maxp || n>HP_MAX_PACKET || (n&3)) {
            hp_v[hp_base(i)+2]++;hp_fail(HP_TRANSFER);return -1;
        }
        fm1_usb_rx_sync();hp_receive(i,hp_rx[i],n);
        hp_hal_rxbuf(3+i,hp_rx[i]);hp_w(S_RXCSR1,0x10);hp_w(S_RXCSR1,0);
    } else {
        if(csr&3)return 0;
        hp_v[H_OUT_PACKETS]++;hp_v[H_OUT_FRAMES]+=hp_out_pending;
    }
    us=(fm1_ticks()-hp_sched.start)/FM1_TICKS_PER_US;
    if(us>hp_v[H_MAX_US])hp_v[H_MAX_US]=us;
    hp_sched.pending=0;return 1;
}

/* Called at 10kHz, never concurrently with enumeration or device-mode USB.
 * Only one packet can be pending, including when a frame deadline is missed. */
static uint32_t hp_frame(void)
{
    uint32_t hi,lo,again;
    hi=hp_r(S_FRAME2);lo=hp_r(S_FRAME1);again=hp_r(S_FRAME2);
    if(hi!=again) {hi=again;lo=hp_r(S_FRAME1);}
    return ((hi<<8)|lo)&2047;
}
static void hp_tick_work(void)
{
    uint32_t frame,k;int next;
    if(!hp_running)return;
    hp_v[H_ELAPSED]=fm1_ms-hp_begin_ms;
    if(hp_io_fault) {hp_v[H_IO_ERRORS]++;hp_fail(HP_IO);return;}
    if(hp_finishing){
        if(fm1_ms-hp_finish_ms>2000){hm_cleanup_bad++;hp_fail(HP_MIDI_ERROR);return;}
        if(hp_sched.pending){if(hp_transaction_done()<=0){if(fm1_ms-hp_finish_ms>5)hp_fail(HP_TRANSFER);return;}}
        if(hm_bus.unsafe){hp_fail(HP_MIDI_ERROR);return;}
        if(!hm_usb_poll())return;
        if(!hm_stopping)hm_panic_begin();
        hm_panic_pump();hm_usb_service(1);
        if(hm_panic_done && hm_empty() && !hm_bus.state){hm_stopping=0;hp_running=0;if(hm_cleanup_bad || hm_fault){hp_v[H_STATE]=HP_FAILED;hp_v[H_REASON]=HP_MIDI_ERROR;}}
        else if(fm1_ms-hp_finish_ms>2000){hm_cleanup_bad++;hp_fail(HP_MIDI_ERROR);}
        return;
    }
    hm_pump();
    if(hm_fault){hp_request_stop(HP_FAILED,HP_MIDI_ERROR);return;}
    if(!hp_audio_enabled){hm_usb_service(1);return;}
    for(k=0;k<4 && hp_running;k++) {
        frame=hp_frame();
        if(hp_io_fault) {hp_v[H_IO_ERRORS]++;hp_fail(HP_IO);return;}
        if(frame!=hp_sched.frame) {
            if(hm_bus.state){hm_usb_service(0);if(hm_bus.state){hp_fail(HP_MIDI_ERROR);return;}}
            hp_last_sof_ms=fm1_ms;
            if(hp_new_frame(frame)<0 || hp_v[H_MISSED]) {hp_fail(HP_DEADLINE);return;}
            hp_frame_ticks=fm1_ticks();
        }
        if(hp_sched.pending) {
            if(hp_transaction_done()<=0)break;
        }
        next=hp_next(frame);if(next<0)break;
        hp_start_transaction((uint32_t)next);
    }
    if(hp_io_fault) {hp_v[H_IO_ERRORS]++;hp_fail(HP_IO);return;}
    if(fm1_ms-hp_last_sof_ms>20) {hp_fail(HP_NO_SOF);return;}
    if(!hp_sched.pending && hp_sched.next==3)
        hm_usb_service(fm1_ticks()-hp_frame_ticks<=600u*FM1_TICKS_PER_US);
    if(hp_v[H_ELAPSED]>=hp_v[H_DURATION] && !hp_sched.pending && hp_sched.next==3) {
        hp_v[H_STATE]=(hp_primed && !hp_v[H_A_UNDER] && !hp_v[H_A_OVER] &&
                       !hp_v[H_B_UNDER] && !hp_v[H_B_OVER])?HP_COMPLETE:HP_FAILED;
        hp_v[H_REASON]=hp_v[H_STATE]==HP_COMPLETE?HP_OK:HP_BUFFER;
        hp_request_stop(hp_v[H_STATE],hp_v[H_REASON]);
    }
}

static void hp_tick(void)
{
 uint32_t measure=hp_running&&!hp_finishing,start=fm1_ticks();
 hp_tick_work();if(measure)hp_diag_sample(fm1_ms,(fm1_ticks()-start)/FM1_TICKS_PER_US);
}

static void hp_run_begin(void)
{
    hp_sched=(hp_schedule_t){.next=3,.frame=(uint16_t)hp_frame()};
    hp_begin_ms=hp_last_sof_ms=fm1_ms;hp_frame_ticks=fm1_ticks();hp_finishing=0;
    hp_diag_begin(fm1_ms,HP_BUILD_TAG);
    hm_forward=1;hp_v[H_STATE]=HP_RUNNING;hp_running=1;
}
static void hp_host_start(void)
{
    uint32_t i,rate=hp_v[H_RATE],duration=hp_v[H_DURATION],run=hp_v[H_RUN];
    hp_running=0;hp_owns_usb=1;usb.up=0;hp_io_fault=0;hp_count=0;hp_next_addr=1;
    hp_finishing=0;hm_discovery_begin();
    memset(hp_v,0,sizeof hp_v);memset(hp_devices,0,sizeof hp_devices);
    memset(hp_stream,0,sizeof hp_stream);memset(hp_ring,0,sizeof hp_ring);
    hp_v[H_MAGIC]=0x31504855u;hp_v[H_BUILD]=HP_BUILD_TAG;hp_v[H_RUN]=run;
    hp_v[H_RATE]=rate;hp_v[H_DURATION]=duration;hp_v[H_STATE]=HP_ENUM;hp_v[H_GAIN]=hp_gain;
    hp_v[H_A_MIN]=hp_v[H_B_MIN]=HP_RING;hp_primed=hp_fraction=0;
    fm1_usb_off();fm1_usb_reset();hp_wait(25);hp_hal_attach(ep0buf);hp_wait(25);
    hp_w(S_INTRUSBE,0);hp_w(S_INTRTX1E,0);hp_w(S_INTRTX2E,0);
    hp_w(S_INTRRX1E,0);hp_w(S_INTRRX2E,0);fm1_usb_ep_enable(31);
    hp_w(S_FADDR,0);hp_w(15,3);
    hp_w(S_POWER,8);hp_wait(50);hp_w(S_POWER,0);hp_wait(400);
    if(hp_io_fault || (hp_r(15)&0x20) || hp_enumerate(0,0)<0) {hp_fail(HP_ENUM_ERROR);return;}
    hm_discovery_end();
    if(hp_audio_enabled && !hp_choose(hp_count)) {hp_fail(HP_NO_TRIPLE);return;}
    if(!hp_audio_enabled && (!hm_nin || !hm_nout)){hp_fail(HP_NO_MIDI);return;}
    if((hp_audio_enabled && hp_stream_config()<0) || hm_usb_config()<0) {hp_fail(HP_ENUM_ERROR);return;}
    if(hp_io_fault) {hp_fail(HP_IO);return;}
    hp_begin_ms=hp_last_sof_ms=fm1_ms;
    for(i=0;i<2;i++)hp_v[hp_base(i)+9]=2166136261u;
    hp_v[H_OUT_HASH]=2166136261u;
    if(hp_defer_run)hp_v[H_STATE]=HP_READY;else hp_run_begin();
}
static void hp_host_stop(void)
{
    hp_running=0;
    hp_midi_counters();
    if(hp_v[H_STATE]==HP_RUNNING) {hp_v[H_STATE]=HP_STOPPED;hp_v[H_REASON]=HP_INCOMPLETE;}
    /* Reset aborts any pending packet before the shared address returns to device mode. */
    fm1_usb_off();hp_wait(25);usb.detached=0;usb.config=0;usb.sx_on=0;usb.sx_len=0;
    usb_start();hp_owns_usb=0;
}
