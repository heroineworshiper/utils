#include <pic16f1508.h>
#include <stdint.h>


// 900Mhz radio Si4421

#define RADIO_SDO_LAT LATC7
#define RADIO_SDO_TRIS TRISC7

#define RADIO_SCK_LAT LATC6
#define RADIO_SCK_TRIS TRISC6

#define RADIO_CS_LAT LATC3
#define RADIO_CS_TRIS TRISC3

// RADIO_CHANNEL is from 96-3903 & set by the user
// data rate must be slow enough to service FIFOs
// kbps = 10000 / (29 * (DRVSREG<6:0> + 1) * (1 + DRPE * 7))
// RADIO_BAUD_CODE = 10000 / (29 * kbps) / (1 + DRPE * 7) - 1
// RADIO_DATA_SIZE is the amount of data to read before resetting the sync code

//#define RADIO_CHANNEL 3903
// DEBUG
#define RADIO_CHANNEL 96

// scan for synchronous code
#define FIFORSTREG 0xCA81
// read continuously
//#define FIFORSTREG              (0xCA81 | 0x0004)
// 915MHz
#define FREQ_BAND 0x0030
// Center Frequency: 915.000MHz
#define CFSREG (0xA000 | RADIO_CHANNEL)
// crystal load 10pF
#define XTAL_LD_CAP 0x0003
// power management page 16
#define PMCREG 0x8201
#define GENCREG (0x8000 | XTAL_LD_CAP | FREQ_BAND)
#define SOFT_RESET 0xfe00

// +3/-4 Fres
//#define AFCCREG 0xc4f7
// +15/-16 Fres
#define AFCCREG 0xc4d7

// Data Rate
// data rate must be slow enough to service FIFOs
// kbps = 10000 / (29 * (DRVSREG<6:0> + 1) * (1 + DRPE * 7))
// RADIO_BAUD_CODE = 10000 / (29 * kbps) - 1
#define RADIO_BAUD_CODE 3

// data rate prescaler.  Divides data rate by 8 if 1
//#define DRPE (1 << 7)
#define DRPE 0
#define DRVSREG (0xC600 | DRPE | RADIO_BAUD_CODE)


// Page 37 of the SI4421 datasheet gives optimum bandwidth values
// but the lowest that works is 200khz
//#define RXCREG 0x9481     // BW 200KHz, LNA gain 0dB, RSSI -97dBm
//#define RXCREG 0x9440     // BW 340KHz, LNA gain 0dB, RSSI -103dBm
#define RXCREG 0x9420       // BW 400KHz, LNA gain 0dB, RSSI -103dBm

//#define TXCREG 0x9850     // FSK shift: 90kHz
#define TXCREG 0x98f0       // FSK shift: 165kHz
#define STSREG 0x0000
#define RXFIFOREG 0xb000

// analog filter for raw mode
#define BBFCREG                 0xc23c


extern uint8_t tick;


void write_radio(uint16_t data)
{
    RADIO_CS_LAT = 0;
    uint8_t i;
    for(i = 0; i < 16; i++)
    {
        RADIO_SDO_LAT = (uint8_t)((data & 0x8000) ? 1 : 0);
        data <<= 1;
        RADIO_SCK_LAT = 1;
        RADIO_SCK_LAT = 0;
    }
    RADIO_CS_LAT = 1;
}

void radio_on()
{
// enable outputs
    RADIO_CS_LAT = 1;
    RADIO_CS_TRIS = 0;
    
    RADIO_SDO_LAT = 0;
    RADIO_SDO_TRIS = 0;
    
    RADIO_SCK_LAT = 0;
    RADIO_SCK_TRIS = 0;

// scan for synchronous code
//    write_radio(FIFORSTREG);
    
// enable synchron latch
    write_radio(FIFORSTREG | 0x0002);
    write_radio(GENCREG);
    write_radio(AFCCREG);
    write_radio(CFSREG);
    write_radio(DRVSREG);
    write_radio(PMCREG);
    write_radio(RXCREG);
    write_radio(TXCREG);
    write_radio(BBFCREG);

// receive mode page 16
//    write_radio(PMCREG | 0x0080);
}

void transmit_mode()
{
// switch UART to transmit mode
    RCSTAbits.SREN = 0;
    TXSTAbits.TXEN = 1;
// turn on the transmitter to tune page 16
    write_radio(PMCREG | 0x0020);

// warm up
    tick = 0;
    while(tick < 5)
    {
        ;
    }
}

// flaky
void idle_mode()
{
// switch UART to receive mode page 16
    TXSTAbits.TXEN = 0;
    RCSTAbits.SREN = 1;
    write_radio(PMCREG | 0x0018);
}

void receive_mode()
{
// switch UART to receive mode
    TXSTAbits.TXEN = 0;
    RCSTAbits.SREN = 1;
    write_radio(PMCREG | 0x0080);
}

void reset_radio()
{
// page 36
    write_radio(SOFT_RESET);
    tick = 0;
    while(tick < 2) ;
}



