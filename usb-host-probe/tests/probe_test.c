/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "probe_core.h"
#define HP_BUILD_TAG 12345u
#define FM1_TICKS_PER_US 24u
enum { S_FADDR=0,S_POWER=1,S_INTRTX1E=7,S_INTRTX2E=8,S_INTRRX1E=9,S_INTRRX2E=10,
       S_INTRUSBE=11,S_FRAME1=12,S_FRAME2=13,S_INDEX=14,S_TXMAXP=16,S_CSR0=17,
       S_TXCSR1=17,S_TXCSR2=18,S_RXMAXP=19,S_RXCSR1=20,S_RXCSR2=21,S_COUNT0=22,
       S_RXCOUNT1=22,S_RXCOUNT2=23 };
static uint8_t ep0buf[68],*fake_rxbuf[5];
static struct {uint32_t timeouts,up,detached,config,sx_on,sx_len;} usb;
static uint32_t fm1_ms,fake_us,fake_addr,fake_index,fake_csr[5][32],fake_port;
static uint32_t fake_pending,fake_done_at,fake_delay=120,fake_active_addr,fake_error,fake_bad_len;
static uint32_t fake_rx_count,fake_output_nonzero,fake_switches,fake_stall,fake_rate=48000;
static int32_t fake_frozen_frame=-1;
static uint32_t fake_midi_enabled,fake_midi_delay=20,fake_blocked_device,fake_late_ack;
static uint32_t fake_midi_data[4][64],fake_midi_w[4],fake_midi_r[4];
static uint32_t fake_midi_sent[4][2048],fake_midi_count[4],fake_midi_tx_len;
static uint8_t fake_midi_tx[64],fake_dev_rx_toggle[64],fake_dev_tx_toggle[64];
static uint32_t fake_addr_device[64],fake_ctrl_len,fake_ctrl_pos,fake_ctrl_request,fake_ctrl_requested;
static uint8_t fake_ctrl[4096];
static volatile uint8_t hp_owns_usb;
static uint32_t fm1_ticks(void) {return fake_us*24;}
static void fm1_wdt_feed(void) {fake_us+=1000;fm1_ms=fake_us/1000;}
static void fm1_usb_reset(void) {fake_pending=0;}
static void fm1_usb_off(void) {fake_pending=0;}
static void fm1_usb_ep_enable(uint32_t n) {(void)n;}
static void fm1_usb_ep_txbuf(uint32_t ep,void *p) {(void)ep;(void)p;}
static void fm1_usb_rx_sync(void) {}
static void fm1_usb_ep0_send(void *p,uint32_t n) {(void)p;(void)n;}
static void usb_start(void) {usb.up=1;}

