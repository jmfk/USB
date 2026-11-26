/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>

#define HM_PORTS 32u
#define HM_PIPES 64u
#define HM_QUEUE 64u
enum { HM_OK, HM_CAPACITY, HM_DESCRIPTOR, HM_PACKET, HM_USB, HM_QUIESCE, HM_BLOCKED };
typedef struct {
    uint8_t addr,cfg,iface,alt,ep,mps,cables,toggle,failed;
    uint16_t vid,pid,path;
} hm_pipe_t;
typedef struct {uint8_t pipe,cable,jack;} hm_port_t;
typedef struct {uint32_t w,r,data[HM_QUEUE];} hm_queue_t;
static hm_pipe_t hm_pipes[HM_PIPES];
static hm_port_t hm_inputs[HM_PORTS],hm_outputs[HM_PORTS];
static hm_queue_t hm_inq[HM_PORTS],hm_outq[HM_PIPES];
static uint32_t hm_routes[HM_PORTS],hm_old_keys[2*HM_PORTS][3];
static uint8_t hm_npipe,hm_nin,hm_nout,hm_old_in,hm_old_out;
static uint8_t hm_in_sysex[HM_PORTS],hm_owner[HM_PORTS];
static volatile uint8_t hm_forward,hm_stopping;
static uint8_t hm_fault,hm_changed,hm_panic_port,hm_panic_item,hm_panic_done;
static uint32_t hm_rx_packets,hm_tx_packets,hm_events,hm_fanout,hm_idle_polls;
static uint32_t hm_retries,hm_invalid,hm_backpressure,hm_cleanup_bad,hm_used_outputs;

static uint32_t hm_fill(const hm_queue_t *q){return q->w-q->r;}
static void hm_put(hm_queue_t *q,uint32_t word){q->data[q->w++&(HM_QUEUE-1)]=word;}
static uint32_t hm_word(const hm_queue_t *q){return q->data[q->r&(HM_QUEUE-1)];}
static uint32_t hm_all(void){return hm_nout==32?0xffffffffu:(1u<<hm_nout)-1u;}
static int hm_route_set(uint32_t input,uint32_t mask)
{
    if(input>=hm_nin || hm_forward || hm_stopping || (mask&~hm_all()))return 0;
    hm_routes[input]=mask;return 1;
}
static void hm_key(const hm_port_t *port,uint32_t key[3])
{
    const hm_pipe_t *p=&hm_pipes[port->pipe];
    key[0]=(uint32_t)p->vid|(uint32_t)p->pid<<16;
    key[1]=(uint32_t)p->path|(uint32_t)p->iface<<16|(uint32_t)p->alt<<24;
    key[2]=(uint32_t)p->ep|(uint32_t)port->cable<<8|(uint32_t)port->jack<<16;
}
static void hm_discovery_begin(void)
{
    uint32_t i;hm_old_in=hm_nin;hm_old_out=hm_nout;
    for(i=0;i<hm_nin;i++)hm_key(&hm_inputs[i],hm_old_keys[i]);
    for(i=0;i<hm_nout;i++)hm_key(&hm_outputs[i],hm_old_keys[HM_PORTS+i]);
    hm_npipe=hm_nin=hm_nout=hm_fault=hm_forward=hm_stopping=hm_changed=0;
    hm_rx_packets=hm_tx_packets=hm_events=hm_fanout=hm_idle_polls=0;
    hm_retries=hm_invalid=hm_backpressure=hm_cleanup_bad=hm_used_outputs=0;
    memset(hm_pipes,0,sizeof hm_pipes);memset(hm_inq,0,sizeof hm_inq);memset(hm_outq,0,sizeof hm_outq);
    memset(hm_owner,0,sizeof hm_owner);memset(hm_in_sysex,0,sizeof hm_in_sysex);
}
static void hm_discovery_end(void)
{
    uint32_t i,k,key[3];hm_changed=hm_nin!=hm_old_in || hm_nout!=hm_old_out;
    for(i=0;i<hm_nin;i++){
        hm_key(&hm_inputs[i],key);for(k=0;k<3;k++)if(key[k]!=hm_old_keys[i][k])hm_changed=1;
    }
    for(i=0;i<hm_nout;i++){
        hm_key(&hm_outputs[i],key);for(k=0;k<3;k++)if(key[k]!=hm_old_keys[HM_PORTS+i][k])hm_changed=1;
    }
    if(hm_changed)memset(hm_routes,0,sizeof hm_routes);
}
static uint32_t hm_fingerprint(uint32_t routes)
{
    uint32_t h=2166136261u,i,j,key[3];
    h=(h^hm_nin)*16777619u;h=(h^hm_nout)*16777619u;
    for(i=0;i<hm_nin+hm_nout;i++){
        hm_key(i<hm_nin?&hm_inputs[i]:&hm_outputs[i-hm_nin],key);
        for(j=0;j<3;j++)h=(h^key[j])*16777619u;
        if(routes && i<hm_nin)h=(h^hm_routes[i])*16777619u;
    }
    return h;
}

