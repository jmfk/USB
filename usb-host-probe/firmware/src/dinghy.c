/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
static volatile uint8_t hp_owns_usb;
static void hp_tick(void);
static void hp_ui_invalidate(void);
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
static void hp_run_begin(void){hp_diag_begin(fm1_ms,HP_BUILD_TAG);hp_running=hm_forward=1;hp_v[H_STATE]=HP_RUNNING;}
static void hp_request_stop(uint32_t state,uint32_t reason){hp_diag_stop(fm1_ms,state,reason);hp_running=hm_forward=0;hp_v[H_STATE]=state;hp_v[H_REASON]=reason;}
static void hp_host_stop(void){hp_owns_usb=0;}
#endif
#include "probe_wire.h"
static void app_audio(int32_t *out,uint32_t n) {(void)out;(void)n;}
static const char *const hp_states[]={"DEVICE / NOT ARMED","ARMED / PRESS PLAY","DISCOVERING USB","RUNNING","BUS TEST COMPLETE","TEST FAILED","STOPPED","READY / SET ROUTES"};
static const char *const hp_reasons[]={"", "NEED 2 IN + 1 OUT", "REGISTER TIMEOUT", "FRAME DEADLINE", "USB TRANSFER ERROR", "NO USB FRAME CLOCK", "BUFFER DRIFT/NO AUDIO", "ARM ON COMPUTER", "DEVICE/FORMAT SETUP", "INCOMPLETE RUN", "MIDI ROUTING ERROR", "NO MIDI IN/OUT"};
/* Keep text, not a framebuffer. Unchanged pages do no LCD I/O; changed text
 * boxes replace their own background and glyphs together. Only a layout change
 * clears the panel, so the idle page never repeatedly blanks on real hardware. */
