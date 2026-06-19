// app/app_main.c
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "qpc.h"
#include "app_events.h" // 你的全局事件定义
#include "elab_port.h"
#if !defined(__linux__)
// #include "main.h"
#endif
// 1. 定义 QP/C 需要的底层资源 (仅在此文件可见)
static QSubscrList l_subscrSto[MAX_PUB_SIG]; // 发布-订阅事件的记录表
static QF_MPOOL_EL(QEvt) l_smlPoolSto[128];  // 小事件的内存池 (容量128)
static QEvt const *blinky_queue[10];
extern void led_hw_init(void);
extern void Blinky_ctor(void);
extern QActive *const AO_Blinky;
#include "elab_foc_motor.h"
#include "elab_tle5012b.h"
#include "elab_power_key.h"
#include "elab_usart1.h"
#include "elab_protocol.h"
/* ========================================================================= */
/* ⚡ 闪电指针缓存区 (专供 20kHz 中断使用)                                   */
/* ========================================================================= */
elab_foc_motor_t *g_motor_L = NULL;
elab_foc_motor_t *g_motor_R = NULL;

/* 注意这里类型是具体的 elab_tle5012b_t，而不是抽象的 elab_device_t */
elab_tle5012b_t *g_enc_L = NULL;
elab_tle5012b_t *g_enc_R = NULL;
/* 定义你的全局控制变量，供 FOC 电机或状态机使用 */
// 假设我们有全局变量供状态机或电机使用

// uint8_t g_system_enable = 0;
int16_t g_remote_speed = 0;  // 目标线速度
int16_t g_remote_turn = 0;   // 目标转向度
bool g_car_light_on = false; // 车灯状态
bool g_car_horn_on = false;  // 喇叭状态
/* ------------------------------------------------------------------
 * 步骤 1：定义回调函数 (充当从中断层到应用层的桥梁)
 * 🚨 警告：此函数在串口中断内执行，绝对不能有 delay，处理越快越好！
 * ------------------------------------------------------------------*/
static void On_ESP32_Data_Received(uint8_t *data, uint16_t len)
{
    /* 方案 A：直接在这里调用你的二进制协议解析函数 */
    Protocol_Parse_App_String(data, len);

    /* 方案 B（更推荐的高级 RTOS 玩法）：
       在这里把 data 拷贝到一个全局/消息池，然后向你的主状态机 Post 一个事件
       QACTIVE_POST((QActive *)AO_MainApp, (QEvt *)&esp32_rx_evt, 0U);
    */
}

/**
 * @brief  解析来自手机 APP 的字符串指令
 * @param  data: DMA 接收到的原始数据指针
 * @param  len:  本次实际接收到的字节数 (极度重要，用来切断幽灵数据)
 */

