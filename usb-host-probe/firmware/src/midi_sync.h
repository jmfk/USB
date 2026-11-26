/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
static uint8_t hm_sync_active,hm_sync_cc_on,hm_sync_cc_stop_pending,hm_sync_cc_blocked;
static uint32_t hm_sync_phase,hm_sync_last,hm_sync_clocks,hm_sync_skipped,hm_sync_rejected,hm_sync_late;
static int hm_sync_emit(uint32_t word)
{
 uint32_t out=hm_sync_config.output;
 if(out>=hm_nout)return 0;
 hm_port_t *p=&hm_outputs[out];hm_queue_t *q=&hm_outq[p->pipe];
 if(hm_pipes[p->pipe].failed||hm_fill(q)>=HM_QUEUE)return 0;
 if((word&15)!=15&&hm_owner[out])return 0;
 hm_put(q,(word&~0xf0u)|((uint32_t)p->cable<<4));hm_used_outputs|=1u<<out;return 1;
}
static void hm_sync_begin(uint32_t ticks)
{
 hm_sync_active=hm_sync_config.enabled;hm_sync_phase=0;hm_sync_last=ticks;
 hm_sync_cc_on=hm_sync_cc_stop_pending=hm_sync_cc_blocked=0;hm_sync_clocks=hm_sync_skipped=hm_sync_rejected=hm_sync_late=0;
 if(hm_sync_active&&!hm_sync_emit(15u|(0xfau<<8))){hm_sync_active=0;hm_fault=HM_BLOCKED;}
}
static void hm_sync_tick(uint32_t ticks,uint32_t ticks_per_us)
{
 uint32_t elapsed=ticks-hm_sync_last;hm_sync_last=ticks;
 if(!hm_sync_active||!hm_forward||hm_stopping||hm_fault)return;
 /* Bound arithmetic after a stalled service; no clock catch-up burst. */
 if(elapsed>10000u*ticks_per_us){hm_sync_phase=0;hm_sync_late++;return;}
 uint32_t threshold=60000000u*ticks_per_us;
 hm_sync_phase+=elapsed*hm_sync_config.bpm*24u;
 if(hm_sync_phase>=threshold){
  uint32_t due=hm_sync_phase/threshold;hm_sync_phase%=threshold;
  if(hm_sync_emit(15u|(0xf8u<<8))){hm_sync_clocks++;hm_sync_skipped+=due-1;}else hm_sync_skipped+=due;
 }
}
static int hm_sync_cc(void)
{
 if(!hm_sync_active||!hm_forward||hm_stopping||hm_fault)return 0;
 uint32_t on=!hm_sync_cc_on,value=on?hm_sync_config.on:hm_sync_config.off;
 if(!hm_sync_emit(11u|((0xb0u|hm_sync_config.channel)<<8)|((uint32_t)hm_sync_config.cc<<16)|(value<<24))){hm_sync_rejected++;hm_sync_cc_blocked=1;return 0;}
 hm_sync_cc_blocked=0;hm_sync_cc_on=(uint8_t)on;return 1;
}
static void hm_sync_cleanup(void)
{
 if(hm_sync_active){
  hm_sync_active=0;hm_sync_cc_stop_pending=hm_sync_cc_on;
  if(!hm_sync_emit(15u|(0xfcu<<8)))hm_cleanup_bad++;
 }
}
/* Queue the configured CC-off after the panic pump has queued F7/cleanup.
 * Existing queue/ACK completion must drain it before host ownership is released. */
static void hm_sync_finish_cc(void)
{
 if(!hm_sync_cc_stop_pending)return;
 uint32_t out=hm_sync_config.output;
 if(out>=hm_nout||hm_pipes[hm_outputs[out].pipe].failed){hm_cleanup_bad++;hm_sync_cc_stop_pending=0;return;}
 if(hm_sync_emit(11u|((0xb0u|hm_sync_config.channel)<<8)|((uint32_t)hm_sync_config.cc<<16)|((uint32_t)hm_sync_config.off<<24))){hm_sync_cc_on=0;hm_sync_cc_stop_pending=0;}
}
