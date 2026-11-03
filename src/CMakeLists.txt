idf_component_register(SRCS "audio_player.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES driver esp_lcd spiffs nvs_flash)