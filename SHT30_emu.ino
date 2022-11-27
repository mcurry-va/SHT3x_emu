#include "Wire.h"

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);

    Serial.begin(115200);

    Serial.write("SHT30 Emulator\r\n");

    // CRC 0xBEEF = 0x92 test
//    uint8_t test[2] = { 0xBE, 0xEF };
//    uint8_t crc = crcGen(test, 2);
//    Serial.write("0xBEEF CRC = ");
//    Serial.print(crc, HEX);
//    Serial.write("\r\n");
    
    //analogReadResolution(12);
    
    // Configure I2C as slave for address 0x44 (SHT3x address A)
    Wire.begin(0x44);
    Wire.onReceive(handleRx);
    Wire.onRequest(handleReq);
}

typedef enum : uint16_t
{
    CMD_NONE = 0,

    CMD_READ_SERIALNBR    = 0x3780,   // read serial number 
    CMD_READ_STATUS       = 0xF32D,   // read status register 
    CMD_CLEAR_STATUS      = 0x3041,   // clear status register 
    CMD_HEATER_ENABLE     = 0x306D,   // enabled heater 
    CMD_HEATER_DISABLE    = 0x3066,   // disable heater 
    CMD_SOFT_RESET        = 0x30A2,   // soft reset 
    CMD_MEAS_CLOCKSTR_H   = 0x2C06,   // measurement: clock stretching, high repeatability 
    CMD_MEAS_CLOCKSTR_M   = 0x2C0D,   // measurement: clock stretching, medium repeatability 
    CMD_MEAS_CLOCKSTR_L   = 0x2C10,   // measurement: clock stretching, low repeatability 
    CMD_MEAS_POLLING_H    = 0x2400,   // measurement: polling, high repeatability 
    CMD_MEAS_POLLING_M    = 0x240B,   // measurement: polling, medium repeatability 
    CMD_MEAS_POLLING_L    = 0x2416,   // measurement: polling, low repeatability 
    CMD_MEAS_PERI_05_H    = 0x2032,   // measurement: periodic 0.5 mps, high repeatability 
    CMD_MEAS_PERI_05_M    = 0x2024,   // measurement: periodic 0.5 mps, medium repeatability 
    CMD_MEAS_PERI_05_L    = 0x202F,   // measurement: periodic 0.5 mps, low repeatability 
    CMD_MEAS_PERI_1_H     = 0x2130,   // measurement: periodic 1 mps, high repeatability 
    CMD_MEAS_PERI_1_M     = 0x2126,   // measurement: periodic 1 mps, medium repeatability 
    CMD_MEAS_PERI_1_L     = 0x212D,   // measurement: periodic 1 mps, low repeatability 
    CMD_MEAS_PERI_2_H     = 0x2236,   // measurement: periodic 2 mps, high repeatability 
    CMD_MEAS_PERI_2_M     = 0x2220,   // measurement: periodic 2 mps, medium repeatability 
    CMD_MEAS_PERI_2_L     = 0x222B,   // measurement: periodic 2 mps, low repeatability 
    CMD_MEAS_PERI_4_H     = 0x2334,   // measurement: periodic 4 mps, high repeatability 
    CMD_MEAS_PERI_4_M     = 0x2322,   // measurement: periodic 4 mps, medium repeatability 
    CMD_MEAS_PERI_4_L     = 0x2329,   // measurement: periodic 4 mps, low repeatability 
    CMD_MEAS_PERI_10_H    = 0x2737,   // measurement: periodic 10 mps, high repeatability 
    CMD_MEAS_PERI_10_M    = 0x2721,   // measurement: periodic 10 mps, medium repeatability 
    CMD_MEAS_PERI_10_L    = 0x272A,   // measurement: periodic 10 mps, low repeatability 
    CMD_STOP_PERIODIC     = 0x3093,   
    CMD_FETCH_DATA        = 0xE000,   // readout measurements for periodic mode 
    CMD_R_AL_LIM_LS       = 0xE102,   // read alert limits, low set 
    CMD_R_AL_LIM_LC       = 0xE109,   // read alert limits, low clear 
    CMD_R_AL_LIM_HS       = 0xE11F,   // read alert limits, high set 
    CMD_R_AL_LIM_HC       = 0xE114,   // read alert limits, high clear 
    CMD_W_AL_LIM_HS       = 0x611D,   // write alert limits, high set 
    CMD_W_AL_LIM_HC       = 0x6116,   // write alert limits, high clear
    CMD_W_AL_LIM_LC       = 0x610B,   // write alert limits, low clear 
    CMD_W_AL_LIM_LS       = 0x6100,   // write alert limits, low set 
    CMD_NO_SLEEP          = 0x303E,   
} teCommands;

