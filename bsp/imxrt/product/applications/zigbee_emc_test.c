#include <rtthread.h>
#include <rtdevice.h>
#include <drv_gpio.h>

extern void GOS_Init(void);
extern void GOS_Start(void);
extern void ZA_ZigbeeApplicationStart(); 
extern void sys_extra_init(void);

int test_zigbee_init()
{
    sys_extra_init();  
    GOS_Init();
    GOS_Start();   
    rt_pin_mode(GET_PIN(BSP_USING_EFR32_RST_PORT, BSP_USING_EFR32_RST_PIN), PIN_MODE_OUTPUT);
    rt_pin_mode(GET_PIN(BSP_USING_ANTENNA_RF_SWITCH_EFR32_PORT, BSP_USING_ANTENNA_RF_SWITCH_EFR32_PIN), PIN_MODE_OUTPUT);
    ZA_ZigbeeApplicationStart(); 

    return 0;
}

INIT_APP_EXPORT(test_zigbee_init);

