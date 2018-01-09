
#include "singlewirebus.h"
#include "singlewirenet.h"
#include <stdbool.h>
#include <string.h>

#define DEVICE_TYPE_ADDR    0x06


#define RETAIN_MAX_HANDLED   0x08
#define SEND_FIFO_SIZE       0x04

#define ADDR_MATCH_ALL       0xff
#define ADDR_NOT_FORWORD     0xfe

#define CMD_INTRODUCE_ME     0xf0
#define CMD_UNDERSTAND_YOU   0xf1
#define CMD_OFFLINE_NODE     0xf2

typedef  struct
{
	SingleBusStruct fifoSendPacket[SEND_FIFO_SIZE];
	int head;
	int tail;
	
	uint8_t activeFlag;
	uint8_t connectChannelAddr;
}SingleNetChannelStruct;

static uint8_t routeChannel[256];
static uint32_t handledPacketHead[RETAIN_MAX_HANDLED];
static SingleNetChannelStruct netChannel[6];
static uint32_t serialNum;
static uint8_t ownAddr = DEVICE_TYPE_ADDR;
static int handledPacketIndex;
static bool nodeConnected,sendedCmdUnd;
static uint32_t lastSendTime;

static uint8_t checkSum(SingleBusStruct *receivePacket)
{
	uint8_t *p = (uint8_t *)receivePacket;
	int length = receivePacket->length + 4;
	uint8_t sum = 0;
	
	for(int i = 0 ; i < length ; i ++)
	{
		sum += p[i];
	}
	
	return sum;
}

static bool isActivePacket(SingleBusStruct *receivePacket)
{
	if(receivePacket->length > MAX_NUM_SIZE)
		return false;
	
	return (checkSum(receivePacket) == receivePacket->data[receivePacket->length]);
}


static bool isHandled(SingleBusStruct *receivePacket)
{
	uint32_t handleHead;
	
	memcpy(&handleHead,receivePacket,4);
	for(int i = 0 ; i < RETAIN_MAX_HANDLED ; i ++)
	{
		if(handleHead == handledPacketHead[i])
			return true;
	}
	handledPacketHead[handledPacketIndex ++] = handleHead;
	handledPacketIndex %= RETAIN_MAX_HANDLED;
	return false;
}

static void moveFifoToChannel(void)
{
	for(int i = 0 ; i < 6 ; i ++)
	{
		if(netChannel[i].head > netChannel[i].tail)
		{
			if(sendSingleBusData(i,netChannel[i].fifoSendPacket + (netChannel[i].tail % SEND_FIFO_SIZE),3) != -1)
			{
				netChannel[i].tail ++;
			}
		}
	}
}

static bool sendPacketActChannel(uint8_t channel,SingleBusStruct *receivePacket)
{
	if(netChannel[channel].activeFlag == 0)
		return false;
	if(netChannel[channel].head - netChannel[channel].tail >= SEND_FIFO_SIZE)
		return false;
	
	memcpy(netChannel[channel].fifoSendPacket + (netChannel[channel].head % SEND_FIFO_SIZE),receivePacket,receivePacket->length + 5);
	netChannel[channel].head ++;
	return true;
}

static bool sendPacketChannel(uint8_t channel,SingleBusStruct *receivePacket)
{
	if(netChannel[channel].head - netChannel[channel].tail >= SEND_FIFO_SIZE)
		return false;
	
	memcpy(netChannel[channel].fifoSendPacket + (netChannel[channel].head % SEND_FIFO_SIZE),receivePacket,receivePacket->length + 5);
	netChannel[channel].head ++;
	return true;
}

static bool relayBroadcastExceptChl(SingleBusStruct *receivePacket,uint8_t expChannel) 
{
	for(int i = 0 ; i < 6 ; i ++)
	{
		if(i != expChannel)
		{
			sendPacketActChannel(i,receivePacket);
		}
	}
	return true;
}

static bool relayPacketFromChl(SingleBusStruct *receivePacket,uint8_t fromChannel) 
{
	uint8_t sendChannel = routeChannel[receivePacket->disAddr] ;
	if(sendChannel < 6 && sendChannel > 0)
		sendPacketActChannel(sendChannel,receivePacket);
	else
		relayBroadcastExceptChl(receivePacket,fromChannel);
	return true;
}


static void handleCmdFromChl(SingleBusStruct *receivePacket,uint8_t fromChannel)
{
	if(receivePacket->length == 0)
		return;
	switch(receivePacket->data[0])
	{
		case CMD_UNDERSTAND_YOU:
			sendNetData(receivePacket->srcAddr,NULL,0);
			break;
		case CMD_OFFLINE_NODE:
			routeChannel[receivePacket->data[1]] = 0xff;
			break;
	}
}

