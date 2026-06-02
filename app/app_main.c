// app/app_main.c
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

/* ========================================================================= */
/* ⚡ 闪电指针缓存区 (专供 20kHz 中断使用)                                   */
/* ========================================================================= */
elab_foc_motor_t *g_motor_L = NULL;
elab_foc_motor_t *g_motor_R = NULL;

/* 注意这里类型是具体的 elab_tle5012b_t，而不是抽象的 elab_device_t */
elab_tle5012b_t *g_enc_L = NULL;
elab_tle5012b_t *g_enc_R = NULL;

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

    QTIMEEVT_TICK_X(0U, &l_clock_tick_sender); // time events at rate 0
#endif
}