static uint32_t fake_config(uint8_t *p,int sink)
{
    uint8_t d[]={9,2,52,0,1,1,0,0x80,50,
        9,4,1,1,1,1,2,0,0,
        7,0x24,1,1,1,1,0,
        11,0x24,2,1,2,2,16,1,0x80,0xbb,0,
        9,5,0x81,5,200,0,1,0,0,
        7,0x25,1,1,0,0,0};
    if(sink) {d[38]=1;d[39]=9;}
    d[33]=(uint8_t)fake_rate;d[34]=(uint8_t)(fake_rate>>8);
    memcpy(p,d,sizeof d);
    if(fake_midi_enabled){
        const uint8_t midi[]={9,4,2,0,2,1,3,0,0,7,0x24,1,0,1,37,0,
            9,5,0x82,2,64,0,0,0,0,6,0x25,1,2,1,2,
            9,5,2,2,64,0,0,0,0,6,0x25,1,2,3,4};
        memcpy(p+sizeof d,midi,sizeof midi);p[2]=sizeof d+sizeof midi;p[4]=2;
        return sizeof d+sizeof midi;
    }
    return sizeof d;
}
static void fake_setup(void)
{
    uint32_t rq=ep0buf[1],v=ep0buf[2]|(uint32_t)ep0buf[3]<<8;
    uint32_t ix=ep0buf[4]|(uint32_t)ep0buf[5]<<8;
    uint32_t dev=fake_addr?fake_addr_device[fake_addr]:fake_port;
    fake_ctrl_len=fake_ctrl_pos=0;fake_ctrl_request=rq;fake_stall=0;
    fake_ctrl_requested=ep0buf[6]|(uint32_t)ep0buf[7]<<8;
    if(rq==5)fake_addr_device[v]=dev;
    else if(rq==6 && (v>>8)==1) {
        uint8_t d[]={18,1,0x10,1,0,0,0,64,0x34,0x12,0,0,0,1,0,0,0,1};
        d[4]=dev?0:9;d[10]=(uint8_t)dev;memcpy(fake_ctrl,d,18);fake_ctrl_len=18;
    } else if(rq==6 && (v>>8)==2) {
        if(!dev){uint8_t d[]={9,2,9,0,1,1,0,0xe0,0};memcpy(fake_ctrl,d,9);fake_ctrl_len=9;}
        else fake_ctrl_len=fake_config(fake_ctrl,dev==3);
    } else if(rq==6 && (v>>8)==0x29) {
        uint8_t d[]={9,0x29,3,0,0,10,0,0,255};memcpy(fake_ctrl,d,9);fake_ctrl_len=9;
    } else if(rq==0 && ep0buf[0]==0xa3) {
        fake_ctrl[0]=3;fake_ctrl[1]=0;fake_ctrl[2]=16;fake_ctrl[3]=0;fake_ctrl_len=4;
    } else if(rq==3 && v==4 && ep0buf[0]==0x23)fake_port=ix;
    else if(rq==0x81 && ep0buf[0]==0xa2) {
        fake_ctrl[0]=(uint8_t)fake_rate;fake_ctrl[1]=(uint8_t)(fake_rate>>8);fake_ctrl[2]=0;fake_ctrl_len=3;
    }
}
static void fake_finish(void)
{
    if(!fake_pending || fake_us<fake_done_at)return;
    assert(fake_addr==fake_active_addr); /* no address switch while transaction in flight */
    if(fake_pending>=4){
        uint32_t dev=fake_addr_device[fake_addr];
        if(fake_pending==4){
            if(fake_midi_r[dev]==fake_midi_w[dev])return;
            assert(((fake_csr[1][S_RXCSR2]>>1)&1)==fake_dev_rx_toggle[fake_addr]);
            uint32_t count=fake_midi_w[dev]-fake_midi_r[dev];if(count>16)count=16;
            for(uint32_t i=0;i<count;i++){
                uint32_t word=fake_midi_data[dev][fake_midi_r[dev]++%64];
                for(uint32_t j=0;j<4;j++)fake_rxbuf[1][4*i+j]=(uint8_t)(word>>(8*j));
            }
            fake_csr[1][S_RXCOUNT1]=count*4;fake_csr[1][S_RXCOUNT2]=0;fake_csr[1][S_RXCSR1]=1;
            fake_csr[1][S_RXCSR2]^=2;fake_dev_rx_toggle[fake_addr]^=1;
        }else{
            if(dev==fake_blocked_device)return;
            assert((fake_csr[1][S_TXCSR2]&1)==fake_dev_tx_toggle[fake_addr]);
            for(uint32_t i=0;i<fake_midi_tx_len/4;i++){
                const uint8_t *p=fake_midi_tx+4*i;assert(fake_midi_count[dev]<2048);
                fake_midi_sent[dev][fake_midi_count[dev]++]=p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
            }
            fake_csr[1][S_TXCSR1]=0;fake_csr[1][S_TXCSR2]^=1;fake_dev_tx_toggle[fake_addr]^=1;
        }
        fake_pending=0;return;
    }
    if(fake_pending<3) {
        uint32_t pipe=fake_pending-1,ep=3+pipe,dev=fake_addr_device[fake_addr];
        uint32_t frames=fake_rate==48000?48:44+(((fake_us/1000)%10)==0);
        uint32_t len=frames*4;
        assert(dev==pipe+1); /* same endpoint numbers on distinct devices */
        for(uint32_t i=0;i<frames;i++) {
            int16_t left=dev==1?1000:3000,right=dev==1?-2000:-4000;
            uint8_t *p=fake_rxbuf[ep]+4*i;
            p[0]=(uint8_t)left;p[1]=(uint8_t)((uint16_t)left>>8);
            p[2]=(uint8_t)right;p[3]=(uint8_t)((uint16_t)right>>8);
        }
        fake_csr[ep][S_RXCSR1]=fake_error?4:1;
        if(fake_bad_len)len=201;
        fake_csr[ep][S_RXCOUNT1]=len&255;fake_csr[ep][S_RXCOUNT2]=len>>8;
        fake_rx_count++;
    } else fake_csr[3][S_TXCSR1]=fake_error?4:0;
    fake_pending=0;
}
static uint32_t sie_rd(uint32_t r)
{
    fake_finish();
    if(r==S_FRAME1)return (fake_frozen_frame>=0?(uint32_t)fake_frozen_frame:fake_us/1000)&255;
    if(r==S_FRAME2)return ((fake_frozen_frame>=0?(uint32_t)fake_frozen_frame:fake_us/1000)>>8)&7;
    if(r==15)return 0x44;
    return fake_csr[fake_index][r];
}
static void sie_wr(uint32_t r,uint32_t v)
{
    fake_finish();
    if(r==S_FADDR){assert(!fake_pending);fake_addr=v;fake_switches++;return;}
    if(r==S_INDEX){fake_index=v;return;}
    if(!fake_index && r==S_CSR0) {
        if(v==0x0a){fake_setup();fake_csr[0][S_CSR0]=0;}
        else if(v==0x20){
            uint32_t n=fake_ctrl_len-fake_ctrl_pos;if(n>64)n=64;
            uint32_t requested=ep0buf[6]|(uint32_t)ep0buf[7]<<8;(void)requested;
            /* firmware requests 8/9-byte prefixes: original request length is retained separately below */
            if(fake_ctrl_pos==0 && fake_ctrl_request==6 && fake_ctrl_len==18 && fake_addr==0)n=8;
            if(fake_ctrl_pos==0 && fake_ctrl_request==6 && fake_ctrl_len==52 && fake_ctrl[2]==52 && fake_ctrl[0]==9 && fake_ctrl_len>fake_ctrl_requested)n=fake_ctrl_requested;
            if(fake_ctrl_pos==0 && n>fake_ctrl_requested)n=fake_ctrl_requested;
            memcpy(ep0buf,fake_ctrl+fake_ctrl_pos,n);fake_ctrl_pos+=n;
            fake_csr[0][S_COUNT0]=n;fake_csr[0][S_CSR0]=1;
        } else if(v==0x60){fake_csr[0][S_COUNT0]=0;fake_csr[0][S_CSR0]=1;}
        else fake_csr[0][S_CSR0]=0;
        return;
    }
    if(fake_index==1 && fake_pending==4 && r==S_RXCSR1 && !(v&0x20)){
        uint32_t dev=fake_addr_device[fake_addr];
        if(fake_midi_r[dev]==fake_midi_w[dev])fake_pending=0;
    }
    if(fake_index==1 && r==S_RXCSR1 && (v&0x10)){
        fake_csr[1][S_RXCSR1]=0;return;
    }
    if(fake_index==1 && r==S_TXCSR1 && (v&8)){
        if(!fake_late_ack || fake_addr_device[fake_addr]==fake_blocked_device)fake_pending=0;
        fake_csr[1][S_TXCSR1]=0;return;
    }
    if(fake_index==1 && r==S_RXCSR1)v=(v&~1u)|(fake_csr[1][r]&v&1u); /* RxPktRdy is W0C. */
    fake_csr[fake_index][r]=v;
    if((r==S_RXCSR1 && v==0x20) || (r==S_TXCSR1 && v==1)) {
        assert(!fake_pending);fake_active_addr=fake_addr;fake_done_at=fake_us+fake_delay;
        fake_pending=fake_index==1?(r==S_RXCSR1?4:5):(r==S_RXCSR1?fake_index-2:3);
        if(fake_index==1)fake_done_at=fake_us+fake_midi_delay;
    }
}
static void fm1_usb_ep_send(uint32_t ep,void *p,uint32_t n)
{
    uint8_t *d=p;
    if(ep==1){assert(n<=64);memcpy(fake_midi_tx,p,n);fake_midi_tx_len=n;return;}
    assert(ep==3 && n<=200 && fake_addr_device[fake_addr]==3);
    if(n && hp_primed) {
        int16_t l=(int16_t)(d[0]|d[1]<<8),r=(int16_t)(d[2]|d[3]<<8);
        assert(l==1000 && r== -1500);fake_output_nonzero++;
    }
}
#include "probe_usb.h"
static uint8_t fake_wire[464];static uint32_t fake_wire_len;
static void ota_wr32(uint8_t *p,uint32_t v){for(uint32_t i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t ota_pack7(const uint8_t *p,uint32_t n,uint8_t *out)
{
    uint32_t a=0,b=0,k=0;while(n--){a|=(uint32_t)*p++<<b;b+=8;while(b>=7){out[k++]=a&127;a>>=7;b-=7;}}
    if(b)out[k++]=a&127;return k;
}
static int ota_wire_send(const uint8_t *p,uint32_t n){assert(n<=sizeof fake_wire);memcpy(fake_wire,p,n);fake_wire_len=n;return 0;}
#include "probe_wire.h"
static void put7(uint8_t *p,uint32_t v,uint32_t n){while(n--){*p++=v&127;v>>=7;}}
static void test_wire(const char *fixture)
{
    uint8_t req[17]={0x7d,0x55,0x48,1,2},raw[384];uint32_t a=0,b=0,k=0;
    hp_frozen_valid=0;hp_freeze();assert(hp_sysex(req,5) && fake_wire[6]==0);
    for(uint32_t i=7;i+1<fake_wire_len;i++){
        a|=(uint32_t)fake_wire[i]<<b;b+=7;if(b>=8){raw[k++]=a&255;a>>=8;b-=8;}
    }
    assert(k==332 && !memcmp(raw,hp_frozen,sizeof hp_frozen));
    if(fixture){FILE *f=fopen(fixture,"wb");assert(f);assert(fwrite(raw,1,k,f)==k);fclose(f);}
    /* Bounded diagnostics, immutable read/CRC and independent acknowledgement. */
    hp_diag_valid=0;memset(hp_diag_nonce,0,sizeof hp_diag_nonce);hp_diag_begin(fm1_ms,HP_BUILD_TAG);hp_diag_finish(fm1_ms,HP_STOPPED,HP_INCOMPLETE);assert(!hp_diag_valid);
    uint8_t arm_req[21]={0x7d,0x55,0x48,1,12};
    for(uint32_t j=0;j<4;j++)put7(arm_req+5+4*j,j+1,4);
    hp_sysex(arm_req,21);assert(!fake_wire[6] && hp_diag_nonce[3]==4);
    put7(arm_req+5,99,4);hp_sysex(arm_req,21);assert(fake_wire[6]==3 && hp_diag_nonce[0]==1);
    put7(arm_req+5,1,4);hp_sysex(arm_req,21);assert(!fake_wire[6]);
    hp_diag_begin(fm1_ms,HP_BUILD_TAG);assert(hp_diag[25]==1 && !hp_diag_nonce[0]);
    for(uint32_t j=0;j<18;j++){fm1_ms+=1000;hp_diag_sample(fm1_ms,25+j);}
    hp_diag_stop(fm1_ms,HP_STOPPED,HP_INCOMPLETE);hp_freeze();
    assert(hp_diag_valid && hp_diag[22]==15 && hp_diag[23]>0 && hp_diag[19]==42);
    req[4]=9;hp_sysex(req,5);assert(!fake_wire[6]);a=b=k=0;
    for(uint32_t j=7;j+1<fake_wire_len;j++){a|=(uint32_t)fake_wire[j]<<b;b+=7;if(b>=8){raw[k++]=a&255;a>>=8;b-=8;}}
    assert(k==sizeof hp_diag_frozen+4 && !memcmp(raw,hp_diag_frozen,sizeof hp_diag_frozen));
    if(fixture){char path[512];snprintf(path,sizeof path,"%s.performance",fixture);FILE *f=fopen(path,"wb");assert(f);assert(fwrite(raw,1,k,f)==k);fclose(f);}
    uint32_t saved_crc=hp_diag_crc;hp_diag[19]++;hp_freeze();assert(hp_diag_crc==saved_crc);
    req[4]=11;hp_sysex(req,5);assert(!fake_wire[6]);
    hp_owns_usb=1;req[4]=9;hp_sysex(req,5);assert(fake_wire[6]==1);hp_owns_usb=0;
    req[4]=10;put7(req+5,hp_diag_frozen[2],4);put7(req+9,(hp_diag_crc^1)&0x0fffffff,4);
    hp_sysex(req,13);assert(hp_diag_valid && fake_wire[6]==2);
    put7(req+9,hp_diag_crc&0x0fffffff,4);hp_sysex(req,13);assert(!hp_diag_valid && !fake_wire[6]);
    req[4]=9;hp_sysex(req,5);assert(fake_wire[6]==3);
    req[4]=1;put7(req+5,77,4);put7(req+9,10000,3);put7(req+12,48000,3);req[15]=25;
    hp_sysex(req,17);assert(fake_wire[6]==3 && hp_frozen_valid); /* cannot replace uncollected snapshot */
    req[4]=3;put7(req+5,hp_frozen[H_RUN],4);put7(req+9,hp_frozen_crc&0x0fffffffu,4);
    hp_sysex(req,13);assert(fake_wire[6]==0 && !hp_frozen_valid);
    memset(req+5,0,12);req[4]=1;put7(req+5,77,4);put7(req+9,10000,3);put7(req+12,48000,3);req[15]=25;
    hp_sysex(req,17);assert(fake_wire[6]==0 && hp_v[H_STATE]==HP_ARMED && hp_v[H_RUN]==77);
    assert(!hp_owns_usb && !hp_running); /* arming cannot enter host or update mode */
    hp_owns_usb=1;req[4]=2;hp_sysex(req,5);assert(fake_wire[6]==1);hp_owns_usb=0;
    req[4]=1;req[15]=101;hp_sysex(req,17);assert(fake_wire[6]==2);
}

static void reset_case(uint32_t rate,uint32_t delay)
{
    memset(hp_v,0,sizeof hp_v);memset(fake_csr,0,sizeof fake_csr);memset(fake_addr_device,0,sizeof fake_addr_device);
    fake_us=fm1_ms=fake_addr=fake_index=fake_port=fake_pending=0;
    fake_error=fake_bad_len=fake_output_nonzero=fake_rx_count=fake_switches=0;
    fake_rate=rate;fake_delay=delay;fake_frozen_frame=-1;hp_gain=100;hp_owns_usb=0;usb.timeouts=0;
    fake_midi_enabled=fake_blocked_device=fake_late_ack=0;fake_midi_delay=20;
    memset(fake_midi_w,0,sizeof fake_midi_w);memset(fake_midi_r,0,sizeof fake_midi_r);
    memset(fake_midi_count,0,sizeof fake_midi_count);memset(fake_dev_rx_toggle,0,sizeof fake_dev_rx_toggle);
    memset(fake_dev_tx_toggle,0,sizeof fake_dev_tx_toggle);hp_audio_enabled=1;hp_defer_run=0;
    hp_v[H_RUN]=99;hp_v[H_RATE]=rate;hp_v[H_DURATION]=10000;
}
static void run_case(void)
{
    hp_host_start();assert(hp_v[H_STATE]==HP_RUNNING);
    for(uint32_t i=0;i<120000 && hp_running;i++) {fake_us+=100;fm1_ms=fake_us/1000;hp_tick();}
    assert(!hp_running);hp_host_stop();
}
static void midi_model(void)
{
    reset_case(48000,120);fake_midi_enabled=1;hp_defer_run=1;
    hp_host_start();assert(hp_v[H_STATE]==HP_READY && hm_nin==6 && hm_nout==6);
    assert(hm_route_set(0,(1u<<2)|(1u<<3)|(1u<<4)));
    assert(hm_route_set(2,1));
    fake_midi_data[1][fake_midi_w[1]++]=0x643c9009u; /* ch1 C note-on, cable0 */
    fake_midi_data[1][fake_midi_w[1]++]=0x0000f80fu; /* clock */
    fake_midi_data[1][fake_midi_w[1]++]=0x003c8008u; /* note-off */
    fake_midi_data[2][fake_midi_w[2]++]=0x64409009u;
    fake_midi_data[2][fake_midi_w[2]++]=0x00408008u;
    hp_run_begin();assert(!hm_route_set(0,0));
    for(uint32_t i=0;i<120000 && hp_running;i++){fake_us+=100;fm1_ms=fake_us/1000;hp_tick();}
    assert(!hp_running && hp_v[H_STATE]==HP_COMPLETE && !hm_fault && !hm_cleanup_bad);
    assert(hm_events==5 && hm_fanout==11 && hm_rx_packets>=2 && hm_tx_packets>0);
    uint32_t seen0=0,seen1=0;
    for(uint32_t i=0;i<fake_midi_count[2];i++){
        if(fake_midi_sent[2][i]==0x643c9009u)seen0++;
        if(fake_midi_sent[2][i]==0x643c9019u)seen1++;
    }
    assert(seen0==1 && seen1==1); /* independent virtual output cables, no duplicates */
    assert(hm_idle_polls>0);hp_host_stop();assert(hp_v[H_MIDI_EVENTS]==5);

    /* Standalone routing, delayed ACK during cancellation, and NAK retry. */
    reset_case(48000,120);fake_midi_enabled=1;hp_audio_enabled=0;hp_defer_run=1;
    hp_host_start();assert(hp_v[H_STATE]==HP_READY);assert(hm_route_set(0,1u<<2));
    fake_midi_data[1][fake_midi_w[1]++]=0x643c9009u;
    fake_midi_delay=150;fake_late_ack=1;fake_blocked_device=2;hp_run_begin();
    for(uint32_t i=0;i<1000;i++){
        fake_us+=100;fm1_ms=fake_us/1000;hp_tick();
        if(i==100)fake_blocked_device=0;
    }
    assert(hp_running && !hm_fault && hm_retries>0);
    hp_request_stop(HP_STOPPED,HP_INCOMPLETE);
    for(uint32_t i=0;i<20000 && hp_running;i++){fake_us+=100;fm1_ms=fake_us/1000;hp_tick();}
    assert(!hp_running && !hm_cleanup_bad && !hm_fault);
    seen0=0;for(uint32_t i=0;i<fake_midi_count[2];i++)if(fake_midi_sent[2][i]==0x643c9009u)seen0++;
    assert(seen0==1);hp_host_stop();
    reset_case(48000,120);fake_midi_enabled=1;hp_audio_enabled=0;hp_defer_run=1;
    hp_host_start();assert(hm_route_set(0,1u<<2));
    fake_midi_data[1][fake_midi_w[1]++]=0x643c9009u;fake_blocked_device=2;hp_run_begin();
    for(uint32_t i=0;i<30000 && hp_running;i++){fake_us+=100;fm1_ms=fake_us/1000;hp_tick();}
    assert(!hp_running && hp_v[H_STATE]==HP_FAILED && hm_fault==HM_BLOCKED && hm_cleanup_bad);
    assert(!fake_midi_count[2]);hp_host_stop();
    puts("PASS: actual MIDI transport: composite discovery, virtual cables, audio coexistence, idle IN, NAK retries, late ACK, stop cleanup");
}
int main(int argc,char **argv)
{
    uint8_t d[128];hp_device_t dev={.addr=1,.mps0=64};uint32_t n;
    fake_rate=48000;n=fake_config(d,0);assert(n==52 && hp_parse(d,n,48000,&dev) && dev.in.ep==0x81);
    memset(&dev,0,sizeof dev);assert(!hp_parse(d,n-1,48000,&dev));
    d[29]=1;memset(&dev,0,sizeof dev);assert(!hp_parse(d,n,48000,&dev)); /* mono rejected */
    n=fake_config(d,1);d[39]=5;memset(&dev,0,sizeof dev);assert(!hp_parse(d,n,48000,&dev)); /* async OUT */
    n=fake_config(d,0);d[25]=255;memset(&dev,0,sizeof dev);assert(!hp_parse(d,n,48000,&dev)); /* malformed length */
    for(uint32_t rate=44100;rate<=48000;rate+=3900) {
        reset_case(rate,120);run_case();assert(hp_v[H_STATE]==HP_COMPLETE);
        assert(hp_v[H_OUT_PACKETS]>=9999 && hp_v[H_OUT_PACKETS]<=10001);
        assert(hp_v[H_A_PACKETS]==hp_v[H_OUT_PACKETS] && hp_v[H_B_PACKETS]==hp_v[H_OUT_PACKETS]);
        assert(hp_v[H_ADDR_SWITCHES]==3*hp_v[H_OUT_PACKETS]);
        assert(hp_stream[0].addr!=hp_stream[1].addr && hp_stream[0].addr!=hp_stream[2].addr);
        assert(hp_v[H_A_HASH]!=hp_v[H_B_HASH] && fake_output_nonzero>9000);
        assert(hp_v[H_OUT_FRAMES]==rate*10); /* exact 44/45 and 48-frame cadence */
        if(rate==48000)test_wire(argc>1?argv[1]:0);
    }
    reset_case(48000,400);run_case();assert(hp_v[H_STATE]==HP_FAILED && hp_v[H_REASON]==HP_DEADLINE);
    reset_case(48000,120);hp_host_start();fake_error=1;
    for(uint32_t i=0;i<30 && hp_running;i++){fake_us+=100;fm1_ms=fake_us/1000;hp_tick();}
    assert(hp_v[H_STATE]==HP_FAILED && hp_v[H_REASON]==HP_TRANSFER);hp_host_stop();
    reset_case(48000,120);hp_host_start();fake_bad_len=1;
    for(uint32_t i=0;i<30 && hp_running;i++){fake_us+=100;fm1_ms=fake_us/1000;hp_tick();}
    assert(hp_v[H_STATE]==HP_FAILED && hp_v[H_A_ERRORS]);hp_host_stop();
    reset_case(48000,120);hp_host_start();fake_frozen_frame=(int32_t)hp_sched.frame;
    for(uint32_t i=0;i<300 && hp_running;i++){fake_us+=100;fm1_ms=fake_us/1000;hp_tick();}
    assert(hp_v[H_STATE]==HP_FAILED && hp_v[H_REASON]==HP_NO_SOF);hp_host_stop();
    reset_case(48000,120);hp_host_start();usb.timeouts=1;hp_tick();
    assert(hp_v[H_STATE]==HP_FAILED && hp_v[H_REASON]==HP_IO);usb.timeouts=0;hp_host_stop();
    midi_model();
    puts("PASS: real host code/model: discovery, three addresses, serial packets, mix identity, cadence, deadline/error/length aborts");
    return 0;
}
