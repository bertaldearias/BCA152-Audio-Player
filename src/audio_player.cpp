#include <cstdio>
#include "AppButton.hpp"

namespace { app::AppButton m_app_btn; }

extern "C" void app_main(void)
{
    m_app_btn.init();
    printf("Button test ready\n");

    app::eBtnEvent evt;
    while (true)
    {
        if (xQueueReceive(m_app_btn.getEventQueue(), &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case app::eBtnEvent::L_PRESSED:  printf("LEFT pressed\n"); break;
            case app::eBtnEvent::L_RELEASED: printf("LEFT released\n"); break;
            case app::eBtnEvent::M_CLICK:    printf("MIDDLE click\n"); break;
            case app::eBtnEvent::M_DCLICK:   printf("MIDDLE double-click\n"); break;
            case app::eBtnEvent::R_PRESSED:  printf("RIGHT pressed\n"); break;
            case app::eBtnEvent::R_RELEASED: printf("RIGHT released\n"); break;
            }
        }
    }
}