/* A class-specific endpoint lists the embedded jacks in virtual-cable order.
 * Require MIDI1 header + complete bulk endpoint associations; no cable0 guess. */
static int hm_parse(const uint8_t *p,uint32_t n,const hp_device_t *dev)
{
    uint32_t at=0,midi=0,header=0,ep=0,mps=0,iface=0,alt=0,start=hm_npipe;
    if(n<9 || p[0]!=9 || p[1]!=2 || (uint32_t)(p[2]|p[3]<<8)!=n)return 0;
    while(at<n){
        const uint8_t *d=p+at;uint32_t l=d[0];
        if(l<2 || l>n-at){hm_fault=HM_DESCRIPTOR;return -1;}
        if(d[1]==4 && l>=9){
            midi=d[5]==1 && d[6]==3 && d[7]==0 && d[3]==0;
            iface=d[2];alt=d[3];header=ep=mps=0;
        }else if(midi && d[1]==0x24 && l>=7 && d[2]==1){header=d[3]==0 && d[4]==1;
        }else if(midi && d[1]==5 && l>=7){
            ep=mps=0;
            if(header && (d[3]&3)==2 && (d[2]&15) && !(d[2]&0x70)){
                uint32_t size=d[4]|(uint32_t)d[5]<<8;
                if(size==8 || size==16 || size==32 || size==64){ep=d[2];mps=size;}
            }
        }else if(midi && header && ep && d[1]==0x25 && l>=4 && d[2]==1){
            uint32_t count=d[3],j;uint8_t *total=(ep&128)?&hm_nin:&hm_nout;
            hm_port_t *ports=(ep&128)?hm_inputs:hm_outputs;
            if(!count || count>16 || l!=4+count){hm_fault=HM_DESCRIPTOR;return -1;}
            if(hm_npipe==HM_PIPES || *total+count>HM_PORTS){hm_fault=HM_CAPACITY;return -1;}
            for(j=0;j<count;j++){
                if(!d[4+j]){hm_fault=HM_DESCRIPTOR;return -1;}
                for(uint32_t k=0;k<j;k++)if(d[4+j]==d[4+k]){hm_fault=HM_DESCRIPTOR;return -1;}
            }
            for(j=0;j<hm_npipe;j++)if(hm_pipes[j].addr==dev->addr && hm_pipes[j].ep==ep){hm_fault=HM_DESCRIPTOR;return -1;}
            hm_pipes[hm_npipe]=(hm_pipe_t){.addr=dev->addr,.cfg=p[5],.iface=(uint8_t)iface,.alt=(uint8_t)alt,
                .ep=(uint8_t)ep,.mps=(uint8_t)mps,.cables=(uint8_t)count,.vid=dev->vid,.pid=dev->pid,.path=dev->path};
            for(j=0;j<count;j++)ports[(*total)++]=(hm_port_t){.pipe=hm_npipe,.cable=(uint8_t)j,.jack=d[4+j]};
            hm_npipe++;ep=0;
        }
        at+=l;
    }
    return hm_npipe!=start;
}
static int hm_input_port(uint32_t pipe,uint32_t cable)
{
    for(uint32_t i=0;i<hm_nin;i++)if(hm_inputs[i].pipe==pipe && hm_inputs[i].cable==cable)return (int)i;
    return -1;
}
static int hm_can_read(uint32_t pipe)
{
    for(uint32_t i=0;i<hm_nin;i++)if(hm_inputs[i].pipe==pipe && hm_fill(&hm_inq[i])>HM_QUEUE-16)return 0;
    return 1;
}
static int hm_receive(uint32_t pipe,const uint8_t *data,uint32_t len)
{
    uint8_t ports[16];uint32_t i;
    if(pipe>=hm_npipe || len>hm_pipes[pipe].mps || len>64 || (len&3)){hm_fault=HM_PACKET;hm_invalid++;return 0;}
    if(!hm_can_read(pipe)){hm_fault=HM_BLOCKED;hm_backpressure++;return 0;}
    for(i=0;i<len/4;i++){
        int port=hm_input_port(pipe,data[4*i]>>4);
        if(port<0){hm_fault=HM_PACKET;hm_invalid++;return 0;}ports[i]=(uint8_t)port;
    }
    for(i=0;i<len/4;i++){
        const uint8_t *p=data+4*i;
        hm_put(&hm_inq[ports[i]],p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24);
    }
    hm_rx_packets++;return 1;
}

