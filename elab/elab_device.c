/*
 * eLab Project
 * Copyright (c) 2026, EventOS Team, <event-os@outlook.com>
 * VFS (Virtual File System) Core Implementation
 */

/* 1. 极其重要：必须在所有 #include 之前定义本文件的 TAG！ */
#define ELAB_TAG "EdfDevice"

/* 2. 引入标准库与 eLab 抽象层 */
#include <string.h>
#include <stddef.h>
#include "elab_config.h"
#include "elab_port.h"
#include "elab_device.h"
#define INT8_MIN (-128)
static int8_t export_level_max = INT8_MIN;
/* 为了防止上层 elab.h 缺失导致编译失败，我们在此处显式声明函数指针类型 */
typedef void (*elab_init_fn_t)(void);

/* private function prototypes ---------------------------------------------- */
static void _add_device(elab_device_t *me);

/* private variables -------------------------------------------------------- */
static uint32_t _edf_device_count = 0;
static elab_device_t *_edf_table[ELAB_DEV_NUM_MAX];
static uint32_t count_export_init = 0;
static elab_export_t *export_init_table = NULL;
/* 
 * 巧妙的跨平台线程/上下文获取映射
 */
static inline void* elab_thread_get_id(void) {
#if defined(__linux__)
    #include <pthread.h>
    return (void*)pthread_self();
#else
    #include "qpc.h"
    return (void*)QActive_current(); 
#endif
}

/* public function ---------------------------------------------------------- */
void elab_device_register(elab_device_t *me, elab_device_attr_t *attr)
{
    elab_assert(me != NULL);
    elab_assert(attr != NULL);
    elab_assert(attr->name != NULL);
    elab_assert(elab_device_find(attr->name) == NULL);

    ELAB_CRITICAL_ENTER();

    /* Set the device data. */
    memcpy(&me->attr, attr, sizeof(elab_device_attr_t));
    me->enable_count = 0;
    me->lock_count = 0;
    // 彻底切除旧的 OS 锁

    /* Add the device to the edf table. */
    _add_device(me);

    ELAB_CRITICAL_EXIT();
}

void elab_device_unregister(elab_device_t *me)
{
    elab_assert(me != NULL);
    elab_assert(!elab_device_is_enabled(me));

    ELAB_CRITICAL_ENTER();

    for (uint32_t i = 0; i < ELAB_DEV_NUM_MAX; i ++)
    {
        if (_edf_table[i] == me)
        {
            _edf_table[i] = NULL;
            _edf_device_count --;
            break;
        }
    }

    ELAB_CRITICAL_EXIT();
}

uint32_t elab_device_get_number(void)
{
    uint32_t num = 0;
    ELAB_CRITICAL_ENTER();
    num = _edf_device_count;
    ELAB_CRITICAL_EXIT();
    return num;
}

bool elab_device_of_name(elab_device_t *me, const char *name)
{
    bool of_the_name = false;
    __device_mutex_lock(me, true);
    if (strcmp(me->attr.name, name) == 0)
    {
        of_the_name = true;
    }
    __device_mutex_lock(me, false);
    return of_the_name;
}

elab_device_t *elab_device_find(const char *name)
{
    elab_assert(name != NULL);
    
    elab_device_t *me = NULL;

    ELAB_CRITICAL_ENTER();
    for (uint32_t i = 0; i < ELAB_DEV_NUM_MAX; i++)
    {
        if (_edf_table[i] == NULL)
        {
            break;
        }
        elab_assert(_edf_table[i]->attr.name != NULL);
        if (strcmp(_edf_table[i]->attr.name, name) == 0)
        {
            me = _edf_table[i];
            break;
        }
    }
    ELAB_CRITICAL_EXIT();

    return me;
}

bool elab_device_valid(const char *name)
{
    return elab_device_find(name) == NULL ? false : true;
}

bool elab_device_is_sole(elab_device_t *me)
{
    __device_mutex_lock(me, true);
    bool enable_status = me->attr.sole;
    __device_mutex_lock(me, false);
    return enable_status;
}

bool elab_device_is_test_mode(elab_device_t *dev)
{
    return (dev->thread_test != NULL) ? true : false;
}

void elab_device_set_test_mode(elab_device_t *dev)
{
    elab_assert(dev != NULL);
    __device_mutex_lock(dev, true);
    dev->thread_test = elab_thread_get_id();
    __device_mutex_lock(dev, false);
}

void elab_device_set_normal_mode(elab_device_t *dev)
{
    elab_assert(dev != NULL);
    __device_mutex_lock(dev, true);
    dev->thread_test = NULL;
    __device_mutex_lock(dev, false);
}

bool elab_device_is_enabled(elab_device_t *me)
{
    elab_assert(me != NULL);
    __device_mutex_lock(me, true);
    bool enable_status = me->enable_count > 0 ? true : false;
    __device_mutex_lock(me, false);
    return enable_status;
}

/* 用极速临界区代替耗时的 OS Mutex */
void __device_mutex_lock(elab_device_t *me, bool status)
{
    elab_assert(me != NULL);
    if (status)
    {
        ELAB_CRITICAL_ENTER();
    }
    else
    {
        ELAB_CRITICAL_EXIT();
    }
}

