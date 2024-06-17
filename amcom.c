#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "amcom.h"

/// Start of packet character
const uint8_t  AMCOM_SOP         = 0xA1;
const uint16_t AMCOM_INITIAL_CRC = 0xFFFF;

static uint16_t AMCOM_UpdateCRC(uint8_t byte, uint16_t crc)
{
	byte ^= (uint8_t)(crc & 0x00ff);
	byte ^= (uint8_t)(byte << 4);
	return ((((uint16_t)byte << 8) | (uint8_t)(crc >> 8)) ^ (uint8_t)(byte >> 4) ^ ((uint16_t)byte << 3));
}


void AMCOM_InitReceiver(AMCOM_Receiver* receiver, AMCOM_PacketHandler packetHandlerCallback, void* userContext) {
	receiver->packetHandler=packetHandlerCallback;
	receiver->userContext=userContext;
	receiver->payloadCounter=0;
	receiver->receivedPacketState=AMCOM_PACKET_STATE_EMPTY;
}

size_t AMCOM_Serialize(uint8_t packetType, const void* payload, size_t payloadSize, uint8_t* destinationBuffer) {
    
    if (payloadSize>200){
        return 0;
    }
    
    uint16_t crc=AMCOM_INITIAL_CRC;
    
	*destinationBuffer=AMCOM_SOP;
	*(destinationBuffer+1)=packetType;
	*(destinationBuffer+2)=payloadSize;
	
	crc = AMCOM_UpdateCRC(packetType,crc);
	crc = AMCOM_UpdateCRC(payloadSize,crc);

	
	if (payloadSize == 0){
	    *(destinationBuffer+3)=crc&0x00FF;
	    *(destinationBuffer+4)=crc>>8;
	    
	    return 5;
	}
	
	
	for(int i=0; i<payloadSize; i++){
	    crc = AMCOM_UpdateCRC(*(uint8_t*)(payload+i),crc);
	    *(destinationBuffer+5+i)= *(uint8_t*)(payload+i);
	}
	
	    *(destinationBuffer+3)=crc&0x00FF;
	    *(destinationBuffer+4)=crc>>8;
	
	return payloadSize+5;
}

void AMCOM_Deserialize(AMCOM_Receiver* receiver, const void* data, size_t dataSize) {
    for(int i=0;i<dataSize;i++){
        
        uint8_t dataByte=*(uint8_t*)(data+i);
        
        switch(receiver->receivedPacketState){
            case (AMCOM_PACKET_STATE_EMPTY):
                if(dataByte == 0xA1){  //check if sop is correct value, if not then skip
                    receiver->receivedPacket.header.sop=dataByte;
                    receiver->receivedPacketState=AMCOM_PACKET_STATE_GOT_SOP;
                }
                break;
                
            case (AMCOM_PACKET_STATE_GOT_SOP):
                receiver->receivedPacket.header.type=dataByte;
                receiver->receivedPacketState=AMCOM_PACKET_STATE_GOT_TYPE;
                break;
                
            case (AMCOM_PACKET_STATE_GOT_TYPE):
                if(0<=dataByte && dataByte<=200){ //check if size is in 0 to 200 range
                    receiver->receivedPacket.header.length=dataByte;
                    receiver->receivedPacketState=AMCOM_PACKET_STATE_GOT_LENGTH;
                }
                else{           // if size outside range go back to empty state
                    receiver->receivedPacketState=AMCOM_PACKET_STATE_EMPTY; 
                }
                break;
                
            case(AMCOM_PACKET_STATE_GOT_LENGTH):
                receiver->receivedPacket.header.crc=(uint16_t)dataByte;
                receiver->receivedPacketState=AMCOM_PACKET_STATE_GOT_CRC_LO;
                break;
                
            case(AMCOM_PACKET_STATE_GOT_CRC_LO):
            
                receiver->payloadCounter=0; //clear payloadCounter since packet is being assembled (with or without payload)
                
                receiver->receivedPacket.header.crc+=((uint16_t)dataByte)<<8;
                
                if (receiver->receivedPacket.header.length==0){
                    receiver->receivedPacketState=AMCOM_PACKET_STATE_GOT_WHOLE_PACKET;
                }
                else{
                    receiver->receivedPacketState=AMCOM_PACKET_STATE_GETTING_PAYLOAD;
                }
                break;
                
            case(AMCOM_PACKET_STATE_GETTING_PAYLOAD):
                
                receiver->receivedPacket.payload[(uint8_t)(receiver->payloadCounter++)]=dataByte;
                if (receiver->payloadCounter==receiver->receivedPacket.header.length){
                    receiver->receivedPacketState=AMCOM_PACKET_STATE_GOT_WHOLE_PACKET;
                }
                break;
                
            
        }
        
        if(receiver->receivedPacketState==AMCOM_PACKET_STATE_GOT_WHOLE_PACKET){

            //check crc
            uint16_t crc=0xFFFF;
            crc=AMCOM_UpdateCRC(receiver->receivedPacket.header.type,crc);
            crc=AMCOM_UpdateCRC(receiver->receivedPacket.header.length,crc);
            
            for (int i=0;i < receiver->receivedPacket.header.length;i++){
                crc=AMCOM_UpdateCRC(receiver->receivedPacket.payload[i],crc);
            }
            
            if (crc==receiver->receivedPacket.header.crc){ //good crc
                receiver->packetHandler(&receiver->receivedPacket, receiver->userContext);
                receiver->receivedPacketState=AMCOM_PACKET_STATE_EMPTY;
                
            }
            
            else{ //bad CRC
                
                for (int i=0;i < receiver->receivedPacket.header.length;i++){
                    
                } 
                receiver->receivedPacketState=AMCOM_PACKET_STATE_EMPTY;
            }
            //packet was handled either way, need to clear payloadCounter for next packet
            
        }
        
    }
}
