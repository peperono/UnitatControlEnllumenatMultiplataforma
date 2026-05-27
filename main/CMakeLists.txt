set(QPCPP "${CMAKE_CURRENT_SOURCE_DIR}/../qpcpp")
set(ROOT   "${CMAKE_CURRENT_SOURCE_DIR}/..")

# Genera web/index_html.h si no existeix (la primera vegada cal haver executat build.sh
# o executar manualment: bash web/assemble.sh && bash web/gen_html_header.sh)
if(NOT EXISTS "${ROOT}/web/index_html.h")
    message(STATUS "Generant web/index_html.h...")
    execute_process(COMMAND bash "${ROOT}/web/assemble.sh"        WORKING_DIRECTORY "${ROOT}")
    execute_process(COMMAND bash "${ROOT}/web/gen_html_header.sh" WORKING_DIRECTORY "${ROOT}")
endif()

idf_component_register(
    SRCS
        "main_esp32.cpp"
        "${ROOT}/ControlEntrades/ControlEntrades.cpp"
        "${ROOT}/MonitorEntrades/MonitorEntrades.cpp"
        "${ROOT}/ActuadorSortides/ActuadorSortides.cpp"
        "${ROOT}/HttpServer/HttpServer.cpp"
        "${ROOT}/ControlRemot/ControlRemot.cpp"
        "${ROOT}/ControlHorari/ControlHorari.cpp"
        "${ROOT}/Rellotge/Rellotge.cpp"
        "${ROOT}/Blink/Blink.cpp"
        "${ROOT}/mongoose/mongoose.c"
        "${QPCPP}/src/qf/qep_hsm.cpp"
        "${QPCPP}/src/qf/qep_msm.cpp"
        "${QPCPP}/src/qf/qf_act.cpp"
        # qf_actq.cpp NO s'inclou: qf_port.cpp ja reimplementa postx_, postLIFO,
        # get_, getQueueUse/Free/Min per a FreeRTOS (usa QueueHandle_t, no QEQueue).
        "${QPCPP}/src/qf/qf_defer.cpp"
        "${QPCPP}/src/qf/qf_dyn.cpp"
        "${QPCPP}/src/qf/qf_mem.cpp"
        "${QPCPP}/src/qf/qf_ps.cpp"
        "${QPCPP}/src/qf/qf_qact.cpp"
        "${QPCPP}/src/qf/qf_qeq.cpp"
        "${QPCPP}/src/qf/qf_qmact.cpp"
        "${QPCPP}/src/qf/qf_time.cpp"
        "${QPCPP}/ports/freertos/qf_port.cpp"
    INCLUDE_DIRS
        "."
        "${ROOT}"
        "${QPCPP}/include"
        "${QPCPP}/src"
        "${QPCPP}/ports/freertos"
        "$ENV{IDF_PATH}/components/freertos/FreeRTOS-Kernel/include/freertos"
        "$ENV{IDF_PATH}/components/freertos/FreeRTOS-Kernel/portable/xtensa/include"
        "$ENV{IDF_PATH}/components/freertos/esp_additions/include"
    REQUIRES
        esp_wifi
        esp_timer
        nvs_flash
        esp_event
        esp_netif
        app_update
        pthread
        freertos
        esp_driver_gpio
)

# MG_ARCH_ESP32 = 3 (veure mongoose.h)
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    MG_ARCH=3
    MG_ENABLE_POSIX_FS=0
)
