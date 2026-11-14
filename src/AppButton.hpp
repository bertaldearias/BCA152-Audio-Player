#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "button.h"
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
    void button_event_handler(void *param);
}

namespace app
{
    class AppButton
    {
    private:
        QueueHandle_t m_event_queue = NULL;

        button_handle_t makeButton(int gpio)
        {
            button_config_t cfg = {};
            cfg.type = BUTTON_TYPE_GPIO;
            cfg.gpio_button_config.gpio_num = gpio;
            cfg.gpio_button_config.active_level = 0; // pressed = LOW
            button_handle_t h = button_create(&cfg);
            gpio_set_pull_mode((gpio_num_t)gpio, GPIO_PULLUP_ONLY); // keep pin HIGH when not pressed
            return h;
        }

        void attach(button_handle_t btn)
        {
            reinterpret_cast<button_dev_t *>(btn)->cb_user_data = this;
        }

    public:
        static constexpr int PIN_LEFT = 32;
        static constexpr int PIN_MIDDLE = 33;
        static constexpr int PIN_RIGHT = 27;

        void init(void)
        {
            m_event_queue = xQueueCreate(10, sizeof(app::eBtnEvent));

            button_handle_t left = makeButton(PIN_LEFT);
            button_handle_t mid = makeButton(PIN_MIDDLE);
            button_handle_t right = makeButton(PIN_RIGHT);
            attach(left);
            attach(mid);
            attach(right);

            button_register_cb(left, BUTTON_PRESS_DOWN, button_event_handler<app::eBtnEvent::L_PRESSED>);
            button_register_cb(left, BUTTON_PRESS_UP, button_event_handler<app::eBtnEvent::L_RELEASED>);
            button_register_cb(right, BUTTON_PRESS_DOWN, button_event_handler<app::eBtnEvent::R_PRESSED>);
            button_register_cb(right, BUTTON_PRESS_UP, button_event_handler<app::eBtnEvent::R_RELEASED>);
            button_register_cb(mid, BUTTON_SINGLE_CLICK, button_event_handler<app::eBtnEvent::M_CLICK>);
            button_register_cb(mid, BUTTON_DOUBLE_CLICK, button_event_handler<app::eBtnEvent::M_DCLICK>);
        }

        static AppButton &getObject(void *btn_ptr)
        {
            button_dev_t *btn_dev = reinterpret_cast<button_dev_t *>(btn_ptr);
            return *(reinterpret_cast<app::AppButton *>(btn_dev->cb_user_data));
        }

        QueueHandle_t getEventQueue(void) const { return m_event_queue; }
    }; // class end
} // namespace end

namespace
{
    template <app::eBtnEvent E>
    void button_event_handler(void *btn_ptr)
    {
        app::AppButton &app_btn = app::AppButton::getObject(btn_ptr);
        app::eBtnEvent evt{E};
        xQueueSend(app_btn.getEventQueue(), (void *)(&evt), 0);
    }
}