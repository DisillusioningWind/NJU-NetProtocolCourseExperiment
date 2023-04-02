#pragma once

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define MAXLINE 4096
#define SERV_ADDR "47.105.85.28"
#define SERV_PORT 4321
#define WARN_UNUSED_RESULT

#pragma pack(1)

enum weather : uint8_t
{
    overcast = 0x00,
    sunny,
    cloudy,
    rain,
    fog,
    rainstorm,
    thunderstorm,
    breeze,
    sand_st0rm
};

struct condition
{
    weather wea;
    uint8_t temp;

    const char* getWeather()
    {
        switch (this->wea)
        {
        case overcast:return "overcast";
        case sunny:return "sunny";
        case cloudy:return "cloudy";
        case rain:return "rain";
        case fog:return "fog";
        case rainstorm:return "rainstorm";
        case thunderstorm:return "thunderstorm";
        case breeze:return "breeze";
        case sand_st0rm:return "sand_st0rm";
        default:return "wrong weather";
        }
    }
    int getTemp()
    {
        return temp;
    }
};

struct sendpkt
{
    uint8_t stype;
    uint8_t qtype;
    char cname[30];
    uint8_t number;

    void zero()
    {
        memset(this, 0, sizeof(*this));
    }

    sendpkt() { zero(); }
    sendpkt(uint8_t st, uint8_t qt, char n[], uint8_t num)
    {
        zero();
        stype = st;
        qtype = qt;
        number = num;
        strcpy(cname, n);
    }
};

struct recvpkt
{
    uint8_t field1;
    uint8_t field2;
    char cname[30];
    uint16_t year;
    uint8_t mon;
    uint8_t day;
    uint8_t num;
    condition con[45];

    recvpkt() { memset(this, 0, sizeof(*this)); }
    bool isCitypkt() { return field1 == 0x01; }
    bool isWorngpkt() { return field1 == 0x02; }
    bool isWeapkt() { return field1 == 0x03; }
    bool isNoinfopkt() { return field1 == 0x04; }
    char getType() { return (char)field2; }
    int getYear() { return ntohs(year); }
    int getMon() { return mon; }
    int getDay() { return day; }
};

#pragma pack()