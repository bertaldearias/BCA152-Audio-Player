#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "iot_button.h"
#include "button_gpio.h"
#include "esp_log.h"

namespace app
{
    enum class eBtnEvent
    {
        L_PRESSED,
        L_RELEASED,
        M_CLICK,
        M_DCLICK,
        R_PRESSED,
        R_RELEASED
    };
}

namespace
{
    template <app::eBtnEvent>
    void button_event_handler(void *btn_handle, void *user_data);
}

namespace app
{
    class AppButton
    {
    private:
        QueueHandle_t m_event_queue = NULL;

        button_handle_t makeButton(int gpio)
        {
            button_config_t btn_cfg = {};
            button_gpio_config_t gpio_cfg = {};
            gpio_cfg.gpio_num = gpio;
            gpio_cfg.active_level = 0; // pressed = LOW (pull-up)
            button_handle_t handle = nullptr;
            iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &handle);
            return handle;
        }

    public:
        static constexpr int PIN_LEFT = 4;
        static constexpr int PIN_MIDDLE = 5;
        static constexpr int PIN_RIGHT = 6;

        void init(void)
        {
            m_event_queue = xQueueCreate(10, sizeof(app::eBtnEvent));

            button_handle_t left = makeButton(PIN_LEFT);
            button_handle_t mid = makeButton(PIN_MIDDLE);
            button_handle_t right = makeButton(PIN_RIGHT);

            iot_button_register_cb(left, BUTTON_PRESS_DOWN, nullptr,
                                   button_event_handler<app::eBtnEvent::L_PRESSED>, this);
            iot_button_register_cb(left, BUTTON_PRESS_UP, nullptr,
                                   button_event_handler<app::eBtnEvent::L_RELEASED>, this);
            iot_button_register_cb(right, BUTTON_PRESS_DOWN, nullptr,
                                   button_event_handler<app::eBtnEvent::R_PRESSED>, this);
            iot_button_register_cb(right, BUTTON_PRESS_UP, nullptr,
                                   button_event_handler<app::eBtnEvent::R_RELEASED>, this);
            iot_button_register_cb(mid, BUTTON_SINGLE_CLICK, nullptr,
                                   button_event_handler<app::eBtnEvent::M_CLICK>, this);
            iot_button_register_cb(mid, BUTTON_DOUBLE_CLICK, nullptr,
                                   button_event_handler<app::eBtnEvent::M_DCLICK>, this);
        }

        QueueHandle_t getEventQueue(void) const { return m_event_queue; }
    }; // class end
} // namespace end

namespace
{
    template <app::eBtnEvent E>
    void button_event_handler(void *btn_handle, void *user_data)
    {
        app::AppButton *app_btn = static_cast<app::AppButton *>(user_data);
        app::eBtnEvent evt{E};
        xQueueSend(app_btn->getEventQueue(), (void *)(&evt), 0);
    }
}