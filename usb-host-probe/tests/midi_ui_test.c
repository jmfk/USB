/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#define main probe_host_main
#include "../firmware/src/dinghy.c"
#undef main
static void knob(int v){host_enc[ENC_KNOB1]=v;hp_controls(0);}
int main(void)
{
 hp_v[H_STATE]=HP_IDLE;hp_diag_valid=1;hp_controls(1u<<BTN_PLAY);assert(!hp_owns_usb&&!hp_running);hp_diag_valid=0;assert(hp_draw()>0);for(int i=0;i<8;i++)assert(hp_draw()==0);
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
 assert(hp_draw()>0);assert(hp_draw()==0);knob(1);assert(hm_routes[0]==1);assert(hp_draw()==1);
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
 puts("PASS: independent main/sub pages, remembered selection, inert navigation/modifiers, explicit actions, stopped edits, transport, evidence protection and stable redraw");return 0;
}
