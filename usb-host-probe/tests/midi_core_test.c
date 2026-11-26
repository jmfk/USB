/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "probe_core.h"

static const uint8_t descriptor[]={9,2,55,0,1,1,0,0x80,50,
    9,4,2,0,2,1,3,0,0,7,0x24,1,0,1,37,0,
    9,5,0x82,2,64,0,0,0,0,6,0x25,1,2,1,2,
    9,5,2,2,64,0,0,0,0,6,0x25,1,2,3,4};
static void discover(uint32_t path)
{
    hp_device_t dev={.addr=2,.vid=0x1234,.pid=0x5678,.path=(uint16_t)path};
    hm_discovery_begin();assert(hm_parse(descriptor,sizeof descriptor,&dev)==1);
    hm_discovery_end();assert(hm_nin==2 && hm_nout==2 && hm_npipe==2);
}
static void feed(uint32_t input,uint32_t word){hm_put(&hm_inq[input],word);}
int main(void)
{
    uint8_t bad[sizeof descriptor],packet[64];hp_device_t dev={.addr=3};
    discover(1);assert(hm_changed && !hm_routes[0] && !hm_routes[1]);
    assert(hm_route_set(0,hm_all()) && hm_routes[0]==3);
    assert(!hm_route_set(2,1) && !hm_route_set(0,4));
    discover(1);assert(!hm_changed && hm_routes[0]==3);
    discover(2);assert(hm_changed && !hm_routes[0]);
    hm_route_set(0,3);hm_forward=1;
    feed(0,0x643c9009u);hm_pump();assert(hm_events==1 && hm_fanout==2);
    assert(hm_tx_packet(1,packet)==8);
    assert(packet[0]==9 && packet[4]==0x19 && !memcmp(packet+1,packet+5,3));
    assert(hm_fill(&hm_outq[1])==2); /* packet preparation never removes data */
    hm_tx_ack(1,2);assert(!hm_fill(&hm_outq[1]));
    assert(!hm_route_set(0,0));

    /* A fan-out that needs two slots must wait atomically when only one is free. */
    for(uint32_t i=0;i<HM_QUEUE-1;i++)hm_put(&hm_outq[1],0);
    feed(0,0x003c8008u);hm_pump();assert(hm_fill(&hm_inq[0])==1 && hm_fill(&hm_outq[1])==63);
    hm_tx_ack(1,63);hm_pump();assert(!hm_fill(&hm_inq[0]) && hm_fill(&hm_outq[1])==2);

    /* Independent input cables allow the owner of a merged SysEx to finish. */
    discover(2);hm_route_set(0,1);hm_route_set(1,1);hm_forward=1;
    feed(1,0x0201f014u);hm_pump();assert(hm_owner[0]==2);
    feed(0,0x643c9009u);feed(1,0x0000f81fu);hm_pump();
    assert(hm_fill(&hm_inq[0])==1 && hm_events==2); /* realtime inside SysEx */
    feed(1,0x0000f715u);hm_pump();assert(!hm_owner[0]);hm_pump();
    assert(!hm_fill(&hm_inq[0]) && hm_events==4);
    uint32_t read=hm_outq[1].r;
    assert(hm_outq[1].data[read]==0x0201f004u);
    assert(hm_outq[1].data[read+1]==0x0000f80fu);
    assert(hm_outq[1].data[read+2]==0x0000f705u);
    assert(hm_outq[1].data[read+3]==0x643c9009u);
    feed(1,0x0201f014u);hm_pump();hm_panic_begin();
    while(!hm_panic_done)hm_panic_pump();
    assert(hm_fill(&hm_outq[1])==49 && hm_word(&hm_outq[1])==0x0000f705u);
    hm_tx_ack(1,1);
    for(uint32_t ch=0;ch<16;ch++)for(uint32_t j=0;j<3;j++){
        uint32_t cc=j==0?64:j==1?123:120;
        assert(hm_word(&hm_outq[1])==(11u|((0xb0u|ch)<<8)|(cc<<16)));hm_tx_ack(1,1);
    }
    assert(hm_empty() && !hm_cleanup_bad); /* unused output cable received no panic */

    discover(2);hm_forward=1;hm_used_outputs=1;hm_pipes[1].failed=1;
    hm_panic_begin();hm_panic_pump();assert(hm_cleanup_bad==1 && hm_panic_done);
    discover(2);hm_forward=1;feed(0,0x00000009u);hm_pump();assert(hm_fault==HM_PACKET && hm_invalid==1);
    discover(2);memcpy(bad,descriptor,sizeof bad);bad[54]=bad[53];
    assert(hm_parse(bad,sizeof bad,&dev)<0 && hm_fault==HM_DESCRIPTOR);
    hm_discovery_begin();memcpy(bad,descriptor,sizeof bad);bad[16]=2;
    assert(!hm_parse(bad,sizeof bad,&dev) && !hm_npipe); /* MIDI2 protocol excluded */
    hm_discovery_begin();memcpy(bad,descriptor,sizeof bad);bad[54]=0;
    assert(hm_parse(bad,sizeof bad,&dev)<0);
    hm_discovery_begin();
    for(uint32_t i=0;i<16;i++){dev.addr=(uint8_t)(i+1);assert(hm_parse(descriptor,sizeof descriptor,&dev)==1);}
    assert(hm_nin==32 && hm_nout==32 && hm_all()==0xffffffffu);
    dev.addr=17;assert(hm_parse(descriptor,sizeof descriptor,&dev)<0 && hm_fault==HM_CAPACITY);
    puts("PASS: MIDI core: ports, topology, atomic fan-out, cable mapping, SysEx merge, backpressure, cleanup, malformed/capacity rejection");
    return 0;
}
