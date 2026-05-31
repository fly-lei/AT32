// elab/bsp_linux/main_linux.c
#include <stdio.h>
#include <stdlib.h>
#include "qpc.h"
#include "elab.h"

extern void App_System_Start(void);

int main(int argc, char *argv[]) 
{
    printf("[SIL] 跨平台仿真环境启动...\n");

    /* 1. 虚拟外设抽象化 (eLab 接管) */
    // 触发 elab_led_linux.c 中的终端打印设备注册
    eLab_InitAll(); 

    /* 2. 初始化 QSPY 的 TCP 端口 (专属于 Linux 的调试网络) */
#ifdef Q_SPY
    // 允许通过命令行输入端口，例如 ./BalanceCar.elf -c 127.0.0.1:7701
    if (!QS_INIT(argc > 1 ? argv[1] : NULL)) {
        printf("[错误] QSPY 初始化失败，请检查端口是否被占用。\n");
        return -1;
    }
    // 设置过滤，追踪所有状态跃迁
    QS_GLB_FILTER(QS_ALL_RECORDS);
#endif

    /* 3. 业务层大脑初始化 (与 STM32 共用同一份纯净代码！) */
    App_System_Start();

    /* 4. 将控制权移交给 Linux POSIX 线程版 QP/C 内核 */
    printf("[SIL] QP/C 状态机引擎已接管，按 Ctrl+C 退出。\n");
    return QF_run(); 
}