teCommands i2cCommand = CMD_NONE;
float temperature;
float rh;

void handleRx(int len)
{
    // I2C write
    Serial.write("I2C_RX: ");
    Serial.print(len);
    Serial.write(" bytes - ");
    
    uint8_t rxBuff[2];
    if (len != 2)
    {
        // uint16_t commands expected - discard if more (TBC)
        while (Wire.available())
        {
            char tmp = Wire.read();
            Serial.print(tmp, HEX);
        }
        //digitalWrite(LED_BUILTIN, LOW);
        i2cCommand = CMD_NONE;
    }
    else
    {
        rxBuff[0] = Wire.read();  // Command MSB
        rxBuff[1] = Wire.read();  // Command LSB
        i2cCommand = (teCommands)((rxBuff[0] << 8) + rxBuff[1]);
        Serial.print(i2cCommand, HEX);
        //digitalWrite(LED_BUILTIN, HIGH);
    }

    Serial.write("\r\n");
    digitalWrite(LED_BUILTIN, HIGH);
}

void handleReq()
{
    // I2C read
    Serial.write("I2C_Req: ");
    switch (i2cCommand)
    {
    case CMD_READ_SERIALNBR:
        Serial.write("serial ");
        outputU16(0x1234);
        break;

    // All of these commands result in T + RH being output (ignore clock stretching)
    case CMD_MEAS_CLOCKSTR_H:
    case CMD_MEAS_CLOCKSTR_M:
    case CMD_MEAS_CLOCKSTR_L:
    case CMD_MEAS_POLLING_H:
    case CMD_MEAS_POLLING_M:
    case CMD_MEAS_POLLING_L:
    case CMD_FETCH_DATA:
        Serial.write("t:");
        Serial.print(temperature);
        Serial.write(" rh:");
        Serial.print(rh);
        Serial.write(" ");
        if ( outputU16( (uint16_t)((temperature + 45.0) * 65535.0 / 175.0) ) )
        {
            // Only start outputting the RH if all the temperature was ACK'd
            outputU16( (uint16_t)((rh * 65535.0) / 100.0) );
        }
        break;
    }

    Serial.write("\r\n");

    i2cCommand = CMD_NONE;
    //digitalWrite(LED_BUILTIN, LOW);
}

uint8_t  crcGen( uint8_t* data, uint32_t size )
{
    static const uint32_t  POLYNOMIAL = 0x131; // P(x) = x^8 + x^5 + x^4 + 1 = 100110001
    uint8_t     bitIndex = 0;           // bit mask 
    uint8_t     crc = 0xFF;             // init value
    uint8_t     index = 0;              // byte counter
  
    for( index = 0; index < size; index++)
    {
        crc ^= data[index];

        for( bitIndex = 0; bitIndex < 8; bitIndex++ )
        {
            if ( crc & 0x80 ) crc = (crc << 1) ^ POLYNOMIAL; 
            else              crc = (crc << 1);
        }
    }
    
    return crc; 
}

bool outputU16(uint16_t val)
{
    uint8_t outBuf[2] = { (uint8_t)(val >> 8), (uint8_t)(val & 0xff) };
  
    if (1 == Wire.write(outBuf[0]))
    {
        Serial.print(outBuf[0], HEX);
        if (1 == Wire.write(outBuf[1]))
        {
            Serial.print(outBuf[1], HEX);
            uint8_t crc = crcGen(outBuf, 2);
            if (1 == Wire.write(crc))
            {
                Serial.print(crc, HEX);
                return true;
            }
            else
            {
                Serial.write(" NAK");
            }
        }
        else
        {
            Serial.write(" NAK");
        }
    }
    else
    {
        Serial.write(" NAK");
    }
    
    return false;
}

void loop()
{ 
    // Read potentiometers and update local (10bit 5V ADC)
    temperature = ((float)analogRead(A0) / 1023.0f) * 70.0f - 20.0f;  // scale -20 to +50 °C
    rh = ((float)analogRead(A1) / 1023.0f) * 100.0f;  // Scale 0 to 100%
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
}
