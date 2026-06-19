#include "elab_protocol.h"
#include "at32f403a_407.h"
#include "elab_foc_motor.h"
// 假设我们有获取底层数据的接口
extern float Get_Battery_Voltage(void);
extern float Get_Motor_Temperature(void);
// extern float MPU_Get_Pitch_Angle(void);
extern uint8_t g_system_enable;
extern elab_foc_motor_t *g_motor_L;
extern elab_foc_motor_t *g_motor_R;
/* 串口底层发送接口 (上一节定义的) */
extern void elab_usart1_send_dma(uint8_t *data, uint16_t len);

/* 周期性调用此函数 (例如每 20ms)，向 ESP32 汇报状态 */
void Protocol_Send_Telemetry(void)
{
    static Telemetry_Frame_t tx_frame;

    /* 1. 填入帧头和指令码 */
    tx_frame.head1 = 0xA5;
    tx_frame.head2 = 0x5A;
    tx_frame.cmd = 0x11;

    /* 2. 收集并量化模拟数据 (浮点转定点整数) */
    tx_frame.data.bat_voltage_mv = (uint16_t)(Get_Battery_Voltage() * 1000.0f);
    tx_frame.data.motor_temp_01c = (int16_t)(Get_Motor_Temperature() * 10.0f);

    /* 3. 收集电机 FOC 数据 (Iq 电流与速度) */
    if (g_motor_L && g_motor_R)
    {
        // 假设 Iq 是以安培为单位的浮点数，转换为 mA
        tx_frame.data.motor_L_current = (int16_t)(g_motor_L->run_foc.Iq * 1000.0f);
        tx_frame.data.motor_R_current = (int16_t)(g_motor_R->run_foc.Iq * 1000.0f);

        // // 假设 speed 已经计算好，直接强转
        // tx_frame.data.motor_L_speed = (int16_t)(g_motor_L->speed_rpm);
        // tx_frame.data.motor_R_speed = (int16_t)(g_motor_R->speed_rpm);
    }
    else
    {
        tx_frame.data.motor_L_current = 0;
        tx_frame.data.motor_R_current = 0;
        // tx_frame.data.motor_L_speed = 0;
        // tx_frame.data.motor_R_speed = 0;
    }

    /* 4. 收集姿态数据 */
    // tx_frame.data.mpu_pitch_angle = (int16_t)(MPU_Get_Pitch_Angle() * 10.0f);

    /* 5. 构建系统状态掩码 (Bitmask) */
    uint8_t status = 0;
    if (g_system_enable)
        status |= SYS_STAT_RUNNING;
    if (tx_frame.data.bat_voltage_mv < 6400)
        status |= SYS_ERR_BATT_LOW;
    if (tx_frame.data.motor_temp_01c > 850)
        status |= SYS_ERR_OVER_TEMP;
    // ... 如果 MPU 或 编码器报错，也在这里置位 ...
    tx_frame.data.sys_status = status;

    /* 6. 计算 Checksum (极其重要，防止 ESP32 读错) */
    uint8_t calc_sum = 0;
    uint8_t *calc_ptr = (uint8_t *)&(tx_frame.cmd);
    uint16_t calc_len = sizeof(Telemetry_Frame_t) - 3;

    for (uint16_t i = 0; i < calc_len; i++)
    {
        calc_sum += calc_ptr[i];
    }
    tx_frame.checksum = calc_sum;

    /* 7. 直接将结构体内存丢给串口发出去！ (共 19 字节) */
    /* 将原来的 USART1_Send_Bytes 替换为这句 */
    elab_usart1_send_dma((uint8_t *)&tx_frame, sizeof(Telemetry_Frame_t));
}

// 假设我们有全局变量供状态机或电机使用
int16_t g_remote_speed = 0;
int16_t g_remote_turn = 0;
uint8_t g_system_enable = 0;

/* 极速二进制帧解析引擎 */
void Protocol_Parse_Binary(uint8_t *buffer, uint16_t len)
{
    /* 1. 长度防呆过滤 (一个完整遥控帧是 9 字节) */
    if (len < sizeof(Protocol_Frame_t))
        return;

    /* 2. 遍历缓冲区，寻找帧头 0xA5 0x5A */
    for (uint16_t i = 0; i <= len - sizeof(Protocol_Frame_t); i++)
    {
        if (buffer[i] == 0xA5 && buffer[i + 1] == 0x5A)
        {
            /* 3. 内存直接映射！瞬间将数组转化为结构体 */
            Protocol_Frame_t *frame = (Protocol_Frame_t *)&buffer[i];

            /* 4. 计算校验和 (从 cmd 累加到 data 的最后一个字节) */
            uint8_t calc_sum = 0;
            uint8_t *calc_ptr = (uint8_t *)&(frame->cmd);
            uint16_t calc_len = sizeof(Protocol_Frame_t) - 3; // 减去 2头1尾

            for (uint16_t j = 0; j < calc_len; j++)
            {
                calc_sum += calc_ptr[j];
            }

            /* 5. 校验对比，如果错误直接丢弃该帧 */
            if (calc_sum != frame->checksum)
            {
                continue; // 校验失败，继续向后找下一帧
            }

            /* -------------------------------------------------- */
            /* 🚀 6. 校验通过！提取业务数据并执行动作！           */
            /* -------------------------------------------------- */
            if (frame->cmd == 0x01)
            {
                // 解析急停标志位 (Bit 0)
                if (frame->data.ctrl_flags & 0x01)
                {
                    // 紧急刹车！
                    if (g_motor_L)
                        g_motor_L->target_Vq = 0.0f;
                    if (g_motor_R)
                        g_motor_R->target_Vq = 0.0f;
                }
                else
                {
                    // 正常遥控，将摇杆数据转化为左右轮的查速
                    // 这里仅做数据转存，具体的阿克曼/差速逻辑建议在主循环中处理
                    g_remote_speed = frame->data.target_speed;
                    g_remote_turn = frame->data.target_turn;

                    // 开启自平衡标志位 (Bit 1)
                    g_system_enable = (frame->data.ctrl_flags & 0x02) ? 1 : 0;
                }
            }
            // else if (frame->cmd == 0x02) ... 处理其他指令

            /* 找到并处理完一帧后，直接跳过这段内存 */
            i += sizeof(Protocol_Frame_t) - 1;
        }
    }
}