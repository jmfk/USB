/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#define main probe_host_main
#include "../firmware/src/dinghy.c"
#undef main
int main(void)
{
    hp_controls(1u<<BTN_SEL);assert(!hp_audio_enabled && hp_routes_view);
    hp_controls(1u<<BTN_PLAY);assert(hp_owns_usb && hp_v[H_STATE]==HP_READY && !hm_forward);
    assert(hm_nin==1 && hm_nout==1 && !hm_routes[0]);
    hp_controls(1u<<BTN_HOME);assert(hm_routes[0]==1);
    hp_controls(1u<<BTN_SAVE);assert(!hm_routes[0]);
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
    puts("PASS: MIDI physical UI: discovery, route edits, ALL/NONE, start/stop, immutable audio evidence");
    return 0;
}
