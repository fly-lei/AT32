cmake_minimum_required(VERSION 3.16)

# ==============================================================================
# 0. 跨平台双轨开关 & 工具链配置
# ==============================================================================
option(BUILD_SIMULATOR "Build for Linux PC Simulation (SIL)" OFF)

if(NOT BUILD_SIMULATOR)
    # 强制在 project() 之前加载交叉编译工具链 (请确保该文件存在)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/arm-gcc.cmake" CACHE FILEPATH "Toolchain file")
endif()

project(test C ASM)
add_compile_options(-g -O0)
# ==============================================================================
# 1. 目录路径宏定义 (方便后续引用)
# ==============================================================================
set(APP_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/app)
set(ELAB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/elab)
set(QPC_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/qpc)
# set(HAL_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/stm32cubemx)

# ======== 新增：动态推导 QP/C 的 Port 头文件路径 ========
if(BUILD_SIMULATOR)
    set(QPC_PORT_DIR ${QPC_DIR}/ports/posix)
else()
    set(QPC_PORT_DIR ${QPC_DIR}/ports/arm-cm/qv/gnu)
endif()
# =======================================================

# 包含 QP/C 核心源码 (平台无关部分)
set(QPC_CORE_SRC
    ${QPC_DIR}/src/qf/qep_hsm.c
    ${QPC_DIR}/src/qf/qep_msm.c
    ${QPC_DIR}/src/qf/qf_act.c
    ${QPC_DIR}/src/qf/qf_actq.c
    ${QPC_DIR}/src/qf/qf_defer.c
    ${QPC_DIR}/src/qf/qf_dyn.c
    ${QPC_DIR}/src/qf/qf_mem.c
    ${QPC_DIR}/src/qf/qf_ps.c
    ${QPC_DIR}/src/qf/qf_qact.c
    ${QPC_DIR}/src/qf/qf_qeq.c
    ${QPC_DIR}/src/qf/qf_qmact.c
    ${QPC_DIR}/src/qf/qf_time.c
      
)
if(NOT BUILD_SIMULATOR)
    list(APPEND QPC_CORE_SRC ${QPC_DIR}/src/qv/qv.c)
endif()
# ==============================================================================
# 2. 🧠 第一层：应用核心 (app_core) - 【叹息之墙内，绝对纯净】
# ==============================================================================
add_library(app_core STATIC 
    ${APP_DIR}/app_main.c 
    ${APP_DIR}/sm_blinky.c
    ${APP_DIR}/usb_app.c 
    ${QPC_CORE_SRC}
)

# 【架构铁律】: 这里只能暴露自己的头文件，以及抽象层的头文件。
# 绝对不要包含任何 stm32 或 HAL 相关的目录！
target_include_directories(app_core PUBLIC 
    ${APP_DIR}/include
    ${ELAB_DIR}/include
    ${ELAB_DIR}/common
    ${QPC_DIR}/include
    ${QPC_PORT_DIR}
)

# ==============================================================================
# 3. 🔌 第二层：eLab VFS 抽象层 (elab_vfs)
# ==============================================================================
add_library(elab_vfs STATIC 
    ${ELAB_DIR}/elab_device.c
    ${ELAB_DIR}/common/elab_assert.c
    ${ELAB_DIR}/common/elab_log.c
    ${ELAB_DIR}/common/elab_export.c
)

target_include_directories(elab_vfs PUBLIC 
    ${ELAB_DIR}/include
    ${ELAB_DIR}/common
    ${QPC_DIR}/include   # 👈 必须有这个，因为 elab_port.h 用了 qpc.h 里的临界区宏
    ${QPC_PORT_DIR}      # 👈 必须有这个
)


# ==============================================================================
# 4. 🔧 第三层：躯干与底层驱动 (bsp_hw) - 根据平台分叉
# ==============================================================================
if(BUILD_SIMULATOR)
    message(STATUS "🚗 当前构建目标: Linux PC 软件在环仿真 (SIL)")

    add_library(bsp_hw STATIC 
        ${ELAB_DIR}/bsp_linux/elab_led_linux.c
        ${ELAB_DIR}/bsp_linux/elab_port_linux.c   # ✨ 极其重要
        ${QPC_DIR}/ports/posix/qf_port.c
    )
    
    # Linux 平台特定的宏和链接库
    target_compile_definitions(bsp_hw PUBLIC -D_POSIX_C_SOURCE=200809L)
    target_link_libraries(bsp_hw PUBLIC app_core elab_vfs pthread)