elab_err_t __device_enable(elab_device_t *me, bool status)
{
    elab_assert(me != NULL);
    elab_assert(me->ops != NULL);
    elab_assert(me->ops->enable != NULL);

    __device_mutex_lock(me, true);
    
    if (me->attr.sole)
    {
        if (status) { elab_assert(me->enable_count == 0); }
        else        { elab_assert(me->enable_count > 0);  }
    }
    else
    {
        elab_assert(me->enable_count < UINT8_MAX);
    }
    
    elab_err_t ret = ELAB_OK;
    if (status && me->enable_count == 0)
    {
        ret = me->ops->enable(me, true);
    }
    else if (!status && me->enable_count == 1)
    {
        ret = me->ops->enable(me, false);
    }
    me->enable_count = status ? (me->enable_count + 1) : (me->enable_count - 1);

    __device_mutex_lock(me, false);
    return ret;
}

int32_t elab_device_read(elab_device_t *me, uint32_t pos, void *buffer, uint32_t size)
{
    // elab_assert(me != NULL);
    // elab_assert(me->enable_count != 0);
    // elab_assert(me->ops != NULL);
    // elab_assert(me->ops->read != NULL);

    if (elab_device_is_test_mode(me)) { return ELAB_OK; }
    return me->ops->read(me, pos, buffer, size);
}

int32_t elab_device_write(elab_device_t *me, uint32_t pos, const void *buffer, uint32_t size)
{
    elab_assert(me != NULL);
    elab_assert(me->enable_count != 0);
    elab_assert(me->ops != NULL);
    elab_assert(me->ops->write != NULL);

    if (elab_device_is_test_mode(me)) { return ELAB_OK; }
    return me->ops->write(me, pos, buffer, size);
}

/* private functions -------------------------------------------------------- */
static void _add_device(elab_device_t *me)
{
    elab_assert(_edf_device_count < ELAB_DEV_NUM_MAX);

    if (_edf_device_count == 0)
    {
        for (uint32_t i = 0; i < ELAB_DEV_NUM_MAX; i ++)
        {
            _edf_table[i] = NULL;
        }
    }
    _edf_table[_edf_device_count ++] = me;
}

/* ========================================================================== */
/* GCC 链接器自动初始化引擎 (Linker Auto-Init Engine)                         */
/* ========================================================================== */

/* 
 * 引入链接脚本中强行预留的起止地址符号。
 * 使用 __attribute__((weak)) 允许在没有对应 ld 脚本的 Linux 仿真环境下也能安全编译通过。
 */
// extern const elab_export_t _elab_init_start __attribute__((weak));
// extern const elab_export_t _elab_init_end __attribute__((weak));
extern const uint32_t _elab_init_start __attribute__((weak));
extern const uint32_t _elab_init_end __attribute__((weak));
/**
  * @brief  eLab null exporting function.
  * @retval None
  */
static void module_null_init(void)
{
    /* NULL */
}
ELAB_INIT_EXPORT(module_null_init, 0);


/**
  * @brief  Get the init export table using Linker Script boundaries.
  */
static void _get_init_export_table(void)
{
    /* 1. 直接从链接器拿绝对起始地址，精准无误！ */
    export_init_table = (elab_export_t *)&_elab_init_start;
    count_export_init = ((elab_export_t *)&_elab_init_end) - ((elab_export_t *)&_elab_init_start);

    // export_init_table = (elab_export_t *)&_elab_init_start;

    // /* 2. 直接通过指针相减，算出表中共有多少个导出项 (不需要 while 循环去猜！) */
    // count_export_init = ((elab_export_t *)&_elab_init_end) - ((elab_export_t *)&_elab_init_start);

    /* 3. 仅需一次安全遍历，找出最大等级 export_level_max */
    export_level_max = 0;
    
    for (uint32_t i = 0; i < count_export_init; i++)
    {
        // 可选：为了绝对安全，保留你的 magic 校验
        if (export_init_table[i].magic_head == EXPORT_ID_INIT &&
            export_init_table[i].magic_tail == EXPORT_ID_INIT)
        {
            if (export_init_table[i].level > export_level_max)
            {
                export_level_max = export_init_table[i].level;
            }
        }
    }
}

/**
  * @brief  eLab init exporting function executing.
  * @retval None
  */
static void _init_func_execute(int8_t level)
{
    /* Execute the poll function in the specific level. */
    for (uint32_t i = 0; i < count_export_init; i ++)
    {
        if (export_init_table[i].level == level)
        {
            if (!export_init_table[i].exit)
            {
                if (level != EXPORT_UNIT_TEST)
                {
                    // printf("Export init %s." STR_ENTER, export_init_table[i].name);
                }
                ((void (*)(void))export_init_table[i].func)();
            }
        }
    }
}

void eLab_InitAll(void) 
{
    #if defined(__linux__)
    ELAB_LOG_I("设备自动初始化引擎启动...");
     ELAB_LOG_I("SIL 仿真环境：自动注册已跳过，请在 main 中手动初始化虚拟设备。");
    #else
     _get_init_export_table();
        for (uint8_t level = 0; level <= export_level_max; level ++)
    {
        _init_func_execute(level);
    }
    #endif

}
