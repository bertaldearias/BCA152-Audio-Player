#pragma once
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "driver/i2s.h"
#include "audio_player.h"
#include "Settings.hpp"

namespace app
{
    // Read by the i2s_write wrapper in audio_player.cpp
    inline volatile uint8_t g_vol = 50;
    inline volatile bool g_user_muted = false;
    inline volatile bool g_sys_muted = false;
}

namespace
{
    void audio_player_callback(audio_player_cb_ctx_t *obj);
}

namespace app
{
    enum class eAudioEvent
    {
        PLAYING,
        STOPPED
    };

    class AppAudio
    {
    private:
        FILE *m_fp;
        audio_player_state_t m_player_state;
        QueueHandle_t m_event_queue;

    public:
        static constexpr int PIN_BCLK = 26;
        static constexpr int PIN_LRC = 25;
        static constexpr int PIN_DIN = 19;

        AppAudio() : m_fp(nullptr), m_player_state(AUDIO_PLAYER_STATE_IDLE), m_event_queue(nullptr) {}

        void init(audio_player_mute_fn fn)
        {
            i2s_config_t cfg = {};
            cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
            cfg.sample_rate = 44100;
            cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
            cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
            cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
            cfg.intr_alloc_flags = 0;
            cfg.dma_buf_count = 6;
            cfg.dma_buf_len = 512;
            cfg.tx_desc_auto_clear = true;
            i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);

            i2s_pin_config_t pins = {};
            pins.mck_io_num = I2S_PIN_NO_CHANGE;
            pins.bck_io_num = PIN_BCLK;
            pins.ws_io_num = PIN_LRC;
            pins.data_out_num = PIN_DIN;
            pins.data_in_num = I2S_PIN_NO_CHANGE;
            i2s_set_pin(I2S_NUM_0, &pins);

            audio_player_config_t config = {};
            config.port = I2S_NUM_0;
            config.mute_fn = fn;
            config.priority = 1;
            audio_player_new(config);
            audio_player_callback_register(audio_player_callback, this);

            m_event_queue = xQueueCreate(10, sizeof(app::eAudioEvent));
        }

        QueueHandle_t getEventQueue(void) const { return m_event_queue; }

        // returns the volume to show (0 when muted)
        uint8_t mute(bool m, bool toggle = false)
        {
            g_user_muted = toggle ? !g_user_muted : m;
            return getVolume();
        }

        bool isMuted(void) const { return g_user_muted; }

        void togglePlay(const std::string &filename)
        {
            switch (m_player_state)
            {
            case AUDIO_PLAYER_STATE_PLAYING:
                audio_player_pause();
                break;
            case AUDIO_PLAYER_STATE_PAUSE:
                audio_player_resume();
                break;
            default:
                m_fp = fopen(filename.c_str(), "rb");
                if (m_fp)
                    audio_player_play(m_fp);
                else
                    printf("Cannot open %s\n", filename.c_str());
                break;
            }
        }

        void stop(void)
        {
            if (m_player_state != AUDIO_PLAYER_STATE_IDLE)
                audio_player_stop();
        }

        // Short tone through the same I2S output (goes through the volume wrapper)
        void beep(int freq, int ms)
        {
            if (m_player_state == AUDIO_PLAYER_STATE_PLAYING)
                return; // a sound is playing, skip the beep to avoid mixing

            printf("beep %d Hz\n", freq);
            bool prevSysMute = g_sys_muted;
            g_sys_muted = false; // the player's pop-protection mute must not silence the beep

            const int rate = 44100;
            i2s_set_clk(I2S_NUM_0, rate, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);

            const int total = rate * ms / 1000;
            const int fade = rate / 200; // 5 ms fade in/out, prevents clicks
            const float step = 2.0f * 3.14159265f * freq / rate;
            float phase = 0;
            int16_t buf[128 * 2];
            size_t w = 0;

            for (int done = 0; done < total;)
            {
                int n = std::min(128, total - done);
                for (int i = 0; i < n; i++)
                {
                    int pos = done + i;
                    float env = 1.0f;
                    if (pos < fade) env = (float)pos / fade;
                    else if (total - pos < fade) env = (float)(total - pos) / fade;
                    int16_t s = (int16_t)(sinf(phase) * 30000.0f * env);
                    phase += step;
                    buf[2 * i] = s;
                    buf[2 * i + 1] = s;
                }
                i2s_write(I2S_NUM_0, buf, n * 4, &w, pdMS_TO_TICKS(100));
                done += n;
            }

            memset(buf, 0, sizeof(buf)); // a little silence so the tone ends cleanly
            i2s_write(I2S_NUM_0, buf, sizeof(buf), &w, pdMS_TO_TICKS(100));

            g_sys_muted = prevSysMute;
        }

        uint8_t volumeUp(void)
        {
            if (g_vol < 100) g_vol = g_vol + 10;
            Settings::saveU8("vol", g_vol); // remember the volume
            beep(1200, 80);
            return getVolume();
        }

        uint8_t volumeDown(void)
        {
            if (g_vol > 0) g_vol = g_vol - 10;
            Settings::saveU8("vol", g_vol); // remember the volume
            beep(800, 80);
            return getVolume();
        }

        uint8_t getVolume(void) const { return g_user_muted ? 0 : g_vol; }

        void setState(audio_player_state_t state) { m_player_state = state; }
    };
}

namespace
{
    void audio_player_callback(audio_player_cb_ctx_t *param)
    {
        app::AppAudio &app_audio = *(reinterpret_cast<app::AppAudio *>(param->user_ctx));
        audio_player_state_t state = audio_player_get_state();
        app_audio.setState(state);
        app::eAudioEvent evt = state == AUDIO_PLAYER_STATE_PLAYING
                                   ? app::eAudioEvent::PLAYING
                                   : app::eAudioEvent::STOPPED;
        xQueueSend(app_audio.getEventQueue(), (void *)(&evt), 0);
    }
}