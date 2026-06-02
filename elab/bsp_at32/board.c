#include "elab_led_at32.h"
#include "wk_spi.h"
#include "elab_mpu6500.h"
#include "elab_tle5012b.h"
#include "at32f403a_407.h" /* ⚠️ 必须包含雅特力官方头文件，让编译器认识 ADC1、TMR1 等 */
#include "elab_foc_motor.h"

/* ========================================================================= */
/* [双驱自平衡车] 业务控制参数配置表                                     */
/* ========================================================================= */
#define CFG_MOTOR_MASTER true
#define CFG_MOTOR_SLAVE false
#define CFG_M1_MIDI_DUR 8000
#define CFG_M2_MIDI_DUR 9000
#define CFG_M1_AUDIO_STEP 0.05f
#define CFG_M2_AUDIO_STEP 0.0025f
#define CFG_M1_CALI_VD_MAX 0.15f
#define CFG_M2_CALI_VD_MAX 0.125f
/* * 只需要这三行代码！
 * 系统一上电，这 3 个 LED 就会全部自动注册到你的 eLab 设备表里！
 */
REGISTER_LED(led_status, GPIOC, GPIO_PINS_15, EXPORT_DRVIVER); // 状态灯
// REGISTER_LED(led_error,  GPIOA, GPIO_PIN_5,  EXPORT_DRVIVER); // 报警灯
// REGISTER_LED(led_power,  GPIOC, GPIO_PIN_5, EXPORT_DRVIVER); // 电源灯

/* * 等级 1：硬件总线层初始化 (Bus Level)
 * 先把底层的 SPI1 时钟开启，MOSI/MISO 引脚配好
 */
static void hardware_spi3_init(void)
{
    // 开启 SPI1 时钟，配置引脚，配置波特率...
    wk_spi3_init(); // 先初始化 SPI1，确保它的 GPIO 引脚也被正确配置了
}
ELAB_INIT_EXPORT(hardware_spi3_init, 1);

/* * 等级 2：外围设备层初始化 (Device Level)
 * SPI 已经就绪，此时注册并探测 MPU6500 绝对安全！
 */
REGISTER_MPU6500_SPI(imu_sensor, SPI3, GPIOA, GPIO_PINS_15, 2);

/* 1. 【硬件总线】将 SPI1 初始化注册到 Level 1，确保最先启动 */
// ELAB_INIT_EXPORT(wk_spi1_init, 1);

/* 2. 【设备实例化】用一句话凭空创造左轮编码器，绑定 PD5，等级为 2 */
REGISTER_TLE5012B(enc_left, SPI1, GPIOD, GPIO_PINS_5, 1);

/* 3. 【设备实例化】用一句话凭空创造右轮编码器，绑定 PD7，等级为 2 */
REGISTER_TLE5012B(enc_right, SPI1, GPIOD, GPIO_PINS_7, 1);

/* ========================================================================= */
/* [双驱自平衡摩托车] 核心 FOC 硬件设备注册                                  */
/* ========================================================================= */

/* * 🏍️ 凭空创造【左轮电机】：
 * 挂载 ADC1 注入通道 (pdt1/pdt2) 和 TMR1 (pr/c1dt/c2dt/c3dt)
 */
REGISTER_FOC_MOTOR(motor_left,
                   &ADC1->pdt1, &ADC1->pdt2,                         /* 相电流采样 */
                   &TMR1->pr, &TMR1->c1dt, &TMR1->c2dt, &TMR1->c3dt, /* PWM 重载与占空比 */
                   CFG_MOTOR_MASTER, CFG_M1_AUDIO_STEP, CFG_M1_MIDI_DUR, CFG_M1_CALI_VD_MAX,
                   3);

/* * 🏍️ 凭空创造【右轮电机】：
 * 挂载 ADC2 注入通道 (pdt1/pdt2) 和 TMR8 (pr/c1dt/c2dt/c3dt)
 */
REGISTER_FOC_MOTOR(motor_right,
                   &ADC2->pdt1, &ADC2->pdt2,                         /* 相电流采样 */
                   &TMR8->pr, &TMR8->c1dt, &TMR8->c2dt, &TMR8->c3dt, /* PWM 重载与占空比 */
                   CFG_MOTOR_SLAVE, CFG_M2_AUDIO_STEP, CFG_M2_MIDI_DUR, CFG_M2_CALI_VD_MAX,
                   3);