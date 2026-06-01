#include "elab_mpu6500.h"
#include "at32f403a_407.h" // 确保引入了雅特力标准外设库

/* 寄存器地址宏定义 */
#define MPU6500_WHO_AM_I_REG    0x75
#define MPU6500_PWR_MGMT_1_REG  0x6B
#define MPU6500_ACCEL_XOUT_H    0x3B // 14字节连续数据的起始地址
#define MPU6500_ID              0x70

#define SPI_READ_CMD            0x80 // MPU6500 规定最高位为 1 代表读操作

/* ========================================================================= */
/* 底层 SPI 硬件通信层                                                       */
/* ========================================================================= */

/**
  * @brief  SPI 基础单字节收发操作 (底层核心)
  */
static uint8_t mpu_spi_swap_byte(spi_type *spi_x, uint8_t tx_data) {
    /* 等待发送缓冲区空 */
    while(spi_i2s_flag_get(spi_x, SPI_I2S_TDBE_FLAG) == RESET);
    spi_i2s_data_transmit(spi_x, tx_data);
    
    /* 等待接收缓冲区非空 */
    while(spi_i2s_flag_get(spi_x, SPI_I2S_RDBF_FLAG) == RESET);
    return (uint8_t)spi_i2s_data_receive(spi_x);
}

/**
  * @brief  读取单个寄存器
  */
static uint8_t mpu_spi_read_reg(elab_mpu6500_t *inst, uint8_t reg) {
    uint8_t value = 0;
    
    gpio_bits_reset(inst->cs_port, inst->cs_pin);        // 1. 拉低 CS
    mpu_spi_swap_byte(inst->spi_x, reg | SPI_READ_CMD);  // 2. 发送寄存器地址 (带读标志)
    value = mpu_spi_swap_byte(inst->spi_x, 0xFF);        // 3. 发送 Dummy 字节获取返回值
    gpio_bits_set(inst->cs_port, inst->cs_pin);          // 4. 拉高 CS
    
    return value;
}

/**
  * @brief  写入单个寄存器 (用于配置和唤醒传感器)
  */
static void mpu_spi_write_reg(elab_mpu6500_t *inst, uint8_t reg, uint8_t data) {
    gpio_bits_reset(inst->cs_port, inst->cs_pin);        // 1. 拉低 CS
    mpu_spi_swap_byte(inst->spi_x, reg & ~SPI_READ_CMD); // 2. 发送寄存器地址 (写标志)
    mpu_spi_swap_byte(inst->spi_x, data);                // 3. 发送要写入的数据
    gpio_bits_set(inst->cs_port, inst->cs_pin);          // 4. 拉高 CS
}

/* ========================================================================= */
/* eLab 框架操作层                                                           */
/* ========================================================================= */
/* enable 接口 */
static elab_err_t mpu6500_enable(elab_device_t *dev, bool status) {
    return ELAB_OK;
}
/**
  * @brief  eLab 标准读操作回调：连续读取 14 字节原始数据并解析
  */static int mpu6500_read(elab_device_t *dev, uint32_t pos, void *buffer, uint32_t size) {
    elab_mpu6500_t *inst = (elab_mpu6500_t *)dev;
    uint8_t raw_data[14];
    
    // 安全校验：外部期望的是 6 个浮点数 (24字节)
    if (buffer == NULL || size < (sizeof(float) * 6)) {
        return 0; 
    }

    /* 1. 突发读取 14 字节 */
    gpio_bits_reset(inst->cs_port, inst->cs_pin);
    mpu_spi_swap_byte(inst->spi_x, MPU6500_ACCEL_XOUT_H | 0x80);
    for (int i = 0; i < 14; i++) {
        raw_data[i] = mpu_spi_swap_byte(inst->spi_x, 0xFF);
    }
    gpio_bits_set(inst->cs_port, inst->cs_pin);

    /* 2. 拼接原始数据 */
    int16_t raw_accel[3], raw_gyro[3];
    raw_accel[0] = (raw_data[0] << 8) | raw_data[1];
    raw_accel[1] = (raw_data[2] << 8) | raw_data[3];
    raw_accel[2] = (raw_data[4] << 8) | raw_data[5];
    raw_gyro[0]  = (raw_data[8] << 8)  | raw_data[9];
    raw_gyro[1]  = (raw_data[10] << 8) | raw_data[11];
    raw_gyro[2]  = (raw_data[12] << 8) | raw_data[13];

    /* 3. 转换为浮点物理量并填入用户的 buffer 中 */
    float *out_data = (float *)buffer;
    // 加速度 (假设量程 ±8g，灵敏度 4096 LSB/g)
    out_data[0] = (float)raw_accel[0] / 4096.0f;
    out_data[1] = (float)raw_accel[1] / 4096.0f;
    out_data[2] = (float)raw_accel[2] / 4096.0f;
    // 陀螺仪 (假设量程 ±1000 dps，灵敏度 32.8 LSB/dps)
    out_data[3] = (float)raw_gyro[0] / 32.8f;
    out_data[4] = (float)raw_gyro[1] / 32.8f;
    out_data[5] = (float)raw_gyro[2] / 32.8f;

    return size;
}

/* 操作接口表 */
static const elab_dev_ops_t mpu_ops = {
    .enable  = mpu6500_enable,
    .read  = mpu6500_read
    // .write = mpu6500_write, (可预留用于设置滤波器或量程)
};

/* ========================================================================= */
/* eLab 自动注册探针层                                                       */
/* ========================================================================= */

/* 核心注册函数 */
void elab_mpu6500_register(elab_mpu6500_t *inst, elab_device_attr_t *attr)
{
    /* 1. 初始化独立的 CS 引脚为推挽输出 */
    // 注意：如果是用 PA15，必须开启 IOMUX 并解除 JTAG 占用！
    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE); 
    
    if (inst->cs_port == GPIOA && inst->cs_pin == GPIO_PINS_15) {
        gpio_pin_remap_config(SWJTAG_GMUX_010, TRUE); // 禁用 JTAG，保留 SWD，释放 PA15
    }

    gpio_init_type gpio_init_struct;
    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
    gpio_init_struct.gpio_pins = inst->cs_pin;
    gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
    gpio_init(inst->cs_port, &gpio_init_struct);

    gpio_bits_set(inst->cs_port, inst->cs_pin); // 默认拉高，不选中从机

    /* 2. 探针 (Probe) 测试：读取 WHO_AM_I */
    inst->who_am_i = mpu_spi_read_reg(inst, MPU6500_WHO_AM_I_REG);
    
    if (inst->who_am_i != MPU6500_ID) {
        // eLab 探测失败处理 (可以对接你的 ELAB_LOG_E)
        return; 
    }

    /* 3. 唤醒传感器：MPU6500 上电默认睡眠，必须往电源管理寄存器写 0x01 (唤醒并使用自动选择最佳时钟) */
    mpu_spi_write_reg(inst, MPU6500_PWR_MGMT_1_REG, 0x01);

    /* 4. 探测成功，挂载操作接口并注册进框架 */
    inst->dev.ops = &mpu_ops;
    inst->dev.user_data = inst;
    
    // 如果你的框架里有 elab_device_register，在这里调用它：
    elab_device_register(&inst->dev, attr);
}
/* 统一的底层注册入口 */
void elab_mpu6500_core_register(elab_mpu6500_t *inst, elab_device_attr_t *attr) {
    /* 1. 绑定操作接口 */
    
    
    elab_mpu6500_register(inst, attr);
}