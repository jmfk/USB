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
static void hp_tick(void) {if(hp_running){hm_sync_tick(fm1_ms*1000u,1);for(uint32_t i=0;i<hm_npipe;i++)if(hm_fill(&hm_outq[i]))hm_tx_ack(i,hm_fill(&hm_outq[i]));}}
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
static void hp_run_begin(void){hp_diag_begin(fm1_ms,HP_BUILD_TAG);hp_running=hm_forward=1;hm_sync_begin(fm1_ms*1000u);hp_v[H_STATE]=HP_RUNNING;}
static void hp_request_stop(uint32_t state,uint32_t reason){hp_diag_stop(fm1_ms,state,reason);hm_panic_begin();hm_sync_cleanup();do{hm_panic_pump();if(hm_panic_done)hm_sync_finish_cc();for(uint32_t i=0;i<hm_npipe;i++)if(hm_fill(&hm_outq[i]))hm_tx_ack(i,hm_fill(&hm_outq[i]));}while(!hm_panic_done||hm_sync_cc_stop_pending);hm_stopping=0;hp_running=hm_forward=0;hp_v[H_STATE]=state;hp_v[H_REASON]=reason;}
static void hp_host_stop(void){hp_owns_usb=0;}
#endif
#include "probe_wire.h"
static void app_audio(int32_t *out,uint32_t n) {(void)out;(void)n;}
static const char *const hp_reasons[]={"", "NEED 2 IN + 1 OUT", "REGISTER TIMEOUT", "FRAME DEADLINE", "USB TRANSFER ERROR", "NO USB FRAME CLOCK", "BUFFER DRIFT/NO AUDIO", "ARM ON COMPUTER", "DEVICE/FORMAT SETUP", "INCOMPLETE RUN", "MIDI ROUTING ERROR", "NO MIDI IN/OUT"};
/* Keep text, not a framebuffer. Unchanged pages do no LCD I/O; changed text
 * boxes replace their own background and glyphs together. Only a layout change
 * clears the panel, so the idle page never repeatedly blanks on real hardware. */
