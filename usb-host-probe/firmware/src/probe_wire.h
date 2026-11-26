/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
static uint32_t hp_frozen[HP_FIELDS],hp_frozen_meta[18],hp_frozen_crc;
static uint8_t hp_frozen_valid;

static uint32_t hp_crc_bytes(const uint8_t *p,uint32_t n,uint32_t c)
{
    uint32_t j;while(n--){c^=*p++;for(j=0;j<8;j++)c=(c>>1)^((c&1)?0xedb88320u:0);}return c;
}
static uint32_t hp_crc(void)
{
    uint32_t c=hp_crc_bytes((const uint8_t *)hp_frozen,sizeof hp_frozen,0xffffffffu);
    return ~hp_crc_bytes((const uint8_t *)hp_frozen_meta,sizeof hp_frozen_meta,c);
}
static void hp_freeze(void)
{
    uint32_t i;memcpy(hp_frozen,hp_v,sizeof hp_v);
    for(i=0;i<3;i++) {
        hp_stream_t *s=&hp_stream[i];uint32_t *m=hp_frozen_meta+6*i;
        m[0]=s->vid;m[1]=s->pid;m[2]=s->path;m[3]=s->maxp;m[4]=s->sync;
        m[5]=hp_stream_id(s);
    }
    hp_frozen_crc=hp_crc();hp_frozen_valid=1;
}
/* Custom non-commercial SysEx 7D 55 48 01. ARM only stores an identity/config;
 * it never enters host/update mode. READ is immutable until an explicit new ARM. */
static int hp_sysex(const uint8_t *p,uint32_t n)
{
#if defined(DINGHY_HOST) && !defined(HP_WIRE_TEST)
    (void)p;(void)n;return 0;
#else
    uint8_t out[464],raw[384];uint32_t cmd,count=7,len=0,run,duration,rate;
    if(n<4 || p[0]!=0x7d || p[1]!=0x55 || p[2]!=0x48 || p[3]!=1)return 0;
    cmd=n>4?p[4]:0;
    out[0]=0xf0;out[1]=0x7d;out[2]=0x55;out[3]=0x48;out[4]=1;out[5]=(uint8_t)(cmd|0x40);out[6]=0;
    if(hp_owns_usb)out[6]=1;
    else if(cmd==1 && n==17) {
        run=p[5]|(uint32_t)p[6]<<7|(uint32_t)p[7]<<14|(uint32_t)p[8]<<21;
        duration=p[9]|(uint32_t)p[10]<<7|(uint32_t)p[11]<<14;
        rate=p[12]|(uint32_t)p[13]<<7|(uint32_t)p[14]<<14;
        if(!run || (rate!=44100 && rate!=48000) || (duration!=10000 && duration!=60000 && duration!=300000) ||
           p[15]>100 || p[16]!=0)out[6]=2;
        else if(hp_frozen_valid && hp_frozen[H_RUN]!=run)out[6]=3; /* collect/ACK first */
        else if(hp_frozen_valid)out[6]=4; /* completed attempt cannot run twice */
        else {
            hp_v[H_MAGIC]=0x31504855u;hp_v[H_BUILD]=HP_BUILD_TAG;hp_v[H_RUN]=run;
            hp_v[H_RATE]=rate;hp_v[H_DURATION]=duration;hp_v[H_STATE]=HP_ARMED;hp_gain=p[15];
            hp_audio_enabled=1;
        }
    } else if(cmd==2 && n==5) {
        if(!hp_frozen_valid)out[6]=3;
        else {
            memcpy(raw,hp_frozen,sizeof hp_frozen);memcpy(raw+sizeof hp_frozen,hp_frozen_meta,sizeof hp_frozen_meta);
            ota_wr32(raw+sizeof hp_frozen+sizeof hp_frozen_meta,hp_frozen_crc);
            len=sizeof hp_frozen+sizeof hp_frozen_meta+4;
        }
    } else if(cmd==3 && n==13) {
        run=p[5]|(uint32_t)p[6]<<7|(uint32_t)p[7]<<14|(uint32_t)p[8]<<21;
        uint32_t ack=p[9]|(uint32_t)p[10]<<7|(uint32_t)p[11]<<14|(uint32_t)p[12]<<21;
        if(!hp_frozen_valid || run!=hp_frozen[H_RUN] || ack!=(hp_frozen_crc&0x0fffffffu))out[6]=2;
        else {hp_frozen_valid=0;hp_v[H_STATE]=HP_IDLE;}
    } else if(cmd==4 && n==5) {
        ota_wr32(raw,HP_BUILD_TAG);ota_wr32(raw+4,hp_v[H_STATE]);ota_wr32(raw+8,hp_v[H_RUN]);
        ota_wr32(raw+12,hp_frozen_valid);len=16;
    } else if(cmd==5 && n==5){
        ota_wr32(raw,hm_nin);ota_wr32(raw+4,hm_nout);ota_wr32(raw+8,hm_npipe);ota_wr32(raw+12,hm_changed);len=16;
    } else if(cmd==6 && n==7 && p[5]<=1){
        uint32_t index=p[6],total=p[5]?hm_nout:hm_nin;
        if(index>=total)out[6]=2;
        else{
            hm_port_t *port=p[5]?&hm_outputs[index]:&hm_inputs[index];uint32_t key[3];hm_pipe_t *pipe=&hm_pipes[port->pipe];
            hm_key(port,key);for(uint32_t i=0;i<3;i++)ota_wr32(raw+4*i,key[i]);
            ota_wr32(raw+12,pipe->addr);ota_wr32(raw+16,pipe->cfg);ota_wr32(raw+20,pipe->mps);
            ota_wr32(raw+24,p[5]?0:hm_routes[index]);len=28;
        }
    } else if(cmd==7 && n==11 && p[10]<=15){
        uint32_t mask=p[6]|(uint32_t)p[7]<<7|(uint32_t)p[8]<<14|(uint32_t)p[9]<<21|(uint32_t)p[10]<<28;
        if(!hm_route_set(p[5],mask))out[6]=2;
    } else if(cmd==8 && n==5){
        ota_wr32(raw,hm_nin);ota_wr32(raw+4,hm_nout);len=8;
        for(uint32_t i=0;i<hm_nin;i++){ota_wr32(raw+len,hm_routes[i]);len+=4;}
    } else out[6]=2;
    if(len)count+=ota_pack7(raw,len,out+count);
    out[count++]=0xf7;ota_wire_send(out,count);return 1;
#endif
}