static int hm_validate(uint32_t word,uint32_t source,uint8_t *next,uint8_t *start,uint8_t *end,uint8_t *rt)
{
    uint32_t cin=word&15,a=(word>>8)&255,b=(word>>16)&255,c=word>>24;
    uint8_t sx=hm_in_sysex[source];*next=sx;*start=*end=*rt=0;
    if(cin==15){*rt=1;return (a==0xf8 || a==0xfa || a==0xfb || a==0xfc || a==0xfe || a==0xff) && !b && !c;}
    if(cin==4){
        if(b>=128 || c>=128 || !(a<128 || a==0xf0) || (a==0xf0?sx:!sx))return 0;
        *start=!sx;*next=1;return 1;
    }
    if(cin==6 || cin==7){
        if(!(a<128 || a==0xf0) || (a==0xf0?sx:!sx))return 0;
        if(cin==6 ? (b!=0xf7 || c) : (b>=128 || c!=0xf7))return 0;
        *start=!sx;*end=1;*next=0;return 1;
    }
    if(cin==5 && a==0xf7 && sx && !b && !c){*end=1;*next=0;return 1;}
    if(sx)return 0;
    if(cin>=8 && cin<=14)return (a>>4)==cin && b<128 && ((cin==12 || cin==13)?!c:c<128);
    if(cin==2)return (a==0xf1 || a==0xf3) && b<128 && !c;
    if(cin==3)return a==0xf2 && b<128 && c<128;
    return cin==5 && a==0xf6 && !b && !c;
}
static int hm_route_event(uint32_t source)
{
    hm_queue_t *in=&hm_inq[source];uint8_t needed[HM_PIPES]={0},next,start,end,rt;
    uint32_t word,mask,i;
    if(!hm_fill(in))return 0;
    word=hm_word(in);mask=hm_routes[source];
    if(!hm_validate(word,source,&next,&start,&end,&rt)){hm_invalid++;hm_fault=HM_PACKET;return -1;}
    for(i=0;i<hm_nout;i++)if(mask&(1u<<i)){
        if(!rt && hm_owner[i] && hm_owner[i]!=source+1){hm_backpressure++;return 0;}
        needed[hm_outputs[i].pipe]++;
    }
    for(i=0;i<hm_npipe;i++)if(needed[i] && (hm_pipes[i].failed || hm_fill(&hm_outq[i])+needed[i]>HM_QUEUE)){
        hm_backpressure++;return 0;
    }
    for(i=0;i<hm_nout;i++)if(mask&(1u<<i)){
        hm_port_t *dest=&hm_outputs[i];hm_used_outputs|=1u<<i;
        hm_put(&hm_outq[dest->pipe],(word&~0xf0u)|((uint32_t)dest->cable<<4));hm_fanout++;
        if(start && !end)hm_owner[i]=(uint8_t)(source+1);
        if(end)hm_owner[i]=0;
    }
    hm_in_sysex[source]=next;in->r++;hm_events++;return 1;
}
static void hm_pump(void)
{
    if(!hm_forward || hm_fault)return;
    /* At most one event per logical input each pass: independent cable progress. */
    for(uint32_t i=0;i<hm_nin && !hm_fault;i++)hm_route_event(i);
}
static uint32_t hm_tx_packet(uint32_t pipe,uint8_t *data)
{
    hm_queue_t *q=&hm_outq[pipe];uint32_t n=hm_fill(q),max=hm_pipes[pipe].mps/4,i;
    if(n>max)n=max;
    for(i=0;i<n;i++){
        uint32_t w=q->data[(q->r+i)&(HM_QUEUE-1)];
        for(uint32_t j=0;j<4;j++)data[4*i+j]=(uint8_t)(w>>(8*j));
    }
    return n*4;
}
static void hm_tx_ack(uint32_t pipe,uint32_t events)
{
    if(events>hm_fill(&hm_outq[pipe])){hm_fault=HM_USB;return;}
    hm_outq[pipe].r+=events;hm_tx_packets++;
}
static void hm_panic_begin(void)
{
    hm_forward=0;hm_stopping=1;hm_panic_done=0;hm_panic_port=hm_panic_item=0;
    memset(hm_inq,0,sizeof hm_inq);memset(hm_outq,0,sizeof hm_outq);
}
static void hm_panic_pump(void)
{
    uint32_t count=0;
    while(hm_stopping && hm_panic_port<hm_nout && count++<16){
        hm_port_t *port=&hm_outputs[hm_panic_port];hm_queue_t *q=&hm_outq[port->pipe];uint32_t word;
        if(!(hm_used_outputs&(1u<<hm_panic_port))){hm_panic_port++;hm_panic_item=0;continue;}
        if(hm_pipes[port->pipe].failed){hm_cleanup_bad++;hm_panic_port++;hm_panic_item=0;continue;}
        if(hm_fill(q)==HM_QUEUE)return;
        if(!hm_panic_item && hm_owner[hm_panic_port]){
            word=5u|0xf7u<<8;hm_owner[hm_panic_port]=0;
        }else{
            uint32_t item=hm_panic_item?hm_panic_item-1:0,channel=item/3;
            uint32_t cc=item%3==0?64:item%3==1?123:120;
            word=11u|((0xb0u|channel)<<8)|(cc<<16);
            if(!hm_panic_item)hm_panic_item=1;
            hm_panic_item++;
        }
        hm_put(q,word|((uint32_t)port->cable<<4));
        if(hm_panic_item==49){hm_panic_port++;hm_panic_item=0;}
    }
    if(hm_panic_port==hm_nout)hm_panic_done=1;
}
static int hm_empty(void)
{
    for(uint32_t i=0;i<hm_npipe;i++)if(!(hm_pipes[i].ep&128) && !hm_pipes[i].failed && hm_fill(&hm_outq[i]))return 0;
    return 1;
}
