idf_component_register(SRCS "audio_player.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES driver esp_lcd spiffs nvs_flash iot_button chmorgan__esp-audio-player)

# Lets us scale audio samples for the volume control
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=i2s_write")