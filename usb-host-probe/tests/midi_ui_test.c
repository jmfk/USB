/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#define main probe_host_main
#include "../firmware/src/dinghy.c"
#undef main
static void knob(int v){host_enc[ENC_KNOB1]=v;hp_controls(0);}
int main(void)
{
 hp_v[H_STATE]=HP_IDLE;hp_diag_valid=1;hp_controls(1u<<BTN_PLAY);assert(!hp_owns_usb&&!hp_running);hp_diag_valid=0;assert(hp_draw()>0);for(int i=0;i<8;i++)assert(hp_draw()==0);
 /* Choosing the current mode must preserve an armed audio attempt. */
 hp_v[H_STATE]=HP_ARMED;hp_v[H_RUN]=99;knob(1);assert(hp_v[H_STATE]==HP_ARMED&&hp_v[H_RUN]==99);
 hp_v[H_STATE]=HP_IDLE;hp_v[H_RUN]=0;
 /* External updater/error screens invalidate the app's text cache on return. */
 hp_draw();static uint16_t app_pixels[240*240];memcpy(app_pixels,fb,sizeof app_pixels);
 lcd_fill(0,0,240,240,C_WHITE);hp_ui_invalidate();assert(hp_draw()>0);assert(!memcmp(app_pixels,fb,sizeof app_pixels));assert(hp_draw()==0);
 /* Navigation alone never changes operating mode or starts USB. */
 hp_controls(1u<<BTN_SEL);assert(hp_menu==1&&hp_audio_enabled&&!hp_owns_usb);
 hp_controls(1u<<BTN_SAVE);assert(hp_selected_sub()==1&&hp_selected_page()==0);
 hp_controls(1u<<BTN_SAVE);assert(hp_selected_page()==1);
 hp_controls(1u<<BTN_SEL);assert(hp_main_page[1]==1&&hp_selected_sub()==0);
 hp_controls(1u<<BTN_SAVE);assert(hp_selected_sub()==1);
 hp_controls(1u<<BTN_SEL);assert(hp_main_page[1]==0&&hp_selected_sub()==1&&hp_selected_page()==1);
 hp_controls(1u<<BTN_FX);knob(-1);assert(!hp_audio_enabled);
 hp_controls(1u<<BTN_SEL);assert(hp_selected_page()==1);
 hp_controls(1u<<BTN_HOME);assert(hp_link_view());
 hp_controls(1u<<BTN_PLAY);assert(hp_owns_usb&&hp_v[H_STATE]==HP_READY&&!hm_forward);
 assert(hp_draw()>0);assert(hp_draw()==0);knob(1);assert(hm_routes[0]==1);assert(hp_draw()>0);
 /* Submenu selection is never ALL/NONE. REC is the explicit action. */
 hp_controls(1u<<BTN_ARP);assert(hm_routes[0]==1);knob(-1);assert(hm_routes[0]==1);
 hp_controls(1u<<BTN_REC);assert(!hm_routes[0]);knob(1);hp_controls(1u<<BTN_REC);assert(hm_routes[0]==1);
 hp_controls(1u<<BTN_PLAY);assert(hm_forward);
 knob(-1);hp_controls(1u<<BTN_REC);assert(hm_routes[0]==1);
 hp_controls(1u<<BTN_HOME);knob(-1);assert(hm_routes[0]==1);
 uint32_t menu=hp_menu,main=hp_main_page[1],sub=hp_selected_sub(),page=hp_selected_page();
 hp_controls((1u<<BTN_GLO)|(1u<<BTN_SEQ));assert(menu==hp_menu&&main==hp_main_page[1]&&sub==hp_selected_sub()&&page==hp_selected_page()&&hm_forward);
 hp_controls(1u<<BTN_PLAY);assert(!hm_forward&&!hp_running);
 hp_host_stop();hp_freeze();assert(!hp_diag_valid);hp_frozen[H_RUN]=123;hp_frozen_valid=1;
 hp_controls(1u<<BTN_FX);knob(1);assert(!hp_audio_enabled&&hp_frozen_valid);
 hp_controls(1u<<BTN_SEL);assert(hp_link_view());assert(hp_draw()>0);
 static uint16_t previous[240*240];hp_controls(1u<<BTN_ENV);hp_draw();memcpy(previous,fb,sizeof previous);hp_ui_valid=0;hp_draw();assert(!memcmp(previous,fb,sizeof previous));assert(hp_draw()==0);
 assert(hp_cycle(0,-1,32)==31&&hp_cycle(31,1,32)==0);
 /* IRQ completion after finalization check must not permit edits/restart. */
 hp_audio_enabled=0;hp_menu=1;hp_main_page[1]=0;hp_sub[1][0]=0;
 hp_frozen_valid=0;hp_owns_usb=1;hp_running=0;hm_stopping=0;hm_forward=0;hp_v[H_STATE]=HP_STOPPED;hp_v[H_RUN]=0;
 hp_diag_nonce[0]=55;hp_diag_begin(fm1_ms,HP_BUILD_TAG);hp_diag_stop(fm1_ms,HP_STOPPED,HP_INCOMPLETE);
 hm_routes[0]=1;knob(-1);hp_controls(1u<<BTN_PLAY);
 assert(hp_owns_usb&&hp_v[H_STATE]==HP_STOPPED&&hm_routes[0]==1&&!hp_diag_valid);
 assert(hp_finish_if_stopped());assert(!hp_owns_usb&&hp_diag_valid&&hp_diag_frozen[25]==55);
 assert(hp_frozen[H_STATE]==HP_STOPPED);hp_controls(1u<<BTN_PLAY);assert(!hp_owns_usb&&hp_diag_valid);
 /* Real footer glyph widths must fit, including held unused modifiers. */
 for(uint32_t menu=0;menu<4;menu++){
  hp_menu=menu;fm1_in.buttons=(1u<<BTN_GLO)|(1u<<BTN_SEQ);hp_draw();
  hp_ui_box_t *boxes=hp_ui_frames[hp_ui_frame^1];
  for(uint32_t j=0;j<hp_ui_count[hp_ui_frame^1];j++)if(boxes[j].y>=204){
   assert(text_w(&FONT_S,boxes[j].text)<=(int32_t)boxes[j].w-4);
   if(boxes[j].x==202&&boxes[j].y==204)assert(!boxes[j].text[0]);
   if(boxes[j].x==122&&boxes[j].y==222)assert(!boxes[j].text[0]);
  }
 }
 assert(hp_edit(120,0x7fffffff,30,300)==300);assert(hp_edit(120,(-2147483647-1),30,300)==30);
 hp_diag_valid=hp_frozen_valid=0;hp_v[H_STATE]=HP_IDLE;hp_audio_enabled=1;hp_menu=3;hp_sub[3][0]=0;hp_sub_page[3][0][0]=0;
 host_enc[ENC_KNOB2]=1;hp_controls(0);assert(hm_sync_config.enabled&&!hp_audio_enabled&&!hp_running);
 hp_controls(1u<<BTN_PLAY);assert(hp_v[H_STATE]==HP_READY&&!hm_sync_active);
 hp_controls(1u<<BTN_PLAY);assert(hp_running&&hm_sync_active&&((hm_word(&hm_outq[hm_outputs[hm_sync_config.output].pipe])>>8)&255)==0xfa);
 uint32_t tempo=hm_sync_config.bpm;knob(20);assert(hm_sync_config.bpm==tempo);
 hp_controls(1u<<BTN_REC);assert(hm_sync_cc_on);
 hp_controls(1u<<BTN_PLAY);assert(!hp_running&&!hm_sync_active&&!hm_sync_cc_on);
 puts("PASS: graphic footer fit, inactive modifiers, sync enable/start/REC/stop and locked configuration");
 puts("PASS: review regressions: completion/freeze action gates, unchanged armed mode and external-screen restoration");
 puts("PASS: independent main/sub pages, remembered selection, inert navigation/modifiers, explicit actions, stopped edits, transport, evidence protection and stable redraw");return 0;
}
