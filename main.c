#include "msp.h"
#include "G8RTOS.h"
#include <BSP.h>
#include "uart.h"
#include "LCDlib.h"
#include "Game.h"




void main(void){
    G8RTOS_Init();
    GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_P4, GPIO_PIN4);
    GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_P5, GPIO_PIN4);
    G8RTOS_InitSemaphore(&LCD_semaphore, 1);
    G8RTOS_InitSemaphore(&WIFI_semaphore, 1);
    G8RTOS_InitSemaphore(&LED_semaphore, 1);
    LCD_Init(false);
    initCC3100(GetPlayerRole());
    G8RTOS_Launch();

    while(1);
}

