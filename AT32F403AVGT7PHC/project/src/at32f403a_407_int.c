/* add user code begin Header */
/**
 **************************************************************************
 * @file     at32f403a_407_int.c
 * @brief    main interrupt service routines.
 **************************************************************************
 * Copyright (c) 2025, Artery Technology, All rights reserved.
 *
 * The software Board Support Package (BSP) that is made available to
 * download from Artery official website is the copyrighted work of Artery.
 * Artery authorizes customers to use, copy, and distribute the BSP
 * software and its related documentation for the purpose of design and
 * development in conjunction with Artery microcontrollers. Use of the
 * software is governed by this copyright notice and the following disclaimer.
 *
 * THIS SOFTWARE IS PROVIDED ON "AS IS" BASIS WITHOUT WARRANTIES,
 * GUARANTEES OR REPRESENTATIONS OF ANY KIND. ARTERY EXPRESSLY DISCLAIMS,
 * TO THE FULLEST EXTENT PERMITTED BY LAW, ALL EXPRESS, IMPLIED OR
 * STATUTORY OR OTHER WARRANTIES, GUARANTEES OR REPRESENTATIONS,
 * INCLUDING BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT.
 *
 **************************************************************************
 */
/* add user code end Header */

/* includes ------------------------------------------------------------------*/
#include "at32f403a_407_int.h"
#include "usb_app.h"
#include "wk_system.h"
/* private includes ----------------------------------------------------------*/
/* add user code begin private includes */
#include "qpc.h"
#include "elab_foc_motor.h"
#include "elab_tle5012b.h"

/* --------------------------------------------------------- */
/* 声明外部全局指针 (告诉编译器去别的文件里找它们)           */
/* --------------------------------------------------------- */
extern elab_foc_motor_t *g_motor_L;
extern elab_foc_motor_t *g_motor_R;
extern elab_tle5012b_t *g_enc_L;
extern elab_tle5012b_t *g_enc_R;
/* 声明应用层的时钟滴答函数 */
extern void QF_onClockTick(void);
/* add user code end private includes */

/* private typedef -----------------------------------------------------------*/
/* add user code begin private typedef */

/* add user code end private typedef */

/* private define ------------------------------------------------------------*/
/* add user code begin private define */

/* add user code end private define */

/* private macro -------------------------------------------------------------*/
/* add user code begin private macro */

/* add user code end private macro */

/* private variables ---------------------------------------------------------*/
/* add user code begin private variables */

/* add user code end private variables */

/* private function prototypes --------------------------------------------*/
/* add user code begin function prototypes */

/* add user code end function prototypes */

/* private user code ---------------------------------------------------------*/
/* add user code begin 0 */

/* add user code end 0 */

/* external variables ---------------------------------------------------------*/
/* add user code begin external variables */

/* add user code end external variables */

/**
 * @brief  this function handles nmi exception.
 * @param  none
 * @retval none
 */
void NMI_Handler(void)
{
  /* add user code begin NonMaskableInt_IRQ 0 */

  /* add user code end NonMaskableInt_IRQ 0 */

  /* add user code begin NonMaskableInt_IRQ 1 */

  /* add user code end NonMaskableInt_IRQ 1 */
}

/**
 * @brief  this function handles hard fault exception.
 * @param  none
 * @retval none
 */
void HardFault_Handler(void)
{
  /* add user code begin HardFault_IRQ 0 */

  /* add user code end HardFault_IRQ 0 */
  /* go to infinite loop when hard fault exception occurs */
  while (1)
  {
    /* add user code begin W1_HardFault_IRQ 0 */

    /* add user code end W1_HardFault_IRQ 0 */
  }
}

/**
 * @brief  this function handles memory manage exception.
 * @param  none
 * @retval none
 */
void MemManage_Handler(void)
{
  /* add user code begin MemoryManagement_IRQ 0 */

  /* add user code end MemoryManagement_IRQ 0 */
  /* go to infinite loop when memory manage exception occurs */
  while (1)
  {
    /* add user code begin W1_MemoryManagement_IRQ 0 */

    /* add user code end W1_MemoryManagement_IRQ 0 */
  }
}

/**
 * @brief  this function handles bus fault exception.
 * @param  none
 * @retval none
 */
void BusFault_Handler(void)
{
  /* add user code begin BusFault_IRQ 0 */

  /* add user code end BusFault_IRQ 0 */
  /* go to infinite loop when bus fault exception occurs */
  while (1)
  {
    /* add user code begin W1_BusFault_IRQ 0 */

    /* add user code end W1_BusFault_IRQ 0 */
  }
}

/**
 * @brief  this function handles usage fault exception.
 * @param  none
 * @retval none
 */
void UsageFault_Handler(void)
{
  /* add user code begin UsageFault_IRQ 0 */

  /* add user code end UsageFault_IRQ 0 */
  /* go to infinite loop when usage fault exception occurs */
  while (1)
  {
    /* add user code begin W1_UsageFault_IRQ 0 */

    /* add user code end W1_UsageFault_IRQ 0 */
  }
}

/**
 * @brief  this function handles svcall exception.
 * @param  none
 * @retval none
 */
