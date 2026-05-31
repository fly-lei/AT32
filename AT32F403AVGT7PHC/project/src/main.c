/* add user code begin Header */
/**
  **************************************************************************
  * @file     main.c
  * @brief    main program
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

/* Includes ------------------------------------------------------------------*/
#include "at32f403a_407_wk_config.h"
#include "wk_acc.h"
#include "wk_adc.h"
#include "wk_debug.h"
#include "wk_spi.h"
#include "wk_tmr.h"
#include "wk_usbfs.h"
#include "wk_gpio.h"
#include "usb_app.h"
#include "wk_system.h"

/* private includes ----------------------------------------------------------*/
/* add user code begin private includes */
#include "qpc.h"
#include "elab.h"

extern void App_System_Start(void);
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

/* 修复 2：提供临界区挂钩 (QP/C 需要靠它们来防止中断打断状态机) */
void QF_crit_entry_(void) {
    __disable_irq(); /* 关中断 */
}
void QF_crit_exit_(void) {
    __enable_irq();  /* 开中断 */
}

/* 修复 3：顺手补上 QV 内核强制要求的空闲回调 */
void QV_onIdle(void) {
    // __enable_irq();
    // __WFI(); /* Wait For Interrupt：系统没事干时进入休眠，降低单片机功耗 */
    QF_INT_ENABLE();
}
/* add user code end 0 */

/**
  * @brief main function.
  * @param  none
  * @retval none
  */
int main(void)
{
  /* add user code begin 1 */

  /* add user code end 1 */

  /* system clock config. */
  wk_system_clock_config();

  /* config periph clock. */
  wk_periph_clock_config();

  /* init debug function. */
  wk_debug_config();

  /* nvic config. */
  wk_nvic_config();

  /* timebase config for
     void wk_delay_ms(uint32_t delay); */
  wk_timebase_init();

  /* init gpio function. */
  wk_gpio_config();

  /* init adc1 function. */
  wk_adc1_init();

  /* init spi1 function. */
  wk_spi1_init();

  /* init spi3 function. */
  wk_spi3_init();

  /* init tmr1 function. */
  wk_tmr1_init();

  /* init tmr6 function. */
  wk_tmr6_init();

  /* init acc function. */
  wk_acc_init();

  /* init usbfs function. */
  wk_usbfs_init();

  /* init usb app function. */
  wk_usb_app_init();

  /* add user code begin 2 */
  eLab_InitAll(); 
  App_System_Start();
  QF_run(); /* 进入 QP/C 的事件循环，永不返回！ */
  /* add user code end 2 */

  while(1)
  {
    wk_usb_app_task();

    /* add user code begin 3 */

    /* add user code end 3 */
  }
}

  /* add user code begin 4 */

  /* add user code end 4 */