enum {HP_UI_BOXES=24,HP_UI_TEXT=40};
typedef struct {uint16_t x,y,w;uint8_t align;char text[HP_UI_TEXT];} hp_ui_box_t;
static hp_ui_box_t hp_ui_frames[2][HP_UI_BOXES];
static uint8_t hp_ui_frame,hp_ui_count[2],hp_ui_valid;
static void hp_ui_invalidate(void){hp_ui_valid=0;}
static void hp_box(uint32_t x,uint32_t y,uint32_t w,const char *text,uint32_t align)
{
    uint32_t i=0;hp_ui_box_t *box;
    if(hp_ui_count[hp_ui_frame]>=HP_UI_BOXES)return;
    box=&hp_ui_frames[hp_ui_frame][hp_ui_count[hp_ui_frame]++];
    box->x=x;box->y=y;box->w=w;box->align=align;
    memset(box->text,0,sizeof box->text);
    while(text[i] && i+1<sizeof box->text){box->text[i]=text[i];i++;}
}
static uint32_t hp_present(void)
{
    uint32_t old=hp_ui_frame^1u,n=hp_ui_count[hp_ui_frame],layout=!hp_ui_valid || n!=hp_ui_count[old],writes=0;
    for(uint32_t i=0;i<n && !layout;i++){
        hp_ui_box_t *a=&hp_ui_frames[hp_ui_frame][i],*b=&hp_ui_frames[old][i];
        layout=a->x!=b->x || a->y!=b->y || a->w!=b->w || a->align!=b->align;
    }
    if(layout){lcd_fill(0,0,240,240,C_BLACK);writes++;}
    for(uint32_t i=0;i<n;i++){
        hp_ui_box_t *box=&hp_ui_frames[hp_ui_frame][i];
        if(layout || memcmp(box->text,hp_ui_frames[old][i].text,HP_UI_TEXT)){
            draw_text_box(box->x,box->y,box->w,&FONT_S,box->text,C_WHITE,box->align);writes++;
        }
    }
    hp_ui_valid=1;hp_ui_frame^=1u;return writes;
}
static void hp_text(uint32_t y,const char *text) {hp_box(4,y,232,text,0);}
static void hp_number(uint32_t y,const char *name,uint32_t value)
{
    char line[32],digits[12];uint32_t n=0,j=0;
    while(*name && n<18)line[n++]=*name++;
    line[n++]=' ';
    do {digits[j++]=(char)('0'+value%10);value/=10;}while(value);
    while(j)line[n++]=digits[--j];
    line[n]=0;hp_text(y,line);
}
#ifdef DINGHY_HOST
#define str_len strlen
#define str_cpy(d,s,n) snprintf(d,n,"%s",s)
#endif
#include "ui_navigation.h"
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
static uint32_t hp_draw(void)
{
    uint32_t state=hp_v[H_STATE],editable=!hp_running && !hm_stopping && (!hp_owns_usb || state==HP_READY);
    if(state>HP_READY)state=HP_FAILED;
    hp_ui_count[hp_ui_frame]=0;
    char title[40];uint32_t sub=hp_selected_sub(),page=hp_selected_page();
    str_cpy(title,hp_main_name(hp_menu),sizeof title);
    uint32_t n=str_len(title);title[n++]=' ';title[n++]='/';title[n++]=' ';
    const char *name=hp_sub_name(sub);while(*name&&n<24)title[n++]=*name++;
    title[n++]=' ';title[n++]='M';title[n++]=(char)('1'+hp_main_page[hp_menu]);title[n++]='/';title[n++]=(char)('0'+hp_main_count(hp_menu));
    title[n++]=' ';title[n++]='S';title[n++]=(char)('1'+page);title[n++]='/';title[n++]=(char)('0'+hp_page_count());title[n]=0;
    hp_text(4,title);hp_text(30,hp_diag_valid?"LOG READY / FETCH MIDI":hp_states[state]);
    if(hp_link_view()){
        if(hm_nin){
            hp_input%=hm_nin;hp_port_heading(56,"IN",hp_input,&hm_inputs[hp_input]);
            if(hm_nout){hp_output_port%=hm_nout;hp_port_heading(82,"OUT",hp_output_port,&hm_outputs[hp_output_port]);hp_text(110,(hm_routes[hp_input]&(1u<<hp_output_port))?"ROUTE ON":"ROUTE OFF");}
            hp_text(150,editable?"K1 RIGHT ON / LEFT OFF":"STOP TO EDIT ROUTES");
            hp_text(174,"SELECT IN / ALGO OUT");
        }else hp_text(70,"PLAY TO DISCOVER MIDI");
    }else if(hp_menu==1&&!hp_main_page[1]){
        if(sub==1){
            hp_text(60,page?"OUTPUT PORT":"INPUT PORT");
            if(page?hm_nout:hm_nin){if(page){hp_output_port%=hm_nout;hp_port_heading(90,"OUT",hp_output_port,&hm_outputs[hp_output_port]);hp_port_line(120,&hm_outputs[hp_output_port]);}else{hp_input%=hm_nin;hp_port_heading(90,"IN",hp_input,&hm_inputs[hp_input]);hp_port_line(120,&hm_inputs[hp_input]);}}
            else hp_text(90,"NO PORTS DISCOVERED");
        }else{hp_text(65,hp_action_all?"ALL OUTPUTS":"NO OUTPUTS");hp_text(95,"K1 LEFT NONE / RIGHT ALL");hp_text(130,editable?"REC: APPLY TO INPUT":"STOP TO EDIT ROUTES");}
    }else if(hp_menu==1){
        if(!sub){hp_number(62,"MIDI RX",hm_rx_packets);hp_number(88,"MIDI TX",hm_tx_packets);}
        else if(sub==1){hp_number(62,"FAULT",hm_fault);hp_text(100,hp_reasons[hp_v[H_REASON]<=HP_NO_MIDI?hp_v[H_REASON]:HP_IO]);}
        else{hp_number(62,"SERVICE MAX US",hp_diag[19]);hp_number(90,"IN QUEUE MAX",hp_diag[17]);hp_number(118,"OUT QUEUE MAX",hp_diag[18]);hp_number(146,"LOG DROPPED",hp_diag[23]);}
    }else if(hp_menu==0){
        if(!sub){hp_text(62,hp_audio_enabled?"MODE AUDIO + MIDI":"MODE MIDI ONLY");hp_text(95,"K1 RIGHT AUDIO / LEFT MIDI");hp_text(130,"POWERED HUB REQUIRED");}
        else if(sub==1){hp_text(62,"UAC1 PCM16 STEREO");hp_number(90,"RATE",hp_v[H_RATE]);hp_text(122,"44100 / 48000 HZ");}
        else if(!page){hp_number(62,"AUDIO IN A",hp_v[H_A_PACKETS]);hp_number(88,"AUDIO IN B",hp_v[H_B_PACKETS]);hp_number(114,"OUTPUT",hp_v[H_OUT_PACKETS]);}
        else{hp_number(62,"IO ERRORS",hp_v[H_IO_ERRORS]);hp_text(100,hp_reasons[hp_v[H_REASON]<=HP_NO_MIDI?hp_v[H_REASON]:HP_IO]);}
    }else if(!sub){hp_text(62,hp_owns_usb?"USB0 HOST":"USB0 DEVICE");hp_number(90,"MIDI INPUTS",hm_nin);hp_number(116,"MIDI OUTPUTS",hm_nout);}
    else{hp_text(62,hp_diag_valid?"FETCH MIDI LOG FIRST":"USB " HP_LABEL);hp_number(90,"BUILD",HP_BUILD_TAG);hp_text(125,"HARDWARE UNVERIFIED");}
    for(uint32_t i=0;i<12;i++){
        const char *label="";char selected[12];uint32_t active=0;
        if(i<3){label=hp_main_name(i);active=i==hp_menu;}
        if(i==5){label="SHIFT";active=!!(fm1_in.buttons&(1u<<BTN_GLO));}
        if(i>=6&&i<9&&i-6<hp_sub_count()){static const char *const setup[]={"MODE","FMT","RES"},*const route[]={"LINK","PORT","ACT"},*const mon[]={"TRAF","ERR","PERF"},*const bus[]={"STATE","BUILD"};label=hp_menu==0?setup[i-6]:hp_menu==2?bus[i-6]:hp_main_page[1]?mon[i-6]:route[i-6];active=i-6==sub;}
        if(i==9){label="ALT";active=!!(fm1_in.buttons&(1u<<BTN_SEQ));}
        if(i==10)label=hp_running?"STOP":"PLAY";
        if(i==11&&hp_menu==1&&!hp_main_page[1]&&sub==2&&editable)label="APPLY";
        if(active){selected[0]='>';str_cpy(selected+1,label,sizeof selected-1);label=selected;}
        hp_box((i%6)*40,204+(i/6)*18,40,label,1);
    }
    return hp_present();
}
static void hp_controls(uint32_t pressed)
{
    int32_t a=fm1_enc_take(ENC_SELECT),b=fm1_enc_take(ENC_ALGORITHM),k=fm1_enc_take(ENC_KNOB1);
    hp_input=hp_cycle(hp_input,a,hm_nin);hp_output_port=hp_cycle(hp_output_port,b,hm_nout);
    if(pressed&(1u<<BTN_FX))hp_main_press(0);
    if(pressed&(1u<<BTN_SEL))hp_main_press(1);
    if(pressed&(1u<<BTN_ENV))hp_main_press(2);
    if(pressed&(1u<<BTN_HOME))hp_sub_press(0);
    if(pressed&(1u<<BTN_SAVE))hp_sub_press(1);
    if(pressed&(1u<<BTN_ARP))hp_sub_press(2);
    uint32_t editable=!hp_running&&!hm_stopping&&(!hp_owns_usb||hp_v[H_STATE]==HP_READY);
    if(hp_menu==0&&!hp_selected_sub()&&k&&hp_audio_enabled!=(k>0)&&!hp_owns_usb&&editable&&(!hp_frozen_valid||!hp_frozen[H_RUN])){
        hp_frozen_valid=0;hp_audio_enabled=k>0;hp_v[H_STATE]=HP_IDLE;
    }
    if(hp_link_view()&&hm_nin&&hm_nout&&editable&&k){uint32_t mask=hm_routes[hp_input];hm_route_set(hp_input,k>0?mask|(1u<<hp_output_port):mask&~(1u<<hp_output_port));}
    if(hp_menu==1&&!hp_main_page[1]&&hp_selected_sub()==2){
        if(k)hp_action_all=k>0;
        if((pressed&(1u<<BTN_REC))&&editable&&hm_nin&&hm_nout)hm_route_set(hp_input,hp_action_all?hm_all():0);
    }
    if((pressed&(1u<<BTN_PLAY))&&hp_diag_valid&&!hp_owns_usb){return;}
    if(pressed&(1u<<BTN_PLAY)){
        if(hp_v[H_STATE]==HP_READY){fm1_irq_off();hp_run_begin();fm1_irq_on();}
        else if(hp_owns_usb && hp_running){fm1_irq_off();hp_request_stop(HP_STOPPED,HP_INCOMPLETE);fm1_irq_on();}
        else if(!hp_owns_usb && ((!hp_audio_enabled && (!hp_frozen_valid || !hp_frozen[H_RUN])) || hp_v[H_STATE]==HP_ARMED)){
            if(!hp_audio_enabled){hp_frozen_valid=0;hp_v[H_RUN]=0;hp_v[H_RATE]=48000;hp_v[H_DURATION]=0;}
            hp_v[H_STATE]=HP_ENUM;hp_draw();hp_defer_run=1;hp_host_start();
        }
    }
}
/* Release the stopped host and freeze evidence before accepting another action.
 * Controls also gate the IRQ completion window after this check. */
static uint32_t hp_finish_if_stopped(void)
{
 if(hp_owns_usb && !hp_running && hp_v[H_STATE]!=HP_ENUM && hp_v[H_STATE]!=HP_READY){
  hp_host_stop();hp_freeze();hp_ui_invalidate();return 1;
 }
 return 0;
}
static void app_main(void)
{
    uint32_t draw=0;palette_set(4);
    hp_v[H_MAGIC]=0x31504855u;hp_v[H_BUILD]=HP_BUILD_TAG;hp_v[H_STATE]=HP_IDLE;
    for(;;){
        uint32_t released,pressed;platform_poll();if(hp_finish_if_stopped())draw=0;pressed=fm1_input_edges(&released);(void)released;
        hp_controls(pressed);
        if(pressed)draw=0;
        if(hp_finish_if_stopped())draw=0;
        if(!draw || fm1_ms-draw>=250){hp_draw();draw=fm1_ms?fm1_ms:1;}
    }
}
