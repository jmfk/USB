/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "probe_core.h"
static void setup(uint32_t bpm){hm_discovery_begin();hm_npipe=1;hm_nout=2;hm_pipes[0]=(hm_pipe_t){.ep=1,.mps=64};hm_outputs[0]=(hm_port_t){.pipe=0,.cable=0};hm_outputs[1]=(hm_port_t){.pipe=0,.cable=1};hm_sync_config=(hm_sync_config_t){bpm,1,1,2,20,127,0};hm_forward=1;}
int main(void){
 for(uint32_t tpu=1;tpu<=24;tpu+=23)for(uint32_t bpm=30;bpm<=300;bpm+=30){
  setup(bpm);uint32_t now=0xfffff000u;hm_sync_begin(now);assert(hm_word(&hm_outq[0])==0x0000fa1fu);hm_tx_ack(0,1);
  for(uint32_t i=0;i<10000;i++){now+=100*tpu;hm_sync_tick(now,tpu);if(hm_fill(&hm_outq[0])){assert(hm_fill(&hm_outq[0])==1&&hm_word(&hm_outq[0])==0x0000f81fu);hm_tx_ack(0,1);}}
  assert(hm_sync_clocks==bpm*24/60&&!hm_sync_skipped&&!hm_sync_late);
 }
 setup(120);hm_sync_begin(0);hm_tx_ack(0,1);hm_owner[1]=1;
 assert(!hm_sync_cc()&&!hm_sync_cc_on&&hm_sync_rejected==1);
 hm_sync_phase=60000000-1;hm_sync_tick(100,1);assert(hm_word(&hm_outq[0])==0x0000f81fu);hm_tx_ack(0,1);
 hm_owner[1]=0;assert(hm_sync_cc()&&hm_sync_cc_on);assert(hm_word(&hm_outq[0])==0x7f14b21bu);hm_tx_ack(0,1);
 for(uint32_t i=0;i<64;i++)hm_put(&hm_outq[0],0);
 assert(!hm_sync_cc()&&hm_sync_cc_on);hm_sync_phase=60000000-1;hm_sync_tick(200,1);assert(hm_sync_skipped==1&&hm_fill(&hm_outq[0])==64);
 hm_tx_ack(0,64);hm_sync_tick(50000,1);assert(hm_sync_late==1&&!hm_fill(&hm_outq[0]));
 hm_panic_begin();hm_sync_cleanup();assert(hm_word(&hm_outq[0])==0x0000fc1fu&&!hm_sync_active);
 while(!hm_panic_done)hm_panic_pump();hm_sync_finish_cc();assert(!hm_sync_cc_on&&!hm_sync_cc_stop_pending);
 assert(hm_fill(&hm_outq[0])==50);uint32_t last=(hm_outq[0].w-1)&63;assert(hm_outq[0].data[last]==0x0014b21bu);
 setup(120);hm_sync_config.enabled=0;hm_sync_begin(0);hm_sync_tick(25000,1);assert(!hm_sync_active&&!hm_fill(&hm_outq[0]));
 puts("PASS: sync24PPQN across tick wrap/tempo range, explicit Start/Stop/CC, SysEx ownership, backpressure, no burst and CC-off cleanup");return 0;
}
