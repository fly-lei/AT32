#include "elab_foc_motor.h"
#include <math.h>

/* 保留你原有的全局变量 (用于双电机协同和系统宏) */
extern uint8_t Cubli_Cali_Status;
extern uint8_t Music_flag;
extern uint16_t delay1;
extern uint16_t delay2;
extern uint16_t delay_t;
extern uint8_t hz1, hz2, hz3;
extern float fpq_dc;

#define OFFSET_SAMPLE_CNT 5000u
#define CALI_HOLD_CNT 20000u
#define ENCODER_RESOLUTION 0x8000u
#define ENC_TO_DEG 0.010986f
#define POLE_PAIRS_SCALE 0.12217f
#define CURR_SCALE 0.0040283f

/* ========================================================================= */
/* 纯数学算法层 (无状态)                                                     */
/* ========================================================================= */
static void Clarke_Park_Ipark(FocData_t *foc)
{
    float theta = foc->theta;
    float sin_theta = sinf(theta);
    float cos_theta = cosf(theta);

    // ... (保留你原本一模一样的 Clarke/Park 代码)
}

/* ========================================================================= */
/* SVPWM 生成器 (操作对象化)                                                 */
/* ========================================================================= */
static void svpwm(FocData_t *foc, elab_foc_motor_t *inst)
{
    uint16_t Ta = 0, Tb = 0, Tc = 0;
    float Valpha = foc->Valpha;
    float Vbeta = foc->Vbeta;
    uint16_t arr = *(inst->tmr_arr_reg); /* 动态读取对应的 ARR */

    // ... (保留你原本一模一样的 7 段式扇区计算，求出 Ta, Tb, Tc)

    /* 极致抽象：直接写入绑定的寄存器，无需判断 motor_id ! */
    *(inst->tmr_ccr1_reg) = Ta;
    *(inst->tmr_ccr2_reg) = Tb;
    *(inst->tmr_ccr3_reg) = Tc;
}

/* ========================================================================= */
/* 音效播放器 (合并 Start_Music1 和 Start_Music8)                            */
/* ========================================================================= */
static void Start_Music(elab_foc_motor_t *inst)
{
    float fpq = inst->cfg_fpq_hz;

/* 振荡内核宏：使用传入的 fpq，操作 inst->audio_foc */
#define MUSIC_STEP()                             \
    do                                           \
    {                                            \
        if (inst->midi_bit)                      \
        {                                        \
            if (inst->audio_foc.Vq < fpq_dc)     \
            {                                    \
                inst->audio_foc.Vq += fpq;       \
                if (inst->audio_foc.Vq > fpq_dc) \
                    inst->audio_foc.Vq = fpq_dc; \
            }                                    \
            if (inst->audio_foc.Vd > 0.0f)       \
            {                                    \
                inst->audio_foc.Vd -= fpq;       \
                if (inst->audio_foc.Vd < 0.0f)   \
                    inst->audio_foc.Vd = 0.0f;   \
            }                                    \
        }                                        \
        else                                     \
        {                                        \
            if (inst->audio_foc.Vd < fpq_dc)     \
            {                                    \
                inst->audio_foc.Vd += fpq;       \
                if (inst->audio_foc.Vd > fpq_dc) \
                    inst->audio_foc.Vd = fpq_dc; \
            }                                    \
            if (inst->audio_foc.Vq > 0.0f)       \
            {                                    \
                inst->audio_foc.Vq -= fpq;       \
                if (inst->audio_foc.Vq < 0.0f)   \
                    inst->audio_foc.Vq = 0.0f;   \
            }                                    \
        }                                        \
    } while (0)

    // ... (保留你原本的音符 1/2/3 切换逻辑，把 t1_midi_xxx 换成 inst->midi_xxx，把 jiange_a 换成 inst->cfg_jiange)

    if (inst->midi_flag == 3)
    {
        inst->audio_foc.Vd = 0.0f;
        inst->audio_foc.Vq = 0.0f;
        if (!inst->is_master)
            Music_flag = 1; /* 从机播放完毕，拉高全局标志 */
    }

#undef MUSIC_STEP

    /* 主机专有逻辑：递增 delay2 触发从机 */
    if (inst->is_master && delay2 < delay_t)
    {
        delay2++;
    }
}

