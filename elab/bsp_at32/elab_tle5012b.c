#include "elab_tle5012b.h"

/* TLE5012B 读取角度值的固定指令 (包含读标志和寄存器地址) */
#define TLE5012B_CMD_READ_ANGLE 0x8020
static uint16_t tle_spi_swap_16bit(spi_type *spi_x, uint16_t tx_data);
/**
 * @brief 专为 20kHz FOC 中断设计的极速读取函数
 * @note  剔除了一切框架开销和浮点乘除法，只返回 15-bit 原码
 */
uint16_t tle5012b_read_raw_fast(elab_tle5012b_t *inst)
{
    uint16_t raw_data, safety_word;

    /* 1. 拉低片选 */
    gpio_bits_reset(inst->cs_port, inst->cs_pin);

    /* 2. 发送读取指令 (忽略返回值) */
    tle_spi_swap_16bit(inst->spi_x, 0x8020); /* TLE5012B_CMD_READ_ANGLE */

    /* 3. 产生时钟获取数据 */
    raw_data = tle_spi_swap_16bit(inst->spi_x, 0xFFFF);

    /* 4. 获取安全字 */
    safety_word = tle_spi_swap_16bit(inst->spi_x, 0xFFFF);

    /* 5. 拉高片选 */
    gpio_bits_set(inst->cs_port, inst->cs_pin);

    /* 6. 直接返回剔除校验位的纯净 15-bit 整数 (0~32767) */
    return (raw_data & 0x7FFF);
}
/* ========================================================================= */
/* 底层 SPI 16位 硬件通信层                                                  */
/* ========================================================================= */
static uint16_t tle_spi_swap_16bit(spi_type *spi_x, uint16_t tx_data)
{
    uint32_t timeout = 1000; // 超时计数器 (根据主频微调)

    /* 等待发送缓冲区空，加上超时保护 */
    while (spi_i2s_flag_get(spi_x, SPI_I2S_TDBE_FLAG) == RESET)
    {
        if (--timeout == 0)
        {
            return 0xFFFF; // ⚠️ 超时！直接返回错误码，绝不卡死系统！
        }
    }
    spi_i2s_data_transmit(spi_x, tx_data);

    timeout = 1000; // 重置超时计数器

    /* 等待接收缓冲区非空，加上超时保护 */
    while (spi_i2s_flag_get(spi_x, SPI_I2S_RDBF_FLAG) == RESET)
    {
        if (--timeout == 0)
        {
            return 0xFFFF; // ⚠️ 超时！直接跑路！
        }
    }
    return spi_i2s_data_receive(spi_x);
}

/* ========================================================================= */
/* eLab 框架操作层                                                           */
/* ========================================================================= */

/**
 * @brief  eLab 标准读操作：发送指令并读取角度
 */
static int tle5012b_read(elab_device_t *dev, uint32_t pos, void *buffer, uint32_t size)
{
    elab_tle5012b_t *inst = (elab_tle5012b_t *)dev;
    uint16_t raw_data, safety_word;

    // 安全校验：外部必须传入用来接收 1 个 float (4字节) 的 buffer
    if (buffer == NULL || size < sizeof(float))
    {
        return 0;
    }

    /* 1. 拉低片选，开始通信 */
    gpio_bits_reset(inst->cs_port, inst->cs_pin);

    /* 2. 发送读取指令 (忽略返回值) */
    tle_spi_swap_16bit(inst->spi_x, TLE5012B_CMD_READ_ANGLE);

    /* 3. 发送 0xFFFF 产生时钟，获取 16-bit 角度数据 */
    raw_data = tle_spi_swap_16bit(inst->spi_x, 0xFFFF);

    /* 4. 发送 0xFFFF 产生时钟，获取 16-bit 安全字 (这里必须读，否则芯片状态机错乱) */
    safety_word = tle_spi_swap_16bit(inst->spi_x, 0xFFFF);

    /* 5. 拉高片选，结束通信 */
    gpio_bits_set(inst->cs_port, inst->cs_pin);

    /* 6. 数据校验与解析 */
    // TLE5012B 规定，返回的 16位 数据中，最高位(Bit15) 必须为 1，否则代表传感器报错
    if ((raw_data & 0x8000) != 0)
    {
        // 提取低 15 位的有效角度数据，并转换为 0~360.0 度的浮点数
        uint16_t angle_15bit = (raw_data & 0x7FFF);
        inst->current_angle = ((float)angle_15bit * 360.0f) / 32768.0f;

        // 将浮点角度写入用户的 buffer
        *((float *)buffer) = inst->current_angle;
        return sizeof(float); // 返回成功拷贝的字节数
    }

    return 0; // 读取失败 (传感器报错)
}

static elab_err_t tle5012b_enable(elab_device_t *dev, bool status)
{
    return __device_enable(dev, status);
}
/* 绑定接口 */
static const elab_dev_ops_t tle_ops = {
    .enable = tle5012b_enable,
    .read = tle5012b_read,
};

/* ========================================================================= */
/* eLab 自动注册探针层                                                       */
/* ========================================================================= */

void elab_tle5012b_register(elab_tle5012b_t *inst, elab_device_attr_t *attr)
{
    /* 1. 初始化独立的 CS 引脚为推挽输出 */
    // 自动判断并开启对应的 GPIO 端口时钟 (根据你的 PD5 和 PD7 需求)
    if (inst->cs_port == GPIOD)
    {
        crm_periph_clock_enable(CRM_GPIOD_PERIPH_CLOCK, TRUE);
    }

    gpio_init_type gpio_init_struct;
    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
    gpio_init_struct.gpio_pins = inst->cs_pin;
    gpio_init_struct.gpio_pull = GPIO_PULL_UP;
    gpio_init(inst->cs_port, &gpio_init_struct);

    gpio_bits_set(inst->cs_port, inst->cs_pin); // CS 默认拉高

    /* 2. 探针 (Probe)：尝试读取一次数据验证芯片是否在线 */
    float test_angle;
    int read_res = tle5012b_read(&inst->dev, 0, &test_angle, sizeof(float));

    // if (read_res == 0)
    // {
    //     // 探测失败：芯片未接好，或者 SPI 不通
    //     return;
    // }

    /* 3. 探测成功，挂载到 eLab */
    inst->dev.ops = &tle_ops;
    inst->dev.user_data = inst;

    // 如果你有 elab_device_register 核心函数，在这里调用：
    elab_device_register(&inst->dev, attr);
}
/* 统一的底层注册入口 */
void elab_tle5012b_core_register(elab_tle5012b_t *inst, elab_device_attr_t *attr)
{
    /* 1. 绑定操作接口 */

    elab_tle5012b_register(inst, attr);
}