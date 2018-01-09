

#ifndef _SINGLEWIREBUS_H
#define _SINGLEWIREBUS_H

#include "stm32f4xx_hal.h"
#define  MAX_NUM_SIZE  32

#define GET_SYS_CLOCK  HAL_GetTick

typedef __packed struct {
    uint8_t disAddr;
	  uint8_t srcAddr;
	  uint8_t serialNum;
	  uint8_t length;
	  uint8_t data[MAX_NUM_SIZE + 1];
}SingleBusStruct;


void singleWireBusInit(void);
void singleWireBusUpdate(void);
int  getSingleBusData(SingleBusStruct *p_data);
int  sendSingleBusData(int channel,SingleBusStruct *p_data,int retry);
int  getSendFaileTimes(int channel);
uint32_t getLastReceiveTime(int channel);
uint32_t getLastActiveTime(int channel);
uint32_t getLastSendTime(int channel);

#endif