/* ========================================================================= */
/* 核心中断控制器 (合并 M1_FOC_handle 和 M2_FOC_handle)                      */
/* ========================================================================= */
void elab_foc_isr_handle(elab_foc_motor_t *inst, uint16_t EncoderValue)
{
    /* 阶段0：零偏校准 */
    if (inst->offset_cnt < OFFSET_SAMPLE_CNT)
    {
        inst->offset_cnt++;
        inst->curr_offset[0] += ((float)(*inst->adc_ic_reg) - inst->curr_offset[0]) * 0.5f;
        inst->curr_offset[1] += ((float)(*inst->adc_ib_reg) - inst->curr_offset[1]) * 0.5f;
        return;
    }

    /* 阶段1：音效 */
    if (Music_flag != 1 && Cubli_Cali_Status != 0)
    {
        if (Cubli_Cali_Status == 1)
        {
            bool should_play = inst->is_master ? (delay1 >= delay_t) : (delay2 >= delay_t);

            if (should_play)
            {
                Start_Music(inst);
            }
            else if (inst->is_master)
            {
                delay1++; /* 仅主机递增 delay1 */
            }
            Clarke_Park_Ipark(&inst->audio_foc);
            svpwm(&inst->audio_foc, inst);
        }
        return;
    }

    /* 阶段2：正常 FOC 运行 */
    inst->adc_ic_sum += (float)((int)((float)(*inst->adc_ic_reg) - inst->curr_offset[0])) * CURR_SCALE;
    inst->adc_ib_sum += (float)((int)((float)(*inst->adc_ib_reg) - inst->curr_offset[1])) * CURR_SCALE;

    if (++inst->iqr_cnt >= 2u)
    {
        inst->iqr_cnt = 0;

        inst->run_foc.Ic = inst->adc_ic_sum * 0.5f;
        inst->run_foc.Ib = inst->adc_ib_sum * 0.5f;
        inst->run_foc.Ia = -inst->run_foc.Ib - inst->run_foc.Ic;

        inst->adc_ic_sum = 0.0f;
        inst->adc_ib_sum = 0.0f;

        switch (inst->run_foc.Cali_flag)
        {
        case 1:
            // ... 闭环控制 ...
            // 注意：由于 M1_Control() 可能是强业务耦合的，如果它也在外部，可以通过函数指针回调，或者直接在这里处理 PID
            break;
        case 2:
            // ... 校准 ...
            break;
        case 5:
            inst->run_foc.Cali_Status = 5;
            if (inst->cali_cnt >= CALI_HOLD_CNT)
            {
                // ... 对齐完成 ...
            }
            else
            {
                inst->cali_cnt++;
                inst->run_foc.theta = 0.0f;
                if (inst->run_foc.Vd < inst->cfg_case5_vd_max)
                { /* 动态上限 */
                    inst->run_foc.Vd += 0.0001f;
                    // 注意：这里为了完美复刻你的代码，M1赋值是0.125，上限是0.15。如果是通用的，建议直接写死 inst->run_foc.Vd = 0.125f;
                }
            }
            break;
        default:
            // ... 软关断 ...
            break;
        }

        Clarke_Park_Ipark(&inst->run_foc);
        svpwm(&inst->run_foc, inst);
    }
}

/* ========================================================================= */
/* eLab 框架挂载                                                             */
/* ========================================================================= */
void elab_foc_motor_register(elab_foc_motor_t *inst, elab_device_attr_t *attr)
{

    // 如果后续你需要通过状态机发送目标电流/速度，可以在这里挂载 ops 接口！
    elab_device_register(inst, attr);
}