void Protocol_Parse_App_String(uint8_t *data, uint16_t len)
{
    /* 1. 防呆保护 */
    if (data == NULL || len == 0)
        return;

    /* 2. 建立局部安全缓冲区，过滤掉后面的“幽灵残留数据” */
    char cmd_str[32] = {0};
    uint16_t copy_len = (len < sizeof(cmd_str)) ? len : (sizeof(cmd_str) - 1);

    // 只拷贝本次真正收到的长度
    memcpy(cmd_str, data, copy_len);
    // 强制加上字符串结束符 '\0'，把后面的乱码彻底物理隔绝！
    cmd_str[copy_len] = '\0';

    /* 3. 剔除末尾的换行符 '\n' 或 '\r'，让字符串变得干净，方便用 strcmp 对比 */
    for (int i = 0; i < copy_len; i++)
    {
        if (cmd_str[i] == '\n' || cmd_str[i] == '\r')
        {
            cmd_str[i] = '\0';
            break;
        }
    }

    /* ========================================================= */
    /* 4. 核心路由：命令比对与动作分发                           */
    /* ========================================================= */
    /* 解析逻辑改造 */
    if (strncmp(cmd_str, "Speed_", 6) == 0)
    {
        MotorEvt *e = Q_NEW(MotorEvt, MOTOR_CTRL_SIG);
        e->speed = atoi(&cmd_str[6]);
        e->turn = g_remote_turn; // 使用上一次保存的状态
        QACTIVE_POST(AO_Blinky, (QEvt *)e, 0U);
    }

    else if (strcmp(cmd_str, "UP") == 0)
    {
        MotorEvt *e = Q_NEW(MotorEvt, MOTOR_CTRL_SIG);
        e->speed = 100;
        e->turn = 0;
        QACTIVE_POST(AO_Blinky, (QEvt *)e, 0U);
    }
    else if (strcmp(cmd_str, "LIGHT") == 0)
    {
        SystemEvt *e = Q_NEW(SystemEvt, SYSTEM_CMD_SIG);
        e->cmd = CMD_LIGHT_TOGGLE;
        QACTIVE_POST(AO_Blinky, (QEvt *)e, 0U);
    }
    else if (strcmp(cmd_str, "DOWN") == 0)
    {
        MotorEvt *e = Q_NEW(MotorEvt, MOTOR_CTRL_SIG);
        e->speed = -100;
        e->turn = 0;
        QACTIVE_POST(AO_Blinky, (QEvt *)e, 0U);
    }
    else if (strcmp(cmd_str, "LEFT") == 0)
    {
        MotorEvt *e = Q_NEW(MotorEvt, MOTOR_CTRL_SIG);
        e->speed = 0;
        e->turn = -50;
        QACTIVE_POST(AO_Blinky, (QEvt *)e, 0U);
    }
    else if (strcmp(cmd_str, "RIGHT") == 0) // 盲猜你的APP有RIGHT
    {
        MotorEvt *e = Q_NEW(MotorEvt, MOTOR_CTRL_SIG);
        e->speed = 0;
        e->turn = 50;
        QACTIVE_POST(AO_Blinky, (QEvt *)e, 0U);
    }

    else if (strcmp(cmd_str, "HORN") == 0)
    {
        SystemEvt *e = Q_NEW(SystemEvt, SYSTEM_CMD_SIG);
        e->cmd = CMD_HORN_ON;
        QACTIVE_POST(AO_Blinky, (QEvt *)e, 0U);
    }

    /* D. 解析自定义按键 A/B/C/D */
    else if (strcmp(cmd_str, "A") == 0)
    {
        // 比如：切换底盘模式 (阿克曼模式 -> 麦克纳姆轮模式)
    }
    else if (strcmp(cmd_str, "B") == 0)
    {
        // 比如：紧急刹车停机
        g_remote_speed = 0;
        g_remote_turn = 0;
    }
    // else if (strcmp(cmd_str, "C") == 0) ...
}