enum {HP_UI_BOXES=32,HP_UI_TEXT=40,HP_UI_TEXTBOX=0,HP_UI_GRAPH=1,HP_UI_LARGE=2,HP_UI_BEAT=3};
#define HP_ACC RGB(72,190,235)
#define HP_ON RGB(85,224,150)
#define HP_WARN RGB(245,184,75)
#define HP_ERR RGB(245,90,95)
#define HP_DIM RGB(145,155,170)
#define HP_ACTIVE RGB(21,65,86)
typedef struct {uint16_t x,y,w,h,color,bg;uint8_t align,kind;uint32_t a,b,c;char text[HP_UI_TEXT];} hp_ui_box_t;
static hp_ui_box_t hp_ui_frames[2][HP_UI_BOXES];
static uint8_t hp_ui_frame,hp_ui_count[2],hp_ui_valid;
static void hp_ui_invalidate(void){hp_ui_valid=0;}
static hp_ui_box_t *hp_shape(uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint32_t kind,uint32_t color,uint32_t bg)
{
 if(hp_ui_count[hp_ui_frame]>=HP_UI_BOXES)return 0;
 hp_ui_box_t *box=&hp_ui_frames[hp_ui_frame][hp_ui_count[hp_ui_frame]++];memset(box,0,sizeof *box);
 box->x=x;box->y=y;box->w=w;box->h=h;box->kind=kind;box->color=color;box->bg=bg;return box;
}
static void hp_color_box(uint32_t x,uint32_t y,uint32_t w,const char *text,uint32_t align,uint32_t color,uint32_t bg)
{
 hp_ui_box_t *box=hp_shape(x,y,w,FONT_S.h,HP_UI_TEXTBOX,color,bg);uint32_t i=0;if(!box)return;
 box->align=align;while(text[i]&&i+1<sizeof box->text){box->text[i]=text[i];i++;}
}
static void hp_box(uint32_t x,uint32_t y,uint32_t w,const char *text,uint32_t align){hp_color_box(x,y,w,text,align,C_WHITE,C_BLACK);}
static void hp_paint(const hp_ui_box_t *box)
{
 cv_begin(box->w,box->h,box->bg);
 if(box->kind==HP_UI_GRAPH){
  uint32_t rows=box->c<3?box->c:3;uint16_t trunk=box->a?HP_ON:HP_DIM;
  cv_line(0,52,20,52,trunk);if(rows)cv_line(20,16,20,16+44*(rows-1)>52?16+44*(rows-1):52,trunk);
  for(uint32_t i=0;i<rows;i++){
   uint32_t out=(box->b+i)%box->c,y=16+44*i;uint16_t col=(box->a&(1u<<out))?HP_ON:HP_DIM;
   if(box->a&(1u<<out))cv_line(20,y,45,y,col);else for(uint32_t x=22;x<44;x+=7)cv_line(x,y,x+3,y,col);
   cv_line(39,y-4,45,y,col);cv_line(39,y+4,45,y,col);
  }
 }else if(box->kind==HP_UI_BEAT){
  for(uint32_t i=0;i<4;i++){uint16_t col=box->b&&i==box->a?HP_WARN:RGB(45,49,58);cv_rect(10+54*i,3,40,6,col);}
 }else{
  const felucca_font_t *f=box->kind==HP_UI_LARGE?&FONT_L:&FONT_S;
  int32_t width=text_w(f,box->text),x=box->align==1?((int32_t)box->w-width)/2:box->align==2?(int32_t)box->w-width:0;
  cv_text(x,0,f,box->text,box->color);
 }
 cv_blit(box->x,box->y);lcd_sync();
}
static uint32_t hp_present(void)
{
 uint32_t old=hp_ui_frame^1u,n=hp_ui_count[hp_ui_frame],layout=!hp_ui_valid||n!=hp_ui_count[old],writes=0;
 for(uint32_t i=0;i<n&&!layout;i++){
  hp_ui_box_t *a=&hp_ui_frames[hp_ui_frame][i],*b=&hp_ui_frames[old][i];
  layout=a->x!=b->x||a->y!=b->y||a->w!=b->w||a->h!=b->h||a->kind!=b->kind;
 }
 if(layout){lcd_fill(0,0,240,240,C_BLACK);writes++;}
 for(uint32_t i=0;i<n;i++)if(layout||memcmp(&hp_ui_frames[hp_ui_frame][i],&hp_ui_frames[old][i],sizeof(hp_ui_box_t))){hp_paint(&hp_ui_frames[hp_ui_frame][i]);writes++;}
 hp_ui_valid=1;hp_ui_frame^=1u;return writes;
}
static void hp_text(uint32_t y,const char *text){hp_box(4,y,232,text,0);}
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
    char title[40],pages[16]="M1/1 S1/1";uint32_t sub=hp_selected_sub(),page=hp_selected_page();
    str_cpy(title,hp_main_name(hp_menu),sizeof title);uint32_t n=str_len(title);title[n++]=' ';title[n++]='/';title[n++]=' ';
    const char *name=hp_sub_name(sub);while(*name&&n<sizeof title-1)title[n++]=*name++;title[n]=0;
    hp_color_box(4,4,232,title,0,HP_ACC,C_BLACK);
    pages[1]=(char)('1'+hp_main_page[hp_menu]);pages[3]=(char)('0'+hp_main_count(hp_menu));pages[6]=(char)('1'+page);pages[8]=(char)('0'+hp_page_count());
    hp_color_box(4,28,112,pages,0,HP_DIM,C_BLACK);
    static const char *const status[]={"DEVICE","ARMED","SCAN","RUN","DONE","ERROR","STOPPED","READY"};
    hp_color_box(120,28,116,hp_diag_valid?"LOG READY":status[state],2,state==HP_FAILED?HP_ERR:state==HP_RUNNING?HP_ON:HP_WARN,C_BLACK);
    if(hp_menu==3){
        if(!sub&&!page){
            char tempo[12];uint32_t bpm=hm_sync_config.bpm,k=0;if(bpm>=100)tempo[k++]='0'+bpm/100;tempo[k++]='0'+(bpm/10)%10;tempo[k++]='0'+bpm%10;tempo[k++]=' ';tempo[k++]='B';tempo[k++]='P';tempo[k++]='M';tempo[k]=0;
            hp_ui_box_t *large=hp_shape(4,65,232,FONT_L.h,HP_UI_LARGE,HP_WARN,C_BLACK);if(large){str_cpy(large->text,tempo,sizeof large->text);large->align=1;}
            hp_color_box(4,110,232,hm_sync_config.enabled?(hm_sync_active?(hm_forward?"CLOCK RUNNING":"CLOCK STOPPING"):"CLOCK READY"):"CLOCK OFF",1,hm_sync_config.enabled?HP_ON:HP_DIM,C_BLACK);
            hp_ui_box_t *beat=hp_shape(4,139,232,12,HP_UI_BEAT,HP_WARN,C_BLACK);if(beat){beat->a=(hm_sync_clocks/24)%4;beat->b=hm_sync_active;}
            hp_color_box(4,165,232,"K1 BPM / K2 ON-OFF",0,HP_DIM,C_BLACK);
            hp_number(184,"SKIP",hm_sync_skipped);
        }else if(!sub){hp_number(62,"CLOCK TX",hm_sync_clocks);hp_number(90,"CLOCK SKIP",hm_sync_skipped);hp_number(118,"SERVICE LATE",hm_sync_late);hp_number(146,"CC BUSY",hm_sync_rejected);
        }else if(sub==1){
            if(!page){hp_number(65,"CC",hm_sync_config.cc);hp_number(96,"CHANNEL",hm_sync_config.channel+1);hp_text(140,"K1 CC / K2 CHANNEL");}
            else{hp_number(65,"ON VALUE",hm_sync_config.on);hp_number(96,"OFF VALUE",hm_sync_config.off);hp_text(140,"K1 ON / K2 OFF");}
            hp_color_box(4,176,232,hm_sync_cc_blocked&&hm_forward?"CC WAIT: SYSEX/QUEUE":hm_sync_cc_on?"REC: CC IS ON":"REC: CC IS OFF",0,hm_sync_cc_on?HP_ON:HP_DIM,C_BLACK);
        }else{
            hp_number(62,"OUTPUT",hm_sync_config.output+1);
            if(hm_sync_config.output<hm_nout){hp_port_heading(94,"OUT",hm_sync_config.output,&hm_outputs[hm_sync_config.output]);hp_port_line(120,&hm_outputs[hm_sync_config.output]);}
            else hp_color_box(4,94,232,"NO USB MIDI OUTPUT",0,HP_WARN,C_BLACK);
            hp_text(156,"ALGORITHM SELECTS OUT");hp_color_box(4,184,232,editable?"SET WHILE STOPPED":"OUTPUT LOCKED",0,HP_DIM,C_BLACK);
        }
    }else if(hp_link_view()){
        if(hm_nin){
            hp_input%=hm_nin;char label[16];
            label[0]='I';label[1]='N';label[2]=' ';label[3]='0'+(hp_input+1)/10;label[4]='0'+(hp_input+1)%10;label[5]=' ';label[6]='C';label[7]="0123456789ABCDEF"[hm_inputs[hp_input].cable];label[8]=0;
            hp_color_box(4,84,88,label,0,HP_ACC,C_BLACK);
            hm_pipe_t *in=&hm_pipes[hm_inputs[hp_input].pipe];const char *hex="0123456789ABCDEF";
            char id[10];for(uint32_t j=0;j<4;j++){id[j]=hex[(in->vid>>(12-4*j))&15];id[5+j]=hex[(in->pid>>(12-4*j))&15];}id[4]=':';id[9]=0;
            hp_color_box(4,106,88,id,0,HP_DIM,C_BLACK);
            uint32_t total=0;for(uint32_t j=0;j<hm_nout;j++)if(hm_routes[hp_input]&(1u<<j))total++;
            label[0]='T';label[1]='O';label[2]=' ';label[3]='0'+total/10;label[4]='0'+total%10;label[5]=0;hp_color_box(4,138,88,label,0,HP_ON,C_BLACK);
            if(hm_nout){
                hp_output_port%=hm_nout;hp_ui_box_t *graph=hp_shape(94,60,48,122,HP_UI_GRAPH,HP_ON,C_BLACK);
                if(graph){graph->a=hm_routes[hp_input];graph->b=hp_output_port;graph->c=hm_nout;}
                for(uint32_t j=0;j<hm_nout&&j<3;j++){
                    uint32_t out=(hp_output_port+j)%hm_nout,y=60+44*j,on=!!(hm_routes[hp_input]&(1u<<out));
                    label[0]='O';label[1]='U';label[2]='T';label[3]=' ';label[4]='0'+(out+1)/10;label[5]='0'+(out+1)%10;label[6]=' ';label[7]='C';label[8]=hex[hm_outputs[out].cable];label[9]=0;
                    hp_color_box(144,y,92,label,0,on?HP_ON:HP_DIM,j?C_BLACK:HP_ACTIVE);
                    hm_pipe_t *p=&hm_pipes[hm_outputs[out].pipe];for(uint32_t k=0;k<4;k++){id[k]=hex[(p->vid>>(12-4*k))&15];id[5+k]=hex[(p->pid>>(12-4*k))&15];}
                    hp_color_box(144,y+18,92,id,0,HP_DIM,C_BLACK);
                }
            }else hp_color_box(120,90,116,"NO OUTPUT",0,HP_WARN,C_BLACK);
            hp_color_box(4,184,232,editable?"K1 LINK / ALGO OUT":"STOP TO EDIT LINKS",0,HP_DIM,C_BLACK);
        }else{hp_color_box(4,76,232,"NO INPUTS",0,HP_WARN,C_BLACK);hp_text(104,"PLAY TO DISCOVER");}
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
        const char *label="";uint32_t active=0;uint16_t col=HP_DIM,bg=C_BLACK;
        if(i<4){label=hp_main_name(i);active=i==hp_menu;}
        if(i>=6&&i<9&&i-6<hp_sub_count()){
            static const char *const setup[]={"MODE","FMT","RES"},*const route[]={"LINK","PORT","ACT"},*const mon[]={"TRAF","ERR","PERF"},*const bus[]={"ST","VER"},*const sync[]={"CLK","CTRL","OUT"};
            label=hp_menu==3?sync[i-6]:hp_menu==0?setup[i-6]:hp_menu==2?bus[i-6]:hp_main_page[1]?mon[i-6]:route[i-6];active=i-6==sub;
        }
        if(i==10){label=hp_running?"STOP":(hp_v[H_STATE]==HP_READY||hp_v[H_STATE]==HP_ARMED||!hp_audio_enabled)&&!hp_diag_valid?"PLAY":"";col=hp_running?HP_WARN:HP_ON;}
        if(i==11&&hp_menu==1&&!hp_main_page[1]&&sub==2&&editable)label="SET";
        if(i==11&&hp_menu==3&&hp_running&&hm_forward&&hm_sync_active){label=hm_sync_cc_on?"OFF":"CC";col=HP_WARN;}
        if(active){col=C_WHITE;bg=HP_ACTIVE;}
        hp_color_box((i%6)*40+2,204+(i/6)*18,36,label,1,col,bg);
    }
    return hp_present();
}
static uint32_t hp_edit(uint32_t value,int32_t delta,uint32_t low,uint32_t high)
{
 if(delta>0&&(uint32_t)delta>high-value)return high;
 if(delta<0&&delta<(int32_t)low-(int32_t)value)return low;
 return value+delta;
}
static void hp_controls(uint32_t pressed)
{
    int32_t a=fm1_enc_take(ENC_SELECT),b=fm1_enc_take(ENC_ALGORITHM),k=fm1_enc_take(ENC_KNOB1),k2=fm1_enc_take(ENC_KNOB2);
    hp_input=hp_cycle(hp_input,a,hm_nin);hp_output_port=hp_cycle(hp_output_port,b,hm_nout);
    if(pressed&(1u<<BTN_FX))hp_main_press(0);
    if(pressed&(1u<<BTN_SEL))hp_main_press(1);
    if(pressed&(1u<<BTN_ENV))hp_main_press(2);
    if(pressed&(1u<<BTN_LFO))hp_main_press(3);
    if(pressed&(1u<<BTN_HOME))hp_sub_press(0);
    if(pressed&(1u<<BTN_SAVE))hp_sub_press(1);
    if(pressed&(1u<<BTN_ARP))hp_sub_press(2);
    uint32_t editable=!hp_running&&!hm_stopping&&(!hp_owns_usb||hp_v[H_STATE]==HP_READY);
    if(hp_menu==3){
        if(editable){
            if(b)hm_sync_config.output=hp_cycle(hm_sync_config.output,b,hm_nout);
            uint32_t sub=hp_selected_sub(),page=hp_selected_page();
            if(!sub&&!page){
                hm_sync_config.bpm=hp_edit(hm_sync_config.bpm,k,30,300);
                if(k2){hm_sync_config.enabled=k2>0;if(k2>0&&!hp_owns_usb&&hp_v[H_STATE]!=HP_ARMED&&(!hp_frozen_valid||!hp_frozen[H_RUN])){hp_audio_enabled=0;hp_frozen_valid=0;hp_v[H_STATE]=HP_IDLE;}}
            }else if(sub==1){
                if(!page){hm_sync_config.cc=hp_edit(hm_sync_config.cc,k,0,119);hm_sync_config.channel=hp_edit(hm_sync_config.channel,k2,0,15);}
                else{hm_sync_config.on=hp_edit(hm_sync_config.on,k,0,127);hm_sync_config.off=hp_edit(hm_sync_config.off,k2,0,127);}
            }
        }
        if((pressed&(1u<<BTN_REC))&&hp_running){fm1_irq_off();hm_sync_cc();fm1_irq_on();}
    }
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
        uint32_t released,pressed;platform_poll();
#ifdef DINGHY_HOST
        hp_tick(); /* Display-fixture clock only; USB ACK timing is modeled separately. */
#endif
        if(hp_finish_if_stopped())draw=0;
        pressed=fm1_input_edges(&released);(void)released;
        hp_controls(pressed);
        if(pressed)draw=0;
        if(hp_finish_if_stopped())draw=0;
        if(!draw || fm1_ms-draw>=250){hp_draw();draw=fm1_ms?fm1_ms:1;}
    }
}