else()
    message(STATUS "🚀 当前构建目标：[ AT32F403AVGT7 真实硬件 ]")
    
    # ✨ 替换宏定义：使用 AT32 的芯片宏和标准固件库宏
    add_compile_definitions(AT32F403AVGT7) 
    add_compile_definitions(USE_STDPERIPH_DRIVER)
    
    # ✨ 假设你将雅特力官方固件库放在了 atAT32F403AVGT7PHC
    set(AT32_BSP_DIR "AT32F403AVGT7PHC")

    include_directories(
        ${AT32_BSP_DIR}/project/inc                       # 存放 at32f403a_407_conf.h、at32f4xx_it.h 等
        ${AT32_BSP_DIR}/libraries/drivers/inc             # 固件库外设驱动头文件
        ${AT32_BSP_DIR}/libraries/cmsis/cm4/core_support  # ARM Cortex-M4 核心头文件 (AT32F403A是M4内核)
        ${AT32_BSP_DIR}/libraries/cmsis/cm4/device_support # 雅特力设备系统头文件
        ${AT32_BSP_DIR}/middlewares/usbd_class/cdc         # 雅特力设备usb
        ${AT32_BSP_DIR}/middlewares/usbd_drivers/inc         # 雅特力设备usb
    )
    
    # 搜集底层所有的 C 源码
    # 1. 你的业务底层封装 (这里假设你把文件放到了 bsp_at32 目录下)
    file(GLOB BSP_SOURCES "elab/bsp_at32/*.c")
    
    # 2. AT32 官方固件库驱动源码
    file(GLOB AT32_DRIVER_SOURCES "${AT32_BSP_DIR}/libraries/drivers/src/*.c")
    list(APPEND BSP_SOURCES ${AT32_DRIVER_SOURCES})
    
    # 3. 业务相关的系统源文件 (通常包含 main.c, system_at32f403a_407.c, at32f4xx_it.c)
    file(GLOB AT32_PROJECT_SOURCES "${AT32_BSP_DIR}/project/src/*.c") 
    list(APPEND BSP_SOURCES ${AT32_PROJECT_SOURCES})
    # ✨ AT32 USB Device 底层核心驱动 (解决了 undefined reference)
    file(GLOB AT32_USB_CORE_SOURCES "${AT32_BSP_DIR}/middlewares/usbd_drivers/src/*.c")
    list(APPEND BSP_SOURCES ${AT32_USB_CORE_SOURCES})

    # # ✨ AT32 USB Class 类驱动源码 (对应上面的 include)
    file(GLOB AT32_USB_CLASS_SOURCES "${AT32_BSP_DIR}/libraries/cmsis/cm4/device_support/*.c")
    list(APPEND BSP_SOURCES ${AT32_USB_CLASS_SOURCES})
    # (可选) 剔除可能干扰编译的冗余外设或模板文件
    # list(FILTER BSP_SOURCES EXCLUDE REGEX "at32f403a_407_board\\.c$") 

    # ✨ 补齐 QP/C 状态机编程框架在 ARM Cortex-M 下的底层驱动代码
    file(GLOB QPC_PORT_SRC "${QPC_PORT_DIR}/*.c")
    list(APPEND BSP_SOURCES ${QPC_PORT_SRC})

    # ✨✨ 致命修复：生成 bsp_hw 库！(保留 QV 协作式内核调度器)
    add_library(bsp_hw STATIC ${BSP_SOURCES}
                ${QPC_DIR}/src/qv/qv.c)
    
    # ✨ 补上这句：让 AT32 底层硬件库继承 eLab 和 QP/C 的头文件路径！
    target_link_libraries(bsp_hw PUBLIC app_core elab_vfs)

endif()

# ==============================================================================
# 5. 🎯 最终链接：生成可执行文件 (.elf)
# ==============================================================================
if(BUILD_SIMULATOR)
    add_executable(${PROJECT_NAME}.elf 
        ${ELAB_DIR}/bsp_linux/main_linux.c
    )
    
    target_link_libraries(${PROJECT_NAME}.elf PRIVATE 
        app_core 
        bsp_hw 
        elab_vfs 
        pthread
    )

else()
    # AT32 下，直接把汇编启动文件挂在最终程序上。
    # main.c 已经在第 4 步被打入 bsp_hw 库中，无需重复添加。
    # ✨ 替换：使用 AT32F403A 的 GCC 启动文件 (根据你的实际目录调整路径)
    add_executable(${PROJECT_NAME}.elf 
        ${AT32_BSP_DIR}/libraries/cmsis/cm4/device_support/startup/gcc/startup_at32f403a_407.s
    )
    
    # ✨✨ 核心修复：强行镇压 CMake，指明必须用 C 语言链接器！
    set_target_properties(${PROJECT_NAME}.elf PROPERTIES LINKER_LANGUAGE C)
    
    target_link_libraries(${PROJECT_NAME}.elf PRIVATE 
        -Wl,--start-group
        app_core 
        -Wl,--whole-archive bsp_hw -Wl,--no-whole-archive
        bsp_hw 
        elab_vfs
        -Wl,--end-group
    )

    # 编译完成后自动生成 .bin 和 .hex 烧录文件
    add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
        COMMENT "✅ 已生成烧录文件: ${PROJECT_NAME}.hex 和 ${PROJECT_NAME}.bin"
    )

    # ✨ 增加一个 "make flash" 目标 (针对 ST-Link V2 和 AT32F403A)
    add_custom_target(flash
        COMMAND openocd -f interface/stlink.cfg -f target/at32f403a.cfg -c "program $<TARGET_FILE:${PROJECT_NAME}.elf> verify reset exit"
        DEPENDS ${PROJECT_NAME}.elf
        COMMENT "⚡ 正在通过 ST-Link V2 烧录到 AT32F403A..."
    )
endif()

