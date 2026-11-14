#pragma once
#include <cstdint>
#include "nvs_flash.h"
#include "nvs.h"

namespace app
{
    // Tiny wrapper around NVS: remembers small values (volume, last animal) across power-offs
    class Settings
    {
    private:
        static constexpr const char *NS = "player";

    public:
        static void init(void)
        {
            esp_err_t e = nvs_flash_init();
            if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND)
            {
                nvs_flash_erase();
                nvs_flash_init();
            }
        }

        static uint8_t loadU8(const char *key, uint8_t def)
        {
            nvs_handle_t h;
            if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK)
                return def; // nothing saved yet
            uint8_t v = def;
            if (nvs_get_u8(h, key, &v) != ESP_OK)
                v = def;
            nvs_close(h);
            return v;
        }

        static void saveU8(const char *key, uint8_t val)
        {
            nvs_handle_t h;
            if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK)
                return;
            nvs_set_u8(h, key, val);
            nvs_commit(h);
            nvs_close(h);
        }
    };
}