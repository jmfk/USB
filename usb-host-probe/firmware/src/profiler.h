/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
/* Inclusive HOST/MIX/MIDI lanes overlap TIMER. AUDIO/UI subtract nested IRQs.
 * Totals use ticks, not rounded microseconds; no hot-path 64-bit division. */
enum { HP_P_TIMER,HP_P_AUDIO,HP_P_UI,HP_P_HOST,HP_P_MIX,HP_P_MIDI,HP_P_LANES };
typedef struct {uint64_t ticks;uint32_t calls,max;} hp_prof_lane_t;
static volatile struct {
 hp_prof_lane_t lane[HP_P_LANES];
 uint64_t wall;
 uint32_t last,period_last,gap_max,gap_late,frame_min,frames,frame_late;
 uint8_t active,period_valid;
} hp_prof;
#define HP_PROFILE_WORDS 64u
static void hp_prof_begin(uint32_t now)
{
 for(uint32_t i=0;i<sizeof hp_prof;i++)((volatile uint8_t *)&hp_prof)[i]=0;
 hp_prof.last=now;hp_prof.frame_min=0xffffffffu;hp_prof.active=1;
}
static void hp_prof_wall(uint32_t now)
{
 if(hp_prof.active){hp_prof.wall+=(uint32_t)(now-hp_prof.last);hp_prof.last=now;}
}
static void hp_prof_stop(uint32_t now){hp_prof_wall(now);hp_prof.active=0;}
static void hp_prof_add(uint32_t lane,uint32_t ticks)
{
 if(!hp_prof.active)return;
 volatile hp_prof_lane_t *p=&hp_prof.lane[lane];p->ticks+=ticks;p->calls++;if(ticks>p->max)p->max=ticks;
}
static void hp_prof_period(uint32_t now,uint32_t ticks_per_us)
{
 if(!hp_prof.active)return;
 hp_prof_wall(now);
 if(hp_prof.period_valid){uint32_t gap=now-hp_prof.period_last;if(gap>hp_prof.gap_max)hp_prof.gap_max=gap;if(gap>150u*ticks_per_us)hp_prof.gap_late++;}
 hp_prof.period_last=now;hp_prof.period_valid=1;
}
static void hp_prof_frame(uint32_t elapsed,uint32_t ticks_per_us)
{
 if(!hp_prof.active)return;
 hp_prof.frames++;
 uint32_t budget=1000u*ticks_per_us,margin=elapsed<budget?budget-elapsed:0;
 if(margin<hp_prof.frame_min)hp_prof.frame_min=margin;
 if(elapsed>=budget)hp_prof.frame_late++;
}

/* Approximate cumulative measured-work percent; raw counters keep full precision. */
static uint32_t hp_prof_percent(uint64_t busy,uint64_t wall)
{
 if(!wall)return 0;
 if(busy>=wall)return 100;
 while(wall>0x00ffffffu){wall>>=1;busy>>=1;}
 return ((uint32_t)busy*100u+(uint32_t)wall/2u)/(uint32_t)wall;
}
