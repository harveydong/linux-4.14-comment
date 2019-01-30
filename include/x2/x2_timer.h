#ifndef _X2_TIMER_H
#define _X2_TIMER_H


/* X2 TIMER register offsets */
#define X2_TIMER_TMREN_REG		  0x00
#define X2_TIMER_TMRSTART_REG	  0x04
#define X2_TIMER_TMRSTOP_REG	  0x08
#define X2_TIMER_TMRMODE_REG	  0x0C
#define X2_TIMER_TMR0TGTL_REG	  0x10
#define X2_TIMER_TMR0TGTH_REG	  0x14
#define X2_TIMER_TMR0DL_REG 	  0x18
#define X2_TIMER_TMR0DH_REG 	  0x1C
#define X2_TIMER_TMR1TGT_REG	  0x20
#define X2_TIMER_TMR1D_REG		  0x24
#define X2_TIMER_WDTGT_REG		  0x28
#define X2_TIMER_WDWAIT_REG 	  0x2C
#define X2_TIMER_WD1D_REG		  0x30
#define X2_TIMER_WD2D_REG		  0x34
#define X2_TIMER_WDCLR_REG		  0x38
#define X2_TIMER_TMR_SRCPND_REG   0x3C
#define X2_TIMER_TMR_INTMASK_REG  0x40
#define X2_TIMER_TMR_SETMASK_REG  0x44
#define X2_TIMER_TMR_UNMASK_REG   0x48

/* X2 TIMER register op-bit Masks */
#define X2_TIMER_T0START		  BIT(0)
#define X2_TIMER_T1START		  BIT(1)
#define X2_TIMER_T2START		  BIT(2)
#define X2_TIMER_T0STOP 		  BIT(0)
#define X2_TIMER_T1STOP 		  BIT(1)
#define X2_TIMER_T2STOP 		  BIT(2)
#define X2_TIMER_ONE_MODE		  0x0	  /* one-time mode */
#define X2_TIMER_PRD_MODE		  0x1	  /* periodical mode */
#define X2_TIMER_CON_MODE		  0x2	  /* continuous mode */
#define X2_TIMER_WDT_MODE		  0x3	  /* watchdog mode, only for timer2 of timer module0 */
#define X2_TIMER_T0MODE_OFFSET	  0x0
#define X2_TIMER_T1MODE_OFFSET	  0x4
#define X2_TIMER_T2MODE_OFFSET	  0x8
#define X2_TIMER_WDT_RESET		  BIT(0)
#define X2_TIMER_T0_INTMASK		  BIT(0)
#define X2_TIMER_T1_INTMASK		  BIT(1)
#define X2_TIMER_T2_INTMASK		  BIT(2)
#define X2_TIMER_WDT_INTMASK	  X2_TIMER_T2_INTMASK


#define X2_TIMER_REF_CLOCK		  24000000


#endif
