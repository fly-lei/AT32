#ifndef ELAB_FOC_MOTOR_H
#define ELAB_FOC_MOTOR_H

#include "elab_device.h"
// #include "AT32F403A_FOC_LIB.h" // 包含你的 FocData_t 定义

typedef struct
{
    uint16_t ThetaOffset;
    uint16_t Cali_flag;
    uint16_t Cali_Status;
    uint16_t Encoder_data;
    float Angle;
    float SinValue;
    float CosValue;
    float Vd;
    float Vq;
    float Valpha;
    float Vbeta;
    float Ia;
    float Ib;
    float Ic;
    float Ialpha;
    float Ibeta;
    float Id;
    float Iq;
    float theta;
    int8_t temp;
} FocData_t;

/* ========================================================================= */
/* FOC 电机实例结构体 (继承自 elab_device_t)                                 */
/* ========================================================================= */
typedef struct
{
    elab_device_t dev; /* ⚠️ 必须在第一位，继承基础设备 */

    /* 1. 硬件绑定层 (直接映射寄存器地址，极致速度) */
    volatile uint16_t *adc_ic_reg;
    volatile uint16_t *adc_ib_reg;
    volatile uint16_t *tmr_arr_reg;
    volatile uint16_t *tmr_ccr1_reg;
    volatile uint16_t *tmr_ccr2_reg;
    volatile uint16_t *tmr_ccr3_reg;

    /* 2. 算法数据层 */
    FocData_t run_foc;   /* 正常运行 FOC 数据 */
    FocData_t audio_foc; /* 音效运行 FOC 数据 */

    float curr_offset[2]; /* [0]=Ic零偏, [1]=Ib零偏 */
    float adc_ib_sum;     /* Ib 两次中断累加值 */
    float adc_ic_sum;     /* Ic 两次中断累加值 */

    uint16_t offset_cnt; /* 零偏采样计数 */
    uint8_t iqr_cnt;     /* 中断分频计数 */
    uint16_t cali_cnt;   /* 校准计数 */

    /* 3. 音乐状态机 */
    uint16_t midi_pr;
    uint16_t midi_cnt;
    uint16_t midi_bit;
    uint16_t midi_flag;

    /* 4. 差异化配置参数 (消除 M1 和 M2 的 if-else 差异) */
    bool is_master;         /* 是否为主电机 (控制 delay1/delay2 逻辑) */
    float cfg_fpq_hz;       /* Vd/Vq 步进幅度 (M1=0.05, M2=0.0025) */
    uint16_t cfg_jiange;    /* 音符时长 (M1=8000, M2=9000) */
    float cfg_case5_vd_max; /* 自检阶段 Vd 上限 (M1=0.15, M2=0.125) */

} elab_foc_motor_t;

/* 对外暴露的 ISR 处理接口 (将在定时器中断中被极速调用) */
extern void elab_foc_isr_handle(elab_foc_motor_t *inst, uint16_t encoder_value);

/* 核心注册函数 */
extern void elab_foc_motor_register(elab_foc_motor_t *inst, elab_device_attr_t *attr);

/* ========================================================================= */
/* 终极造物宏：一句话实例化一个 FOC 控制器                                   */
/* ========================================================================= */
#define REGISTER_FOC_MOTOR(_name, _adc_ic, _adc_ib, _tmr_arr, _ccr1, _ccr2, _ccr3, \
                           _is_master, _fpq_hz, _jiange, _case5_vd, _level)        \
    static elab_foc_motor_t _inst_##_name = {                                      \
        .adc_ic_reg = (volatile uint16_t *)(_adc_ic),                              \
        .adc_ib_reg = (volatile uint16_t *)(_adc_ib),                              \
        .tmr_arr_reg = (volatile uint16_t *)(_tmr_arr),                            \
        .tmr_ccr1_reg = (volatile uint16_t *)(_ccr1),                              \
        .tmr_ccr2_reg = (volatile uint16_t *)(_ccr2),                              \
        .tmr_ccr3_reg = (volatile uint16_t *)(_ccr3),                              \
        .curr_offset = {2048.0f, 2048.0f},                                         \
        .is_master = _is_master,                                                   \
        .cfg_fpq_hz = _fpq_hz,                                                     \
        .cfg_jiange = _jiange,                                                     \
        .cfg_case5_vd_max = _case5_vd};                                            \
    static elab_device_attr_t _attr_##_name = {                                    \
        .name = #_name,                                                            \
        .sole = true};                                                             \
    static void _init_##_name(void)                                                \
    {                                                                              \
        elab_foc_motor_register(&_inst_##_name, &_attr_##_name);                   \
    }                                                                              \
    ELAB_INIT_EXPORT(_init_##_name, _level)

#endif /* ELAB_FOC_MOTOR_H */