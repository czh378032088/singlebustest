


#ifndef _SINGLEWIRENET_H
#define _SINGLEWIRENET_H

#include "stm32f4xx_hal.h"


void singleWireNetInit(void);
void singleWireNetUpdate(void);
int sendNetData(uint8_t disAddr,uint8_t *data,uint8_t length);
int sendNetDataEveryChl(uint8_t disAddr,uint8_t *data,uint8_t length);

#endif
