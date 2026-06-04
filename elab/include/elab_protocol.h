#ifndef __ELAB_PROTOCOL_H
#define __ELAB_PROTOCOL_H

#include <stdint.h>

/* 强制 1 字节对齐，绝不能漏！ */
#pragma pack(push, 1)

/* ========================================================================= */
/* AT32 上报给 ESP32 的全局状态遥测数据包 (CMD = 0x11)                       */
/* ========================================================================= */

/* 系统健康状态位掩码 (Bitmask) */
#define SYS_STAT_RUNNING 0x01  // 0:待机 1:平衡控制中
#define SYS_ERR_MPU 0x02       // 1:陀螺仪掉线/异常
#define SYS_ERR_ENC_L 0x04     // 1:左编码器故障
#define SYS_ERR_ENC_R 0x08     // 1:右编码器故障
#define SYS_ERR_BATT_LOW 0x10  // 1:电池极低警告
#define SYS_ERR_OVER_TEMP 0x20 // 1:电机过温保护

typedef struct
{
    uint16_t bat_voltage_mv; // 电池电压 (毫伏, 例如 7850 = 7.85V)
    int16_t motor_temp_01c;  // 电机温度 (0.1摄氏度, 例如 365 = 36.5℃)

    int16_t motor_L_current; // 左轮实际相电流 (mA)
    int16_t motor_R_current; // 右轮实际相电流 (mA)

    int16_t motor_L_speed; // 左轮实时转速 (RPM)
    int16_t motor_R_speed; // 右轮实时转速 (RPM)

    int16_t mpu_pitch_angle; // 车体俯仰角 (0.1度, 前倾为正, 例如 152 = 15.2度)

    uint8_t sys_status; // 系统状态掩码 (按上面的宏位定义)
} Telemetry_Payload_t;

/* 完整的上行通信帧结构 (复用之前的头尾逻辑) */
typedef struct
{
    uint8_t head1;            // 固定 0xA5
    uint8_t head2;            // 固定 0x5A
    uint8_t cmd;              // 遥测指令 = 0x11
    Telemetry_Payload_t data; // 遥测数据 (15 字节)
    uint8_t checksum;         // 校验和
} Telemetry_Frame_t;

/* ESP32 下发给 AT32 的遥控数据包 (Payload) */
typedef struct
{
    int16_t target_speed; // 目标速度 (-1000 ~ 1000)
    int16_t target_turn;  // 目标转向 (-1000 ~ 1000)
    uint8_t ctrl_flags;   // 控制标志位
} Ctrl_Payload_t;

/* 完整的通信帧结构 */
typedef struct
{
    uint8_t head1;       // 0xA5
    uint8_t head2;       // 0x5A
    uint8_t cmd;         // 指令码
    Ctrl_Payload_t data; // 数据区
    uint8_t checksum;    // 校验和
} Protocol_Frame_t;

#pragma pack(pop)

void Protocol_Parse_Binary(uint8_t *buffer, uint16_t len);

#endif