/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "profiler.h"
int main(void)
{
 assert(hp_prof_percent(0,0)==0 && hp_prof_percent(1,2)==50 && hp_prof_percent(200,100)==100);
 assert(hp_prof_percent(5000000000ull,10000000000ull)==50);
 hp_prof_begin(0xfffffff0u);hp_prof_period(0xfffffff0u,24);hp_prof_period(0x950u,24);
 assert(hp_prof.wall==2400 && hp_prof.gap_max==2400 && !hp_prof.gap_late);
 hp_prof_period(0x950u+4800u,24);assert(hp_prof.gap_late==1);
 for(uint32_t i=0;i<1000;i++)hp_prof_add(HP_P_TIMER,10000000);
 assert(hp_prof.lane[HP_P_TIMER].ticks==10000000000ull && hp_prof.lane[HP_P_TIMER].max==10000000);
 hp_prof_frame(18000,24);assert(hp_prof.frame_min==6000 && hp_prof.frames==1 && !hp_prof.frame_late);
 hp_prof_frame(26000,24);assert(!hp_prof.frame_min && hp_prof.frame_late==1);
 hp_prof_stop(0x950u+7200u);assert(hp_prof.wall==9600);
 hp_prof_add(HP_P_TIMER,999);hp_prof_period(999,24);assert(hp_prof.wall==9600 && hp_prof.lane[HP_P_TIMER].calls==1000);
 puts("PASS: profiler tick wrap, 64-bit totals, timer gaps, frame margin, stop boundary");
}
