/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
/* USB0 EP1 is time-shared, not multipoint. Cancel REQPKT before flushing RX;
 * retain DATA toggles and count TX delivery only on ACK/toggle advancement.
 * MUSB reference: Linux musb_host.c bulk cancellation and FIFO handling.
 * The extra 100us quiescence guard exceeds a Full-Speed 64-byte transaction.
 * These USB0 register assumptions still require hardware acceptance. */
static uint8_t hm_rx_dma[96] __attribute__((aligned(32)));
static uint8_t hm_tx_dma[64] __attribute__((aligned(32)));
static struct {
    uint8_t state,pipe,in,old_toggle,tx_events,round,unsafe;
    uint32_t started,settle;
} hm_bus;
static uint32_t hm_blocked_ms[HM_PIPES];

static int hm_usb_config(void)
{
    uint8_t configured[64]={0};uint32_t i;
    if(hp_audio_enabled)for(i=0;i<3;i++)configured[hp_stream[i].addr]=1;
    for(i=0;i<hm_npipe;i++){
        hm_pipe_t *p=&hm_pipes[i];
        if(!configured[p->addr]){
            uint8_t mps=8;
            for(uint32_t j=0;j<hp_count;j++)if(hp_devices[j].addr==p->addr)mps=hp_devices[j].mps0;
            if(hp_ctl(p->addr,0,9,p->cfg,0,0,0,mps)<0)return -1;
            configured[p->addr]=1;
        }
    }
    memset(&hm_bus,0,sizeof hm_bus);memset(hm_blocked_ms,0,sizeof hm_blocked_ms);
    return 0;
}
static uint32_t hm_usb_toggle(void)
{
    return hm_bus.in?(hp_r(S_RXCSR2)>>1)&1:hp_r(S_TXCSR2)&1;
}
static void hm_usb_start(uint32_t pipe)
{
    hm_pipe_t *p=&hm_pipes[pipe];uint32_t len;
    hm_bus.pipe=(uint8_t)pipe;hm_bus.in=(p->ep&128)!=0;hm_bus.old_toggle=p->toggle;
    hm_bus.started=fm1_ticks();hm_bus.state=1;
    hp_w(S_FADDR,p->addr);hp_w(S_INDEX,1);
    if(hm_bus.in){
        hp_hal_rxbuf(1,hm_rx_dma);hp_w(S_RXMAXP,255);
        hp_w(S_RXCSR1,0x11);hp_w(S_RXCSR1,0x11);
        hp_w(S_RXCSR2,4u|((uint32_t)p->toggle<<1));
        hp_w(26,0x20|(p->ep&15));hp_w(27,1);hp_w(S_RXCSR1,0x20);
    }else{
        len=hm_tx_packet(pipe,hm_tx_dma);hm_bus.tx_events=(uint8_t)(len/4);
        hp_w(S_TXMAXP,255);hp_w(S_TXCSR1,0x08);hp_w(S_TXCSR1,0x08);
        hp_w(S_TXCSR2,0x22u|p->toggle); /* MODE + WriteDataToggle + DATA0/1 */
        hp_w(24,0x20|(p->ep&15));hp_w(25,1);
        fm1_usb_ep_send(1,hm_tx_dma,len);hp_w(S_TXCSR1,1);
        if(!hm_blocked_ms[pipe])hm_blocked_ms[pipe]=fm1_ms+1;
    }
}
static void hm_usb_deliver_rx(void)
{
    uint32_t n=hp_r(S_RXCOUNT1)|(hp_r(S_RXCOUNT2)<<8);
    fm1_usb_rx_sync();hm_receive(hm_bus.pipe,hm_rx_dma,n);
    hm_pipes[hm_bus.pipe].toggle=(uint8_t)hm_usb_toggle();
}
static void hm_usb_ack_tx(void)
{
    hm_pipe_t *p=&hm_pipes[hm_bus.pipe];uint32_t toggle=hm_usb_toggle();
    if(toggle==hm_bus.old_toggle){hm_fault=HM_USB;p->failed=1;return;}
    hm_tx_ack(hm_bus.pipe,hm_bus.tx_events);p->toggle=(uint8_t)toggle;hm_blocked_ms[hm_bus.pipe]=0;
}
/* Return 1 only when EP1 is quiescent and FADDR may safely change. */
static int hm_usb_poll(void)
{
    uint32_t csr,toggle,age;hm_pipe_t *p;
    if(!hm_bus.state)return 1;
    p=&hm_pipes[hm_bus.pipe];hp_w(S_INDEX,1);
    csr=hp_r(hm_bus.in?S_RXCSR1:S_TXCSR1);
    if(hm_bus.state==1){
        if(hm_bus.in && (csr&1)){
            hm_usb_deliver_rx();hp_w(S_RXCSR1,0x10);hp_w(S_RXCSR1,0);hm_bus.state=0;return 1;
        }
        if(!hm_bus.in && !(csr&3) && !(csr&0xa4)){
            hm_usb_ack_tx();hm_bus.state=0;return 1;
        }
        if(csr&(hm_bus.in?0x44u:0x24u)){p->failed=1;hm_fault=HM_USB;}
        age=fm1_ticks()-hm_bus.started;
        if(age<100u*FM1_TICKS_PER_US && !p->failed)return 0;
        if(hm_bus.in){
            /* RxPktRdy is W0C: writing 1 preserves a packet that arrives here. */
            hp_w(S_RXCSR1,(csr|1u)&~0x20u);
        }else{
            hp_w(S_TXCSR1,0x09);hp_w(S_TXCSR1,0x09);
        }
        hm_bus.settle=fm1_ticks();hm_bus.state=2;return 0;
    }
    if(fm1_ticks()-hm_bus.settle<100u*FM1_TICKS_PER_US)return 0;
    if(hm_bus.in){
        if(csr&1)hm_usb_deliver_rx();else hm_idle_polls++;
        hp_w(S_RXCSR1,0x11);hp_w(S_RXCSR1,0x11);
        if(hp_r(S_RXCSR1)&0x21){hm_fault=HM_QUIESCE;hm_bus.unsafe=1;return 0;}
    }else{
        toggle=hm_usb_toggle();
        if(toggle!=hm_bus.old_toggle){
            hm_tx_ack(hm_bus.pipe,hm_bus.tx_events);p->toggle=(uint8_t)toggle;hm_blocked_ms[hm_bus.pipe]=0;
        }else{
            hm_retries++;
            if(hm_blocked_ms[hm_bus.pipe] && fm1_ms+1-hm_blocked_ms[hm_bus.pipe]>500){p->failed=1;hm_fault=HM_BLOCKED;}
        }
        if(hp_r(S_TXCSR1)&3){hm_fault=HM_QUIESCE;hm_bus.unsafe=1;return 0;}
    }
    hm_bus.state=0;return 1;
}
static void hm_usb_service(uint32_t allow_start)
{
    uint32_t i;
    if(!hm_usb_poll() || !allow_start || hm_bus.unsafe || hp_io_fault)return;
    for(i=0;i<hm_npipe;i++){
        uint32_t pick=(hm_bus.round+i)%hm_npipe;hm_pipe_t *p=&hm_pipes[pick];
        if(p->failed)continue;
        if((p->ep&128)?(hm_forward && hm_can_read(pick)):(hm_fill(&hm_outq[pick])!=0)){
            hm_bus.round=(uint8_t)((pick+1)%hm_npipe);hm_usb_start(pick);return;
        }
    }
}
