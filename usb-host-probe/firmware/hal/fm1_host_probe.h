/* SPDX-License-Identifier: GPL-3.0-only */
/* Host attach sequence from Melodee (Kerem Kilic, recursivefieldinstruments),
 * matching the AC79 SDK USB0 sequence. No VBUS power is driven. */
#pragma once
static void hp_hal_attach(void *ep0)
{
    uint32_t n;
    FM1_CLK_CON1 &= ~3u;
    FM1_USB_EP0_ADR=(uint32_t)(uintptr_t)ep0;
    for(n=1;n<=3;n++)FM1_USB_EP_RADR(n)=(uint32_t)(uintptr_t)ep0;
    FM1_USB_EP4_RADR=(uint32_t)(uintptr_t)ep0;
    FM1_USB_CON0=0;FM1_USB_CON1=0;FM1_USB_IO_CON0=0;
    FM1_USB_IO_CON0|=(1u<<10)|(1u<<9)|(1u<<5)|(1u<<4);
    FM1_USB_IO_CON0&=~((1u<<11)|(1u<<8));FM1_USB_CON0|=0x25u;
}
static void hp_hal_rxbuf(uint32_t ep,void *p)
{
    if(ep==4)FM1_USB_EP4_RADR=(uint32_t)(uintptr_t)p;
    else fm1_usb_ep_rxbuf(ep,p);
    __asm__ volatile("csync" ::: "memory");
}
