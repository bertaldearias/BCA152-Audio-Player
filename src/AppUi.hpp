#pragma once
#include <mutex>
#include <string>
#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "Oled.hpp"
#include "AppButton.hpp"
#include "AppNav.hpp"
#include "AppAudio.hpp"

namespace app
{
    class AppUi
    {
    private:
        // Red LED: ON when the volume control is active, OFF when the playlist is active
        static constexpr gpio_num_t PIN_RED_LED = GPIO_NUM_18;

        inline static std::mutex m_ui_access;

        Oled m_oled;
        AppNav m_nav;
        bool m_playlist_active = true;
        bool m_playing = false;
        bool m_pressL = false;
        bool m_pressR = false;
        app::AppAudio *m_app_audio = nullptr;
        app::AppButton *m_app_button = nullptr;

        static void buttonEventTask(void *param)
        {
            AppUi &ui = *(reinterpret_cast<AppUi *>(param));
            app::eBtnEvent evt;
            while (true)
            {
                xQueueReceive(ui.getButtonEventQueue(), &evt, portMAX_DELAY);
                ui.handleButtonEvent(evt);
            }
        }

        static void audioEventTask(void *param)
        {
            AppUi &ui = *(reinterpret_cast<AppUi *>(param));
            app::eAudioEvent evt;
            while (true)
            {
                xQueueReceive(ui.getAudioEventQueue(), &evt, portMAX_DELAY);
                ui.handleAudioEvent(evt);
            }
        }

        static std::string makeAudioPath(const std::string &filename)
        {
            return std::string("/spiffs/") + filename;
        }

        // small key with a symbol; inverted while pressed
        void drawKey(int x, int y, int w, int h, char c, bool pressed)
        {
            m_oled.rect(x, y, w, h, pressed, true);
            m_oled.ch(x + (w - 5) / 2, y + (h - 7) / 2, c, 1, !pressed);
        }

        // redraws the whole screen (caller holds m_ui_access)
        void render()
        {
            m_oled.clear();

            // red LED shows which control is active (on = volume)
            gpio_set_level(PIN_RED_LED, m_playlist_active ? 0 : 1);

            // --- volume row (top) ---
            uint8_t vol = m_app_audio->getVolume();
            m_oled.rect(17, 3, 94, 8);
            m_oled.rect(19, 5, (vol * 90) / 100, 4, true);
            drawKey(2, 2, 11, 10, '-', m_pressL && !m_playlist_active);
            drawKey(115, 2, 11, 10, '+', m_pressR && !m_playlist_active);
            if (!m_playlist_active) m_oled.rect(0, 0, 128, 14); // focus border

            // --- animal name (middle) ---
            const char *name = m_nav.getCurrent().animal.c_str();
            m_oled.textCentered(64, 22, name, 2);

            char st[16];
            if (m_app_audio->isMuted()) snprintf(st, sizeof(st), "Muted");
            else                        snprintf(st, sizeof(st), "Vol %d%%", vol);
            m_oled.textCentered(64, 40, st, 1);

            // --- playlist row (bottom) ---
            drawKey(2, 52, 11, 10, '<', m_pressL && m_playlist_active);
            drawKey(115, 52, 11, 10, '>', m_pressR && m_playlist_active);
            m_oled.rect(30, 52, 68, 10);
            m_oled.textCentered(64, 53, m_playing ? "Pause" : "Play", 1);
            if (m_playlist_active) m_oled.rect(0, 50, 128, 14); // focus border

            m_oled.flush();
        }

        void update(const Animal_t &an, bool play = true)
        {
            (void)an;
            (void)play;
            m_app_audio->stop(); // a new animal starts from the beginning
            m_playing = false;
            Settings::saveU8("idx", (uint8_t)m_nav.index()); // remember the animal
            render();
        }

        void toggleControl(void)
        {
            m_playlist_active = !m_playlist_active;
            render();
        }

    public:
        void init(app::AppButton *button, app::AppAudio *audio)
        {
            m_app_button = button;
            m_app_audio = audio;

            gpio_reset_pin(PIN_RED_LED);
            gpio_set_direction(PIN_RED_LED, GPIO_MODE_OUTPUT);
            gpio_set_level(PIN_RED_LED, 0);

            m_oled.init();
            m_nav.init();
            m_playlist_active = true;
            {
                std::lock_guard<std::mutex> lock(m_ui_access);
                render();
            }

            xTaskCreate(buttonEventTask, "btn_evt", 4 * 1024, this, 3, nullptr);
            xTaskCreate(audioEventTask, "audio_evt", 4 * 1024, this, 3, nullptr);
        }

        QueueHandle_t getButtonEventQueue(void) const { return m_app_button->getEventQueue(); }
        QueueHandle_t getAudioEventQueue(void) const { return m_app_audio->getEventQueue(); }

        void handleButtonEvent(app::eBtnEvent &btn_evt)
        {
            std::lock_guard<std::mutex> lock(m_ui_access);
            switch (btn_evt)
            {
            case app::eBtnEvent::M_DCLICK:
                toggleControl();
                return;

            case app::eBtnEvent::M_CLICK:
                if (m_playlist_active)
                    m_app_audio->togglePlay(makeAudioPath(m_nav.getCurrent().audio));
                else
                    m_app_audio->mute(false, true);
                break;

            case app::eBtnEvent::L_PRESSED:
                m_pressL = true;
                break;

            case app::eBtnEvent::L_RELEASED:
                m_pressL = false;
                if (m_playlist_active) { update(m_nav.prev()); return; }
                m_app_audio->volumeDown();
                break;

            case app::eBtnEvent::R_PRESSED:
                m_pressR = true;
                break;

            case app::eBtnEvent::R_RELEASED:
                m_pressR = false;
                if (m_playlist_active) { update(m_nav.next()); return; }
                m_app_audio->volumeUp();
                break;

            default:
                break;
            }
            render();
        }

        void handleAudioEvent(app::eAudioEvent &evt)
        {
            std::lock_guard<std::mutex> lock(m_ui_access);
            m_playing = (evt == app::eAudioEvent::PLAYING);
            render();
        }
    };
}