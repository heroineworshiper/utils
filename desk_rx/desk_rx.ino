#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

extern "C" {
#include "osapi.h"
#include "os_type.h"
}

#define SSID "OVERPRICED APT"
#define PASSWORD "growamane"

#define PORT 1234
#define BUFSIZE 16
char incomingPacket[BUFSIZE];  // buffer for incoming packets

WiFiUDP Udp;
os_timer_t ticker;
#define HZ 20

// ticks between bits
#define UART_PERIOD (80000000 / 9600)

#define LED 2  //On board LED

// timeout timer
void handle_tick(void* x)
{
//    digitalWrite(LED,!(digitalRead(LED)));
}

// UART bit timer
void ICACHE_RAM_ATTR timer1_int()
{
    timer1_write(UART_PERIOD);
    digitalWrite(LED,!(digitalRead(LED)));
}

void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println();
    Serial.println();
    Serial.println();
    Serial.println();
    pinMode(LED, OUTPUT);
    digitalWrite(LED, 0);

    Serial.printf("Connecting to %s ", SSID);
    WiFi.begin(SSID, PASSWORD);
    
    int tick = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.print(".");
        tick++;
        if(tick >= 10) ESP.restart();
    }
    Serial.println(" connected");

    Udp.begin(PORT);
    Serial.printf("Now listening at IP %s, UDP port %d\n", 
        WiFi.localIP().toString().c_str(), 
        PORT);

    os_timer_setfn(&ticker, handle_tick, 0);
    os_timer_arm(&ticker, 1000 / HZ, 1);
    
    timer1_attachInterrupt(timer1_int);
    timer1_enable(TIM_DIV1, TIM_EDGE, TIM_SINGLE);
    timer1_write(UART_PERIOD);
}


void loop()
{
  int packetSize = Udp.parsePacket();
  if (packetSize)
  {
    // receive incoming UDP packets
    Serial.printf("loop %d: got %d bytes from %s, port %d\n", 
        __LINE__,
        packetSize, 
        Udp.remoteIP().toString().c_str(), 
        Udp.remotePort());
    int len = Udp.read(incomingPacket, BUFSIZE);
  }
}





