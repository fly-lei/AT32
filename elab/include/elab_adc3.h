#include <stdbool.h>
#include <stdio.h>
#include "elab_device.h"
/* 定义 2S 动力电池的真实放电曲线 (必须按电压从高到低排列) */
typedef struct
{
    float voltage;
    uint8_t soc; // 0-100%
} BatteryCurve_t;

#define CURVE_POINTS (sizeof(bat_curve) / sizeof(bat_curve[0]))
/* NTC 物理参数宏定义 */
#define NTC_R25 10000.0f    // 25度时的阻值 10k
#define NTC_B_VALUE 3380.0f // B值 3380K
#define R_DIVIDER 3000.0f   // 下方的分压电阻 3k
/* 假设你的电池是通过 10K 和 1K 电阻分压的 (分压比 11:1) */
#define VOLTAGE_DIVIDER_RATIO 11.0f
#define ADC_REF_VOLTAGE 3.3f
uint8_t Get_Battery_Percent(float real_vol);
float Get_Battery_Voltage(void);
float Get_Motor_Temperature(void);
extern uint16_t g_adc3_buffer[3]; // 定义全局 DMA 缓冲区
