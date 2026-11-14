#include <cstdio>
#include <algorithm>
#include "esp_spiffs.h"
#include "AppButton.hpp"
#include "AppAudio.hpp"
#include "AppUi.hpp"

namespace
{
    app::AppButton m_app_btn;
    app::AppAudio m_app_audio;
    app::AppUi m_app_ui;

    // Called by the audio player around play/pause (pop avoidance)
    esp_err_t audio_mute_function(AUDIO_PLAYER_MUTE_SETTING setting)
    {
        app::g_sys_muted = (setting == AUDIO_PLAYER_MUTE);
        return ESP_OK;
    }
}

// Software volume: the audio player calls i2s_write(); we scale the samples first.
extern "C" esp_err_t __real_i2s_write(i2s_port_t port, const void *src, size_t size,
                                      size_t *written, TickType_t wait);

extern "C" esp_err_t __wrap_i2s_write(i2s_port_t port, const void *src, size_t size,
                                      size_t *written, TickType_t wait)
{
    const int BOOST = 3; // 1 = normal, 2 = double, 3 = triple. Lower it if the sound crackles.
    int vol = (app::g_user_muted || app::g_sys_muted) ? 0 : app::g_vol;
    int gain = vol * BOOST; // linear: volume 50 with BOOST 3 = 150%
    const int16_t *in = static_cast<const int16_t *>(src);
    int16_t buf[512];
    size_t done = 0;
    esp_err_t err = ESP_OK;

    while (done < size)
    {
        size_t n = std::min(sizeof(buf), size - done);
        size_t samples = n / 2;
        for (size_t i = 0; i < samples; i++)
        {
            int v = ((int)in[done / 2 + i] * gain) / 100;
            if (v > 32767) v = 32767;   // clamp so loud peaks don't wrap around
            if (v < -32768) v = -32768;
            buf[i] = (int16_t)v;
        }
        size_t w = 0;
        err = __real_i2s_write(port, buf, samples * 2, &w, wait);
        done += w;
        if (err != ESP_OK || w < samples * 2) break;
    }
    if (written) *written = done;
    return err;
}

extern "C" void app_main(void)
{
    esp_vfs_spiffs_conf_t conf = {};
    conf.base_path = "/spiffs";
    conf.partition_label = "storage";
    conf.max_files = 5;
    conf.format_if_mount_failed = false;
    printf("SPIFFS: %s\n", esp_err_to_name(esp_vfs_spiffs_register(&conf)));

    m_app_btn.init();
    m_app_audio.init(audio_mute_function);
    m_app_ui.init(&m_app_btn, &m_app_audio);
}