void SVC_Handler(void)
{
  /* add user code begin SVCall_IRQ 0 */

  /* add user code end SVCall_IRQ 0 */
  /* add user code begin SVCall_IRQ 1 */

  /* add user code end SVCall_IRQ 1 */
}

/**
 * @brief  this function handles debug monitor exception.
 * @param  none
 * @retval none
 */
void DebugMon_Handler(void)
{
  /* add user code begin DebugMonitor_IRQ 0 */

  /* add user code end DebugMonitor_IRQ 0 */
  /* add user code begin DebugMonitor_IRQ 1 */

  /* add user code end DebugMonitor_IRQ 1 */
}

/**
 * @brief  this function handles pendsv_handler exception.
 * @param  none
 * @retval none
 */
void PendSV_Handler(void)
{
  /* add user code begin PendSV_IRQ 0 */

  /* add user code end PendSV_IRQ 0 */
  /* add user code begin PendSV_IRQ 1 */

  /* add user code end PendSV_IRQ 1 */
}

/**
 * @brief  this function handles systick handler.
 * @param  none
 * @retval none
 */
void SysTick_Handler(void)
{
  /* add user code begin SysTick_IRQ 0 */

  /* add user code end SysTick_IRQ 0 */

  wk_timebase_handler();

  /* add user code begin SysTick_IRQ 1 */
  QF_onClockTick();
  /* add user code end SysTick_IRQ 1 */
}

/**
 * @brief  this function handles ADC1 & ADC2 handler.
 * @param  none
 * @retval none
 */
void ADC1_2_IRQHandler(void)
{
  /* add user code begin ADC1_2_IRQ 0 */

  /* add user code end ADC1_2_IRQ 0 */

  if (adc_interrupt_flag_get(ADC1, ADC_PCCE_FLAG) != RESET)
  {
    /* add user code begin ADC1_ADC_PCCE_FLAG */
    /* clear flag */
    /* 现在这里绝对不会报错了，编译器已经认识它们了！ */
    /* 你的 FOC 核心计算逻辑 ... */
    if (g_enc_L && g_enc_R)
    {
      uint16_t raw_pos_L = tle5012b_read_raw_fast(g_enc_L);
      uint16_t raw_pos_R = tle5012b_read_raw_fast(g_enc_R);

      elab_foc_isr_handle(g_motor_L, raw_pos_L);
      elab_foc_isr_handle(g_motor_R, raw_pos_R);
    }

    /* ========================================================================= */
    /* 🚨 终极修正：必须清除抢占通道（注入通道）的转换结束标志！                 */
    /* ========================================================================= */
    adc_flag_clear(ADC1, ADC_PCCE_FLAG);
    /* add user code end ADC1_ADC_PCCE_FLAG */
  }

  /* add user code begin ADC1_2_IRQ 1 */

  /* add user code end ADC1_2_IRQ 1 */
}

/**
 * @brief  this function handles USART1 handler.
 * @param  none
 * @retval none
 */

// void USART1_IRQHandler(void)
// {
//   /* add user code begin USART1_IRQ 0 */

//   /* add user code end USART1_IRQ 0 */

//   if(usart_interrupt_flag_get(USART1, USART_IDLEF_FLAG) != RESET)
//   {
//     /* add user code begin USART1_USART_IDLEF_FLAG */
//     /* clear flag */
//     usart_flag_clear(USART1, USART_IDLEF_FLAG);
//     /* add user code end USART1_USART_IDLEF_FLAG */
//   }

//   /* add user code begin USART1_IRQ 1 */

//   /* add user code end USART1_IRQ 1 */
// }

/**
 * @brief  this function handles TMR6 handler.
 * @param  none
 * @retval none
 */
void TMR6_GLOBAL_IRQHandler(void)
{
  /* add user code begin TMR6_GLOBAL_IRQ 0 */

  /* add user code end TMR6_GLOBAL_IRQ 0 */

  /* overflow interrupt management */
  if (tmr_interrupt_flag_get(TMR6, TMR_OVF_FLAG) != RESET)
  {
    /* add user code begin TMR6_TMR_OVF_FLAG */
    /* clear flag */
    tmr_flag_clear(TMR6, TMR_OVF_FLAG);
    /* add user code end TMR6_TMR_OVF_FLAG */
  }

  /* add user code begin TMR6_GLOBAL_IRQ 1 */

  /* add user code end TMR6_GLOBAL_IRQ 1 */
}

/**
 * @brief  this function handles ACC handler.
 * @param  none
 * @retval none
 */
void ACC_IRQHandler(void)
{
  /* add user code begin ACC_IRQ 0 */

  /* add user code end ACC_IRQ 0 */

  /* add user code begin ACC_IRQ 1 */

  /* add user code end ACC_IRQ 1 */
}

/**
 * @brief  this function handles USB Map Low handler.
 * @param  none
 * @retval none
 */
void USBFS_MAPL_IRQHandler(void)
{
  /* add user code begin USBFS_MAPL_IRQ 0 */

  /* add user code end USBFS_MAPL_IRQ 0 */

  wk_usbfs_irq_handler();

  /* add user code begin USBFS_MAPL_IRQ 1 */

  /* add user code end USBFS_MAPL_IRQ 1 */
}

/* add user code begin 1 */

/* add user code end 1 */
