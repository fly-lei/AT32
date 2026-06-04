#include "elab_adc3.h"

const BatteryCurve_t bat_curve[] = {
    {8.40f, 100},
    {8.10f, 90},
    {7.96f, 80},
    {7.84f, 70},
    {7.68f, 60},
    {7.52f, 50},
    {7.40f, 40},
    {7.28f, 30},
    {7.14f, 20},
    {6.90f, 10},
    {6.60f, 5},
    {6.00f, 0} // 平衡车强负载下，截止电压可设低至 6.0V 防止急加速断电
};
/* 将测得的实际电压转换为 0~100% */
uint8_t Get_Battery_Percent(float real_vol)
{
    /* 1. 极值保护 */
    if (real_vol >= bat_curve[0].voltage)
        return 100;
    if (real_vol <= bat_curve[CURVE_POINTS - 1].voltage)
        return 0;

    /* 2. 遍历查找表，找到电压落在哪两个点之间 */
    for (int i = 0; i < CURVE_POINTS - 1; i++)
    {
        if (real_vol < bat_curve[i].voltage && real_vol >= bat_curve[i + 1].voltage)
        {
            /* 3. 在这两个点之间进行线性插值，使电量过渡平滑 */
            float v_diff = bat_curve[i].voltage - bat_curve[i + 1].voltage;
            float p_diff = (float)(bat_curve[i].soc - bat_curve[i + 1].soc);
            float v_offset = real_vol - bat_curve[i + 1].voltage;

            return bat_curve[i + 1].soc + (uint8_t)((v_offset / v_diff) * p_diff);
        }
    }
    return 0;
}
/* 获取真实的电机温度 (摄氏度) */
float Get_Motor_Temperature(void)
{
    /* 1. 从 DMA 数组读取原始 ADC 值 (假设 12位 ADC，最大 4095) */
    float adc_val = (float)g_adc3_buffer[1];

    /* 防除零保护 (拔掉传感器或者短路时) */
    if (adc_val < 10.0f || adc_val > 4085.0f)
    {
        return -99.0f; // 传感器异常标志
    }

    /* 2. 硬件分压逆推 NTC 当前阻值
     * 公式: V_adc = 3.3 * (3000 / (R_ntc + 3000))
     * 化简得到 R_ntc 的极速算法：
     */
    float r_ntc = R_DIVIDER * (4095.0f - adc_val) / adc_val;

    /* 3. 使用标准 B值公式计算开尔文温度 (Kelvin)
     * T = 1 / ( 1/T25 + (1/B)*ln(Rt/R25) )
     */
    float kelvin = 1.0f / ((1.0f / 298.15f) + (1.0f / NTC_B_VALUE) * logf(r_ntc / NTC_R25));

    /* 4. 开尔文转摄氏度 */
    float celsius = kelvin - 273.15f;

    return celsius;
}
float Get_Battery_Voltage(void)
{
    /* 直接从 DMA 数组的第 0 个元素拿原始数据 */
    uint16_t raw_vol = g_adc3_buffer[0];

    /* 换算公式：(原始值 / 4095) * 3.3V * 分压比 */
    float real_vol = ((float)raw_vol / 4095.0f) * ADC_REF_VOLTAGE * VOLTAGE_DIVIDER_RATIO;
    return real_vol;
}