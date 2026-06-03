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
/* 双驱 FOC 全局状态机变量 (定义区，分配真实内存)                              */
/* ========================================================================= */

uint8_t Cubli_Cali_Status = 0; /* 全局校准状态（0=空闲, 1=音效校准） */
uint8_t Music_flag = 0;        /* 音效完成标志（1=全部音效播完） */

/* 启动延时计数器 */
uint16_t delay1 = 0;       /* 电机1音效延时计数 */
uint16_t delay2 = 0;       /* 电机2音效启动延时计数 */
uint16_t delay_t = 0x1F40; /* 延时目标 (8000次 ≈ 400 ms) */

/* 音效音调与振幅参数 */
uint8_t hz1 = 0x21;  /* 音符1半周期 = 33  */
uint8_t hz2 = 0x1B;  /* 音符2半周期 = 27  */
uint8_t hz3 = 0x15;  /* 音符3半周期 = 21  */
float fpq_dc = 0.1f; /* Vd/Vq 振幅上限 */
/* ========================================================================= */
/* 纯数学算法层 (无状态)                                                     */
/* ========================================================================= */
/* ========================================================================= */
/* Clarke变换 + Park变换 + 逆Park变换（合并一次执行）                       */
/* ========================================================================= */
static void Clarke_Park_Ipark(FocData_t *foc)
{
    float theta = foc->theta;
    float sin_theta = sinf(theta);
    float cos_theta = cosf(theta);

    foc->SinValue = sin_theta;
    foc->CosValue = cos_theta;

    /* 1. Clarke 变换 (三相静止 -> 两相静止) */
    float Ia = foc->Ia;
    float Ib = foc->Ib;
    float Ic = foc->Ic;

    float sum = (Ia + Ib + Ic) * 0.33333f;
    float Ialpha = Ia - sum;
    float Ibeta = ((Ia - sum) + 2.0f * (Ib - sum)) * 0.57735f;

    foc->Ialpha = Ialpha;
    foc->Ibeta = Ibeta;

    /* 2. Park 变换 (两相静止 -> 两相旋转) */
    float Id = Ialpha * cos_theta + Ibeta * sin_theta;
    float Iq = -Ialpha * sin_theta + Ibeta * cos_theta; // Iq 正向定义

    foc->Id = Id;
    foc->Iq = Iq;

    /* 3. Inverse Park 变换 (两相旋转电压 -> 两相静止电压) */
    float Vd = foc->Vd;
    float Vq = foc->Vq;

    float Valpha = Vd * cos_theta - Vq * sin_theta; // 标准 Inverse Park
    float Vbeta = Vd * sin_theta + Vq * cos_theta;

    foc->Valpha = Valpha;
    foc->Vbeta = Vbeta;
}
/* ========================================================================= */
/* svpwm() 空间矢量脉宽调制（7段式中心对齐 SVPWM）                          */
/* ========================================================================= */
static void svpwm(FocData_t *foc, elab_foc_motor_t *inst)
{
    uint16_t Ta = 0, Tb = 0, Tc = 0;
    float t1, t2, t0, tsum;
    float Valpha = foc->Valpha;
    float Vbeta = foc->Vbeta;
    float X, Y, U, V, W;
    uint16_t arr = *(inst->tmr_arr_reg); /* 动态读取当前电机对应的 ARR 自动重载值 */
    uint8_t sector;

    X = Valpha * 0.86603f;
    Y = Vbeta * 0.5f;
    U = -Vbeta; /* v7 */
    V = X + Y;  /* v8 */
    W = Y - X;  /* v9 */

    /* 扇区判断：1~6 */
    sector = (uint8_t)((U > 0.0f) + 2 * (V > 0.0f) + 4 * (W > 0.0f));

    switch (sector)
    {
    /* 扇区1（U>0, V≤0, W≤0）*/
    case 1:
        t1 = -W;
        t2 = -V;
        tsum = t1 + t2;
        if (tsum > 1.0f)
        {
            t1 /= tsum;
            t2 /= tsum;
        }
        t0 = (1.0f - t1 - t2) * 0.5f;
        Ta = (uint16_t)(uint32_t)((t2 + t0) * arr);
        Tb = (uint16_t)(uint32_t)((t1 + t2 + t0) * arr);
        Tc = (uint16_t)(uint32_t)(t0 * arr);
        break;

    /* 扇区2（U≤0, V>0, W≤0）*/
    case 2:
        t1 = -U;
        t2 = -W;
        tsum = t1 + t2;
        if (tsum > 1.0f)
        {
            t1 /= tsum;
            t2 /= tsum;
        }
        t0 = 1.0f - t1 - t2;
        Ta = (uint16_t)(uint32_t)((t0 * 0.5f) * arr);
        Tb = (uint16_t)(uint32_t)((t2 + t0 * 0.5f) * arr);
        Tc = (uint16_t)(uint32_t)((t1 + t2 + t0 * 0.5f) * arr);
        break;

    /* 扇区3（U>0, V>0, W≤0）*/
    case 3:
        t1 = -Vbeta; /* = U */
        t2 = V;
        tsum = V - Vbeta; /* = V + U */
        if (tsum > 1.0f)
        {
            t1 = U / tsum;
            t2 = V / tsum;
        }
        t0 = (1.0f - t1 - t2) * 0.5f;
        Ta = (uint16_t)(uint32_t)(t0 * arr);
        Tb = (uint16_t)(uint32_t)((t1 + t2 + t0) * arr);
        Tc = (uint16_t)(uint32_t)((t2 + t0) * arr);
        break;

    /* 扇区4（U≤0, V≤0, W>0）*/
    case 4:
        t1 = -V;
        t2 = -U;
        tsum = t1 + t2;
        if (tsum > 1.0f)
        {
            t1 /= tsum;
            t2 /= tsum;
        }
        t0 = (1.0f - t1 - t2) * 0.5f;
        Ta = (uint16_t)(uint32_t)((t1 + t2 + t0) * arr);
        Tb = (uint16_t)(uint32_t)(t0 * arr);
        Tc = (uint16_t)(uint32_t)((t2 + t0) * arr);
        break;

    /* 扇区5（U>0, V≤0, W>0）*/
    case 5:
        t1 = W;
        t2 = U;
        tsum = W - Vbeta; /* = W + U */
        if (tsum > 1.0f)
        {
            t1 = W / tsum;
            t2 = U / tsum;
        }
        t0 = (1.0f - t1 - t2) * 0.5f;
        Ta = (uint16_t)(uint32_t)((t1 + t2 + t0) * arr);
        Tb = (uint16_t)(uint32_t)((t2 + t0) * arr);
        Tc = (uint16_t)(uint32_t)(t0 * arr);
        break;

    /* 扇区6（U≤0, V>0, W>0）*/
    case 6:
        t1 = V;
        t2 = W;
        tsum = W - (-X - Y); /* = W + X + Y = 2Y = Vbeta */
        if (tsum > 1.0f)
        {
            t1 = V / tsum;
            t2 = W / tsum;
        }
        t0 = (1.0f - t1 - t2) * 0.5f;
        Ta = (uint16_t)(uint32_t)((t2 + t0) * arr);
        Tb = (uint16_t)(uint32_t)(t0 * arr);
        Tc = (uint16_t)(uint32_t)((t1 + t2 + t0) * arr);
        break;

    default:
        /* sector=0 或 7（理论不可达），Ta/Tb/Tc 保持 0 */
        break;
    }

    /* =====================================================================
     * 🚀 极致抽象：通过对象绑定的寄存器指针，直接将占空比写入对应的定时器通道
     * 无需关心当前跑的是 TMR1 还是 TMR8，框架会自动路由！
     * ===================================================================== */
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
            /* 1. 解析绝对角度 */
            // 用 32768 减去当前读数，人为反转编码器方向
            // inst->run_foc.Encoder_data = (inst->run_foc.ThetaOffset + (ENCODER_RESOLUTION - EncoderValue)) % ENCODER_RESOLUTION;
            inst->run_foc.Encoder_data = (inst->run_foc.ThetaOffset + EncoderValue) % ENCODER_RESOLUTION;
            inst->run_foc.Angle = (float)inst->run_foc.Encoder_data * ENC_TO_DEG;
            inst->run_foc.theta = inst->run_foc.Angle * POLE_PAIRS_SCALE;

            /* 2. 踩下油门：应用目标扭矩/电压 */
            // 如果你的 M1_Control() 是写了电流环 PID 的，你可以把 target_Vq 作为 PID 的输出。
            // 如果是最简单的电压开环测试，直接把上层给的 target_Vq 塞进 FOC：
            inst->run_foc.Vd = 0.0f;            /* 表贴式电机 D 轴始终给 0 */
            inst->run_foc.Vq = inst->target_Vq; /* Q 轴施加目标电压产生扭矩 */

            /* 如果有电流环 PID，代码应该类似这样：
               inst->run_foc.Vq = PI_Controller(&inst->iq_pi, inst->target_Iq - inst->run_foc.Iq);
               inst->run_foc.Vd = PI_Controller(&inst->id_pi, 0.0f - inst->run_foc.Id);
            */
            break;
        case 2:
            /* 1. 强制电角度为 0 (让磁场固定在 D 轴上) */
            inst->run_foc.theta = 0.0f;

            /* 2. 缓慢注入牵引电压 (像拉皮筋一样把转子强行拉正) */
            // 注意：Vd 不能瞬间给满，必须像斜坡一样慢慢增加，防止瞬间电流过大或发出“砰”的异响
            if (inst->run_foc.Vd < 0.15f)
            {
                inst->run_foc.Vd += 0.0001f;
                if (inst->run_foc.Vd > 0.15f)
                {
                    inst->run_foc.Vd = 0.15f;
                }
            }
            inst->run_foc.Vq = 0.0f; // Q 轴不出力，不产生旋转力矩

            /* 3. 持续记录当前编码器位置作为机械零点偏移量 (ThetaOffset) */
            // ⚠️ 极其重要：这里的公式必须和你在 case 1 里的极性方向完全匹配！

            // 【情况 A】：如果你在上一回合没有改代码，是通过“物理换线”解决的震动
            inst->run_foc.ThetaOffset = ENCODER_RESOLUTION - EncoderValue;

            // 【情况 B】：如果你在上一回合是通过修改 case 1 (软件反转) 解决的震动
            // 那么这里就必须改成直接赋值，否则偏移量就全乱了：
            // inst->run_foc.ThetaOffset = EncoderValue;

            /* 4. 倒计时等待：保持这个状态足够长时间 (CALI_HOLD_CNT)，确保转子完全稳定不动 */
            if (++inst->cali_cnt >= CALI_HOLD_CNT)
            {
                inst->cali_cnt = 0;
                inst->run_foc.Cali_Status = 1;
                inst->run_foc.Cali_flag = 1; /* ⚠️ 核心：校准完自动切入 case 1！ */
                inst->run_foc.Vd = 0.0f;     /* 撤销校准用的 D 轴牵引电压 */

                // (注意：这里绝对不能清零 ThetaOffset，因为 case 1 马上就要用到它)
            }
            break;
            /* ------------------------------------------------------------------
             * case 3：开环强拖 (V/F 控制) —— 硬件诊断模式
             * ----------------------------------------------------------------*/
        case 3:
            /* 1. 无视编码器反馈，强行让电角度匀速递增 (拖着磁场转) */
            // 这里的 0.002f 决定了旋转的速度。如果想转快点，可以改为 0.005f
            inst->run_foc.theta += 0.002f;
            if (inst->run_foc.theta > 6.2831853f)
            { // 超过 2π 则回绕
                inst->run_foc.theta -= 6.2831853f;
            }

            /* 2. 给定恒定的牵引电压 */
            // ⚠️ 警告：开环极度耗电且发热快，电压千万别给大！从 0.05f 试起。
            inst->run_foc.Vd = 0.06f; // 用 D 轴或 Q 轴电压都可以，这里注固定电压
            inst->run_foc.Vq = 0.0f;

            /* 3. 后台悄悄记录编码器的真实读数 (用于排查极对数和方向) */
            inst->run_foc.Encoder_data = EncoderValue;
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