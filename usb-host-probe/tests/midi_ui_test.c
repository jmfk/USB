/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#define main probe_host_main
#include "../firmware/src/dinghy.c"
#undef main
int main(void)
{
    /* A stable boot page must not clear or retransmit the screen every 250 ms. */
    hp_v[H_STATE]=HP_IDLE;assert(hp_draw()>0);
    for(uint32_t i=0;i<8;i++){fm1_ms+=250;assert(hp_draw()==0);}
    hp_v[H_STATE]=HP_ARMED;hp_v[H_RATE]=44100;assert(hp_draw()>0);
    assert(hp_draw()==0);hp_v[H_RATE]=48000;assert(hp_draw()==1);assert(hp_draw()==0);
    hp_v[H_STATE]=HP_IDLE;assert(hp_draw()>0);assert(hp_draw()==0);
    hp_controls(1u<<BTN_SEL);assert(!hp_audio_enabled && hp_routes_view);
    hp_controls(1u<<BTN_PLAY);assert(hp_owns_usb && hp_v[H_STATE]==HP_READY && !hm_forward);
    assert(hm_nin==1 && hm_nout==1 && !hm_routes[0]);
    assert(hp_draw()>0);assert(hp_draw()==0);
    hp_controls(1u<<BTN_HOME);assert(hm_routes[0]==1);
    assert(hp_draw()==1);assert(hp_draw()==0);
    hp_controls(1u<<BTN_SAVE);assert(!hm_routes[0]);
    assert(hp_draw()==1);
    host_enc[ENC_KNOB1]=1;hp_controls(0);assert(hm_routes[0]==1);
    host_enc[ENC_KNOB1]=-1;hp_controls(0);assert(!hm_routes[0]);
    hp_controls(1u<<BTN_HOME);hp_controls(1u<<BTN_PLAY);assert(hm_forward);
    hp_controls(1u<<BTN_SAVE);assert(hm_routes[0]==1); /* live edits blocked */
    hp_controls(1u<<BTN_PLAY);assert(!hm_forward && !hp_running);
    hp_host_stop();hp_freeze();assert(hp_frozen_valid && !hp_frozen[H_RUN]);
    hp_controls(1u<<BTN_FX);assert(hp_audio_enabled && !hp_frozen_valid);
    hp_frozen[H_RUN]=123;hp_frozen_valid=1;
    hp_controls(1u<<BTN_SEL);assert(hp_audio_enabled && hp_frozen_valid); /* preserve real experiment */
    assert(hp_cycle(0,-1,32)==31 && hp_cycle(31,1,32)==0 && hp_cycle(0,1,0)==0);
    /* Shrinking/changing layouts must leave the same pixels as a fresh draw. */
    static uint16_t previous[240*240];
    hp_routes_view=1;assert(hp_draw()>0);hp_routes_view=0;assert(hp_draw()>0);
    memcpy(previous,fb,sizeof previous);hp_ui_valid=0;assert(hp_draw()>0);
    assert(!memcmp(previous,fb,sizeof previous));assert(hp_draw()==0);
    hp_v[H_STATE]=HP_RUNNING;assert(hp_draw()>0);assert(hp_draw()==0);
    hp_v[H_A_PACKETS]++;assert(hp_draw()==1);assert(hp_draw()==0);
    puts("PASS: UI refresh: static boot has no repeated LCD writes, changed values/routes update one box, layout changes clear stale pixels");
    puts("PASS: MIDI physical UI: discovery, route edits, ALL/NONE, start/stop, immutable audio evidence");
    return 0;
}
