/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#define HP_DIAG_LOG 15u
#define HP_DIAG_WORDS (29u+4u*HP_DIAG_LOG)
static uint32_t hp_diag[HP_DIAG_WORDS],hp_diag_log[HP_DIAG_LOG][4];
static uint32_t hp_diag_nonce[4];
static uint32_t hp_diag_seq,hp_diag_written,hp_diag_begin_ms,hp_diag_last_ms,hp_diag_last_fault;
static uint8_t hp_diag_started,hp_diag_stopped,hp_diag_valid;
static void hp_diag_event(uint32_t now,uint32_t kind,uint32_t a,uint32_t b)
{
 uint32_t *p=hp_diag_log[hp_diag_written++%HP_DIAG_LOG];p[0]=now-hp_diag_begin_ms;p[1]=kind;p[2]=a;p[3]=b;
 hp_diag[22]=hp_diag_written<HP_DIAG_LOG?hp_diag_written:HP_DIAG_LOG;hp_diag[23]=hp_diag_written-hp_diag[22];
}
static void hp_diag_begin(uint32_t now,uint32_t build)
{
 memset(hp_diag,0,sizeof hp_diag);memset(hp_diag_log,0,sizeof hp_diag_log);
 hp_diag_written=0;hp_diag_begin_ms=hp_diag_last_ms=now;hp_diag_last_fault=0;hp_diag_started=1;hp_diag_stopped=0;
 hp_diag[0]=0x32445048u;hp_diag[1]=build;hp_diag[2]=hp_diag_seq=(hp_diag_seq+1)&0x0fffffffu;
 hp_diag[6]=hm_fingerprint(0);hp_diag[7]=hm_fingerprint(1);hp_diag[24]=hp_audio_enabled;
 memcpy(hp_diag+25,hp_diag_nonce,sizeof hp_diag_nonce);memset(hp_diag_nonce,0,sizeof hp_diag_nonce);
 hp_diag_event(now,1,hm_nin,hm_nout);
}
static void hp_diag_sample(uint32_t now,uint32_t us)
{
 if(!hp_diag_started)return;
 hp_diag[20]++;if(us>hp_diag[19])hp_diag[19]=us;
 if(0xffffffffu-hp_diag[21]<us)hp_diag[21]=0xffffffffu;else hp_diag[21]+=us;
 for(uint32_t i=0;i<hm_nin;i++){uint32_t n=hm_fill(&hm_inq[i]);if(n>hp_diag[17])hp_diag[17]=n;}
 for(uint32_t i=0;i<hm_npipe;i++){uint32_t n=hm_fill(&hm_outq[i]);if(n>hp_diag[18])hp_diag[18]=n;}
 if(hm_fault!=hp_diag_last_fault){hp_diag_event(now,3,hm_fault,hm_invalid);hp_diag_last_fault=hm_fault;}
 if(now-hp_diag_last_ms>=1000){hp_diag_event(now,2,hm_rx_packets,hm_tx_packets);hp_diag_last_ms=now;}
}
static void hp_diag_stop(uint32_t now,uint32_t state,uint32_t reason)
{
 if(!hp_diag_started||hp_diag_stopped)return;
 hp_diag_stopped=1;hp_diag[3]=now-hp_diag_begin_ms;hp_diag[4]=state;hp_diag[5]=reason;
 hp_diag[8]=hm_rx_packets;hp_diag[9]=hm_tx_packets;hp_diag[10]=hm_events;hp_diag[11]=hm_fanout;
 hp_diag[12]=hm_retries;hp_diag[13]=hm_backpressure;hp_diag[14]=hm_invalid;hp_diag[15]=hm_fault;
 hp_diag_event(now,4,state,reason);
}
static void hp_diag_finish(uint32_t now,uint32_t state,uint32_t reason)
{
 if(!hp_diag_started||hp_diag_valid)return;
 hp_diag_stop(now,state,reason);
 if(!(hp_diag[25]|hp_diag[26]|hp_diag[27]|hp_diag[28])){hp_diag_started=0;return;}
 if(hm_cleanup_bad||hp_diag[4]!=state||hp_diag[5]!=reason)hp_diag_event(now,5,state,hm_cleanup_bad);hp_diag[4]=state;hp_diag[5]=reason;hp_diag[16]=hm_cleanup_bad;
 uint32_t count=hp_diag_written<HP_DIAG_LOG?hp_diag_written:HP_DIAG_LOG,start=hp_diag_written-count;
 hp_diag[22]=count;hp_diag[23]=start;
 for(uint32_t i=0;i<count;i++)memcpy(hp_diag+29+4*i,hp_diag_log[(start+i)%HP_DIAG_LOG],16);
 hp_diag_valid=1;hp_diag_started=0;
}
