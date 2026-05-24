#include<iostream>
#include<cstdint>
#include<vector>   
#include<string>
#include<sstream>
#include<cstring>

#pragma pack(push,1)
typedef struct MessageHeader
{
    uint32_t magic;     //魔数
    uint8_t version;    //版本
    uint32_t length;    //body长度
    uint16_t type;
} MessageHeader;
#pragma pack(pop)