/* 顶层注册的回调函数：负责将底层事件转化为状态机事件 */
static void On_System_Key_Event(elab_key_event_t evt)
{

    static QEvt const short_press_evt = QEVT_INITIALIZER(SHORT_PRESS_SIG);
    static QEvt const long_press_evt = QEVT_INITIALIZER(LONG_PRESS_SIG);

    switch (evt)
    {
    case ELAB_KEY_EVT_SHORT_PRESS:
        /* 邮寄短按事件给主状态机 */
        QACTIVE_POST((QActive *)AO_Blinky, &short_press_evt, 0U);
        break;

    case ELAB_KEY_EVT_LONG_PRESS:
        /* 邮寄长按事件给主状态机 */
        QACTIVE_POST((QActive *)AO_Blinky, &long_press_evt, 0U);
        break;

    default:
        break;
    }
}
void System_Link_Devices(void)
{
    /* 在进入主循环(QF_run)前，把所有对象找齐 */
    g_motor_L = (elab_foc_motor_t *)elab_device_find("motor_left");
    g_motor_R = (elab_foc_motor_t *)elab_device_find("motor_right");

    g_enc_L = (elab_tle5012b_t *)elab_device_find("enc_left");
    g_enc_R = (elab_tle5012b_t *)elab_device_find("enc_right");

    /* 防御性编程：如果有没找到的，直接断言报错，绝不带病运行 */
    elab_assert(g_motor_L && g_motor_R && g_enc_L && g_enc_R);
}
// 2. 跨平台的统一启动总闸
void App_System_Start(void)
{
    elab_power_key_init(On_System_Key_Event);
    elab_usart1_init(115200, On_ESP32_Data_Received);
    System_Link_Devices(); /* 连接设备（必须在 QF_run 之前） */
    // 第一步：初始化 QP 框架的基础设施
    QF_init();
    QF_psInit(l_subscrSto, Q_DIM(l_subscrSto));
    QF_poolInit(l_smlPoolSto, sizeof(l_smlPoolSto), sizeof(l_smlPoolSto[0]));

    // 第二步：为 QSPY 追踪配置字典（为了在电脑上看到状态名，而不是冷冰冰的数字）
#ifdef Q_SPY
    // QS_OBJ_DICTIONARY(...);
    // QS_FUN_DICTIONARY(...);
#endif
#if defined(__linux__)
    led_hw_init();
#endif
    // 第三步：✨ 构造并真正启动我们的 LED 状态机！
    Blinky_ctor(); /* 构造（必须在 start 之前） */

    QActive_start_(AO_Blinky,
                   1U,
                   blinky_queue,
                   sizeof(blinky_queue) / sizeof(blinky_queue[0]),
                   (void *)0, 0U,
                   (QEvt *)0);

    // extern QActive * const AO_BalanceCar;
    // QActive_start(AO_BalanceCar, 2, ..., 0, 0);

    // 注意：这里绝对不要调用 QF_run()！
    // 因为 QF_run() 是一个死循环，一旦调用就不会返回了。
}

/* ================== 系统强制回调 (BSP) ================== */

#if defined(__linux__)
#include <sys/select.h>
#endif

void QF_onStartup(void)
{
#if defined(__linux__)
    /*
     * 官方正统做法：
     * 参数 0U 表示禁用标准的 POSIX timer（改为我们在 QF_onClockTick 里自己阻塞）
     * 参数 10U 表示系统分配给后台 Ticker 线程的优先级
     */
    QF_setTickRate(0U, 10U);
#else
    /* STM32 真实硬件配置：配置 1ms 硬件中断 */
    QF_crit_exit_();
    //  __enable_irq();
    // SysTick_Config(SystemCoreClock / 1000);
#endif
}

void QF_onCleanup(void)
{
    // 系统退出时的清理
}
static uint8_t const l_clock_tick_sender = 0;
void QF_onClockTick(void)
{
#if defined(__linux__)
    /*
     * 官方正统做法：在内部的 Ticker 线程中阻塞 1ms。
     * 使用 select 是 POSIX 环境下最标准的微秒级延时方式。
     */
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 1000; /* 1000 微秒 = 1 毫秒 (即 1 Tick = 1ms) */
    select(0, NULL, NULL, NULL, &tv);
    /* 🔴 探针 3：每秒打印一次，证明后台心跳没死 */
    static int tick_cnt = 0;
    if (++tick_cnt % 1000 == 0)
    {
        printf(">>> DEBUG [TICK]: Clock Tick is running... (1000 ticks)\n");
    }
    /* 驱动 QP 状态机时间事件 */
    QTimeEvt_tick_(0U, &l_clock_tick_sender);
#else
    static uint8_t div_cnt = 0;

    /* 假设 SysTick 基础心跳是 1ms */
    div_cnt++;
    if (div_cnt >= 20)
    {
        div_cnt = 0;

        /* 🚀 驱动按键状态机运转 */
        elab_power_key_tick_isr();
    }
    QTIMEEVT_TICK_X(0U, &l_clock_tick_sender); // time events at rate 0
#endif
}
