/* SPDX-License-Identifier: GPL-3.0-only */
/* Local adaptation of DUB R64/ADR-0122 and KEEL menu contracts.
 * Navigation owns no routing, transport or device-mode data. */
#pragma once
static uint8_t hp_menu,hp_main_page[3],hp_sub[3][2],hp_sub_page[3][2][3];
static uint8_t hp_action_all;
static uint32_t hp_main_count(uint32_t m){return m==1?2:1;}
static uint32_t hp_sub_count(void){return hp_menu==2?2:3;}
static uint32_t hp_page_count(void){return (hp_menu==0&&hp_sub[0][0]==2)||(hp_menu==1&&!hp_main_page[1]&&hp_sub[1][0]==1)?2:1;}
static uint32_t hp_selected_sub(void){return hp_sub[hp_menu][hp_main_page[hp_menu]];}
static uint32_t hp_selected_page(void){return hp_sub_page[hp_menu][hp_main_page[hp_menu]][hp_selected_sub()];}
static void hp_main_press(uint32_t m){if(hp_menu==m)hp_main_page[m]=(hp_main_page[m]+1)%hp_main_count(m);hp_menu=m;}
static void hp_sub_press(uint32_t s){if(s>=hp_sub_count())return;if(hp_selected_sub()==s)hp_sub_page[hp_menu][hp_main_page[hp_menu]][s]=(hp_selected_page()+1)%hp_page_count();else hp_sub[hp_menu][hp_main_page[hp_menu]]=s;}
static const char *hp_main_name(uint32_t m){return m==0?"USB":m==1?(hp_main_page[1]?"MON":"MIDI"):"BUS";}
static const char *hp_sub_name(uint32_t s){static const char *const setup[]={"MODE","FORMAT","RESULT"},*const route[]={"LINK","PORTS","ACTION"},*const mon[]={"TRAFFIC","FAULTS","PERF"},*const bus[]={"STATE","BUILD"};return hp_menu==0?setup[s]:hp_menu==2?bus[s]:hp_main_page[1]?mon[s]:route[s];}
static uint32_t hp_link_view(void){return hp_menu==1&&!hp_main_page[1]&&hp_selected_sub()==0;}
