/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
static void hp_hal_attach(void *p) {(void)p;}
static void hp_hal_rxbuf(uint32_t ep,void *p) {fake_rxbuf[ep]=p;}