void singleWireNetInit(void)
{
	for(int i = 0 ; i < 256 ; i ++)
	{
		routeChannel[i] = 0xff;
	}
	
	for(int i = 0 ; i < RETAIN_MAX_HANDLED; i ++)
	{
		handledPacketHead[i] = 0;
	}
	
	for(int i = 0 ; i < 6 ; i ++)
	{
		netChannel[i].activeFlag = 0;
		netChannel[i].head = 0;
	  netChannel[i].tail = 0;
		netChannel[i].connectChannelAddr = 0xff;
	}
	serialNum = 0;
	handledPacketIndex = 0;
	nodeConnected = false;
	sendedCmdUnd = false;
	lastSendTime = 0;
}

void singleWireNetUpdate(void)
{
	int receiveChannel;
	bool channelChange = false;
	
	SingleBusStruct receivePacket;
	
	singleWireBusUpdate();
	moveFifoToChannel();
	
	do
	{
	  receiveChannel = getSingleBusData(&receivePacket);
		
		if(receiveChannel == -1)
			break;
		
		if(!isActivePacket(&receivePacket) || isHandled(&receivePacket))
			continue;
		
		if(receivePacket.disAddr == ADDR_MATCH_ALL)
		{
			relayBroadcastExceptChl(&receivePacket,receiveChannel);
		}
			
		if(receivePacket.disAddr == ADDR_MATCH_ALL || receivePacket.disAddr == ADDR_NOT_FORWORD || receivePacket.disAddr == ownAddr )
		{
			if(receivePacket.disAddr == ADDR_NOT_FORWORD)
				netChannel[receiveChannel].connectChannelAddr = receivePacket.srcAddr;
			handleCmdFromChl(&receivePacket,receiveChannel);
		}
		else
		{
			relayPacketFromChl(&receivePacket,receiveChannel);
		}
		routeChannel[receivePacket.srcAddr] = receiveChannel;
    netChannel[receiveChannel].activeFlag = 1;
		nodeConnected = true;
	}while(1);
	
	if(GET_SYS_CLOCK() - lastSendTime >= 400)
	{
		lastSendTime = GET_SYS_CLOCK();
	  if(!sendedCmdUnd || !nodeConnected)
	  {
		  uint8_t cmd = CMD_UNDERSTAND_YOU;
		  sendNetDataEveryChl(ADDR_MATCH_ALL,&cmd,1);
			sendedCmdUnd = true;
	  }
	  else 
	  {
			uint8_t cmd = CMD_INTRODUCE_ME;
		  sendNetDataEveryChl(ADDR_NOT_FORWORD,&cmd,1);
	  }
		
		for(int i = 0 ; i < 6 ; i ++)
		{
			if((GET_SYS_CLOCK() - getLastActiveTime(i) >= 1500) && netChannel[i].activeFlag)
			{
				netChannel[i].activeFlag = 0;
				uint8_t cmd = CMD_UNDERSTAND_YOU;
		    sendNetDataEveryChl(ADDR_MATCH_ALL,&cmd,1);
			}
		}
  }
}

int sendNetDataEveryChl(uint8_t disAddr,uint8_t *data,uint8_t length)
{
	SingleBusStruct sendPacket;
	
	if(length > MAX_NUM_SIZE)
		return -1;
	else if(length > 0)
		memcpy(sendPacket.data,data,length);
	
	sendPacket.srcAddr = ownAddr;
	sendPacket.disAddr = disAddr;
	sendPacket.length = length;
	sendPacket.serialNum = serialNum ++;
	
	sendPacket.data[length] = checkSum(&sendPacket);
	
	for(int i = 0 ; i < 6 ; i ++)
	  sendPacketChannel(i,&sendPacket);
	
	return 0;
}

int sendNetData(uint8_t disAddr,uint8_t *data,uint8_t length)
{
	SingleBusStruct sendPacket;
	
	if(length > MAX_NUM_SIZE)
		return -1;
	else if(length > 0)
		memcpy(sendPacket.data,data,length);
	
	sendPacket.srcAddr = ownAddr;
	sendPacket.disAddr = disAddr;
	sendPacket.length = length;
	sendPacket.serialNum = serialNum ++;
	
	sendPacket.data[length] = checkSum(&sendPacket);
		
	relayPacketFromChl(&sendPacket,0xff);	
	return 0;
}

