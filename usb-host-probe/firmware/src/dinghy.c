/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
static volatile uint8_t hp_owns_usb;
static void hp_tick(void);
static int hp_sysex(const uint8_t *,uint32_t);
#define FELUCCA_UAC 0
#define STORE_MAGIC 0x31504855u
#define APP_NAME "USB"
#define APP_SYSEX hp_sysex
#include "run_identity.h"
#include "platform.c"
#include "probe_core.h"
#ifndef DINGHY_HOST
#include "probe_usb.h"
#else
static uint8_t hp_running;
static void hp_tick(void) {}
static void hp_host_start(void)
{
    /* Display simulator fixtures only; USB behavior is tested in probe_test.c. */
    hp_device_t d={.addr=2,.vid=0x1209,.pid=1,.path=1};
    hm_discovery_begin();
    hm_pipes[0]=(hm_pipe_t){.addr=d.addr,.vid=d.vid,.pid=d.pid,.path=d.path,.ep=0x81,.mps=64,.cables=1};
    hm_pipes[1]=(hm_pipe_t){.addr=3,.vid=0x1935,.pid=0x102b,.path=2,.ep=1,.mps=64,.cables=1};
    hm_inputs[0]=(hm_port_t){.pipe=0};hm_outputs[0]=(hm_port_t){.pipe=1};
    hm_npipe=2;hm_nin=hm_nout=1;hm_discovery_end();hp_owns_usb=1;hp_v[H_STATE]=HP_READY;
}
static void hp_run_begin(void){hp_running=hm_forward=1;hp_v[H_STATE]=HP_RUNNING;}
static void hp_request_stop(uint32_t state,uint32_t reason){hp_running=hm_forward=0;hp_v[H_STATE]=state;hp_v[H_REASON]=reason;}
static void hp_host_stop(void){hp_owns_usb=0;}
#endif
#include "probe_wire.h"
static void app_audio(int32_t *out,uint32_t n) {(void)out;(void)n;}
static const char *const hp_states[]={"DEVICE / NOT ARMED","ARMED / PRESS PLAY","DISCOVERING USB","RUNNING","BUS TEST COMPLETE","TEST FAILED","STOPPED","READY / SET ROUTES"};
static const char *const hp_reasons[]={"", "NEED 2 IN + 1 OUT", "REGISTER TIMEOUT", "FRAME DEADLINE", "USB TRANSFER ERROR", "NO USB FRAME CLOCK", "BUFFER DRIFT/NO AUDIO", "ARM ON COMPUTER", "DEVICE/FORMAT SETUP", "INCOMPLETE RUN", "MIDI ROUTING ERROR", "NO MIDI IN/OUT"};
static void hp_text(uint32_t y,const char *text) {draw_text_box(4,y,232,&FONT_S,text,C_WHITE,0);}
static void hp_number(uint32_t y,const char *name,uint32_t value)
{
    char line[32],digits[12];uint32_t n=0,j=0;
    while(*name && n<18)line[n++]=*name++;
    line[n++]=' ';
    do {digits[j++]=(char)('0'+value%10);value/=10;}while(value);
    while(j)line[n++]=digits[--j];
    line[n]=0;hp_text(y,line);
}
static uint8_t hp_routes_view;
static uint32_t hp_input,hp_output_port;
static uint32_t hp_cycle(uint32_t value,int32_t delta,uint32_t count)
{
    int32_t v;if(!count)return 0;v=(int32_t)value+delta%(int32_t)count;
    if(v<0)v+=(int32_t)count;
    return (uint32_t)v%count;
}
static void hp_port_line(uint32_t y,const hm_port_t *port)
{
    hm_pipe_t *p=&hm_pipes[port->pipe];char line[28];const char *hex="0123456789ABCDEF";
    uint32_t k=0,v[3]={p->vid,p->pid,p->path};
    for(uint32_t j=0;j<3;j++){
        if(j)line[k++]=j==1?':':'@';
        for(uint32_t i=0;i<4;i++)line[k++]=hex[(v[j]>>(12-4*i))&15];
    }
    line[k++]=' ';line[k++]='C';line[k++]=hex[port->cable];line[k]=0;hp_text(y,line);
}
static void hp_port_heading(uint32_t y,const char *name,uint32_t index,const hm_port_t *port)
{
    const char *hex="0123456789ABCDEF";hm_pipe_t *p=&hm_pipes[port->pipe];
    char line[25];uint32_t n=0;
    while(*name)line[n++]=*name++;
    line[n++]=' ';line[n++]=(char)('0'+(index+1)/10);line[n++]=(char)('0'+(index+1)%10);
    line[n++]=' ';line[n++]='I';line[n++]=hex[p->iface>>4];line[n++]=hex[p->iface&15];
    line[n++]=' ';line[n++]='E';line[n++]=hex[p->ep>>4];line[n++]=hex[p->ep&15];line[n]=0;
    hp_text(y,line);
}
static void hp_draw(void)
{
    uint32_t state=hp_v[H_STATE],editable=!hp_running && !hm_stopping;
    if(state>HP_READY)state=HP_FAILED;
    lcd_fill(0,0,240,240,C_BLACK);
    hp_text(4,hp_audio_enabled?"USB AUDIO+MIDI " HP_LABEL:"USB MIDI ROUTER " HP_LABEL);hp_text(30,hp_states[state]);
    if(hp_routes_view){
        if(hm_nin){
            hp_input%=hm_nin;hp_port_heading(56,"IN",hp_input,&hm_inputs[hp_input]);hp_port_line(78,&hm_inputs[hp_input]);
            if(hm_nout){
                hp_output_port%=hm_nout;hp_port_heading(104,"OUT",hp_output_port,&hm_outputs[hp_output_port]);hp_port_line(126,&hm_outputs[hp_output_port]);
                hp_text(152,(hm_routes[hp_input]&(1u<<hp_output_port))?"ROUTE ON":"ROUTE OFF");
            }else hp_text(106,"NO MIDI OUTPUTS");
            hp_text(174,editable?"SELECT IN / ALGO OUT":"STOP TO EDIT ROUTES");
            if(editable)hp_text(188,"K1 RIGHT ON / LEFT OFF");
        }else hp_text(70,"DISCOVER MIDI FIRST");
    }else if(state==HP_IDLE){
        hp_text(62,hp_audio_enabled?"ARM VIA COMPUTER MIDI":"CONNECT POWERED HUB");
        hp_text(88,hp_audio_enabled?"MIDI: SELECT B2":"PLAY TO DISCOVER PORTS");
    }else if(state==HP_ARMED){
        hp_number(62,"RATE",hp_v[H_RATE]);hp_number(88,"SECONDS",hp_v[H_DURATION]/1000);hp_text(114,"POWERED HUB REQUIRED");
    }else{
        hp_number(58,hp_audio_enabled?"AUDIO IN A":"MIDI INPUTS",hp_audio_enabled?hp_v[H_A_PACKETS]:hm_nin);
        hp_number(82,hp_audio_enabled?"AUDIO IN B":"MIDI OUTPUTS",hp_audio_enabled?hp_v[H_B_PACKETS]:hm_nout);
        hp_number(106,"MIDI RX",hm_rx_packets);hp_number(130,"MIDI TX",hm_tx_packets);
        hp_text(154,hp_reasons[hp_v[H_REASON]<=HP_NO_MIDI?hp_v[H_REASON]:HP_IO]);
        if(!hp_owns_usb)hp_text(177,"RECONNECT PC / FETCH");
    }
    for(uint32_t i=0;i<12;i++){
        uint32_t x=(i%6)*40,y=204+(i/6)*18;const char *label="";
        if(!hp_owns_usb && (!hp_frozen_valid || !hp_frozen[H_RUN])){if(i==0)label="AUD";if(i==1)label="MIDI";}
        if(i==2)label=hp_routes_view?"STATS":"ROUTE";
        if(i==5 && (hp_routes_view || state==HP_READY))label="BACK";
        if(hp_routes_view && editable && hm_nin){if(i==6)label="ALL";if(i==7)label="NONE";}
        if(i==10)label=state==HP_READY?"PLAY":hp_owns_usb?"STOP":(!hp_audio_enabled || state==HP_ARMED)?"PLAY":"";
        draw_text_box(x,y,40,&FONT_S,label,C_WHITE,1);
    }
}
static void hp_controls(uint32_t pressed)
{
    int32_t a=fm1_enc_take(ENC_SELECT),b=fm1_enc_take(ENC_ALGORITHM),k=fm1_enc_take(ENC_KNOB1);
    hp_input=hp_cycle(hp_input,a,hm_nin);hp_output_port=hp_cycle(hp_output_port,b,hm_nout);
    if(!hp_owns_usb && (!hp_frozen_valid || !hp_frozen[H_RUN])){
        if(pressed&(1u<<BTN_FX)){hp_frozen_valid=0;hp_audio_enabled=1;hp_v[H_STATE]=HP_IDLE;hp_routes_view=0;}
        if(pressed&(1u<<BTN_SEL)){hp_frozen_valid=0;hp_audio_enabled=0;hp_v[H_STATE]=HP_IDLE;hp_routes_view=1;}
    }
    if(pressed&(1u<<BTN_ENV))hp_routes_view=!hp_routes_view;
    if((pressed&(1u<<BTN_GLO)) && hp_routes_view)hp_routes_view=0;
    else if((pressed&(1u<<BTN_GLO)) && hp_v[H_STATE]==HP_READY){hp_v[H_STATE]=HP_STOPPED;hp_v[H_REASON]=HP_INCOMPLETE;}
    if(hp_routes_view && hm_nin && hm_nout && !hp_running){
        uint32_t mask=hm_routes[hp_input];
        if(k)hm_route_set(hp_input,k>0?mask|(1u<<hp_output_port):mask&~(1u<<hp_output_port));
        if(pressed&(1u<<BTN_HOME))hm_route_set(hp_input,hm_all());
        if(pressed&(1u<<BTN_SAVE))hm_route_set(hp_input,0);
    }
    if(pressed&(1u<<BTN_PLAY)){
        if(hp_v[H_STATE]==HP_READY){fm1_irq_off();hp_run_begin();fm1_irq_on();}
        else if(hp_owns_usb && hp_running){fm1_irq_off();hp_request_stop(HP_STOPPED,HP_INCOMPLETE);fm1_irq_on();}
        else if((!hp_audio_enabled && (!hp_frozen_valid || !hp_frozen[H_RUN])) || hp_v[H_STATE]==HP_ARMED){
            if(!hp_audio_enabled){hp_frozen_valid=0;hp_v[H_RUN]=0;hp_v[H_RATE]=48000;hp_v[H_DURATION]=0;}
            hp_v[H_STATE]=HP_ENUM;hp_draw();hp_defer_run=1;hp_host_start();hp_routes_view=1;
        }
    }
}
static void app_main(void)
{
    uint32_t draw=0;palette_set(4);
    hp_v[H_MAGIC]=0x31504855u;hp_v[H_BUILD]=HP_BUILD_TAG;hp_v[H_STATE]=HP_IDLE;
    for(;;){
        uint32_t released,pressed;platform_poll();pressed=fm1_input_edges(&released);(void)released;
        hp_controls(pressed);
        if(pressed)draw=0;
        if(hp_owns_usb && !hp_running && hp_v[H_STATE]!=HP_ENUM && hp_v[H_STATE]!=HP_READY){hp_host_stop();hp_freeze();draw=0;}
        if(!draw || fm1_ms-draw>=250){hp_draw();draw=fm1_ms?fm1_ms:1;}
    }
}
