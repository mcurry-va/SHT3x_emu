// Emulation of SHT3x over I2C, set using LCD+Buttons
// LCD Shield with buttons on WeMos D1 WiFi - comment out WEMOS_D1 for normal arduino pinout
#define WEMOS_D1 1

#include <Wire.h>
#include <LiquidCrystal.h>

// WeMos D1 GPIO to Arduino Uno GPIO map
// Not sure about the doubles - this is as shown on the silkscreen...
const int D1_GPIOMAP[14] =
{
     3, 1,16, 5,
     4,14,12,13,
     0, 2,15,13,
    12,14
};

typedef enum
{
    BTN_NONE = 0,
    BTN_LEFT,
    BTN_DOWN,
    BTN_UP,
    BTN_RIGHT,
    BTN_SELECT
} teButtons;

#ifdef WEMOS_D1
const int LCD_D4 = D1_GPIOMAP[4];
const int LCD_D5 = D1_GPIOMAP[5];
const int LCD_D6 = D1_GPIOMAP[6];
const int LCD_D7 = D1_GPIOMAP[7];
const int LCD_RS = D1_GPIOMAP[8];
const int LCD_EN = D1_GPIOMAP[9];
const int LCD_BL = D1_GPIOMAP[10];
#else
const int LCD_D4 = 4;
const int LCD_D5 = 5;
const int LCD_D6 = 6;
const int LCD_D7 = 7;
const int LCD_RS = 8;
const int LCD_EN = 9;
const int LCD_BL = 10;
#endif
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

void setup() 
{
    Serial.begin(115200);
    Serial.println("SHT30 Emulator (LCD shield)");

    // Configure I2C as slave for address 0x44 (SHT3x address A)
#ifdef WEMOS_D1
    Wire.begin(0x44, 14, 15);
#else
    Wire.begin(0x44);
#endif
    Wire.onReceive(handleRx);
    Wire.onRequest(handleReq);

    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(LCD_BL, OUTPUT);
    digitalWrite(LCD_BL, HIGH);       // HIGH for on
    digitalWrite(LED_BUILTIN, HIGH);  // HIGH for off
    
    lcd.begin(16, 2);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("SHT30 Emulator");
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
float temperature = 21.0f;
float rh = 50.0f;

teButtons GetButton()
{
    int raw = analogRead(A0);
    Serial.print("Buttons raw: ");
    Serial.println(raw);

#ifdef WEMOS_D1
    if (raw > 900)  return BTN_NONE;  // Typ 1024
    if (raw > 700)  return BTN_SELECT;// ???
    if (raw > 500)  return BTN_LEFT;  // Typ 678
    if (raw > 300)  return BTN_DOWN;  // Typ 429
    if (raw > 100)  return BTN_UP;    // Typ 173
    return BTN_RIGHT; // Typ 6
#else
    if (raw > 750)  return BTN_NONE;  // Typ 1023
    if (raw > 550)  return BTN_SELECT;// Typ 639
    if (raw > 350)  return BTN_LEFT;  // Typ 409
    if (raw > 150)  return BTN_DOWN;  // Typ 256
    if (raw > 50)   return BTN_UP;    // Typ 99
    return BTN_RIGHT; // Typ 0
#endif
}

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

typedef enum
{
    MENU_TEMPERATURE = 0,
    MENU_RH,
    
    MENU_MAX
} teMenus;

teMenus menu = MENU_TEMPERATURE;

void DisplayCurrent()
{
    lcd.clear();
    lcd.setCursor(0,0);
    
    switch (menu)
    {
    case MENU_RH:
        lcd.print("Set RH %");
        lcd.setCursor(0,1);
        lcd.print(rh);
        break;
    
    case MENU_TEMPERATURE:
        lcd.print("Set temp degC");
        lcd.setCursor(0,1);
        lcd.print(temperature);
        break;
    }
}

void IncMenu()
{
    menu = static_cast<teMenus>(menu + 1);
    if (menu >= MENU_MAX)
    {
        menu = static_cast<teMenus>(0);
    }
    DisplayCurrent();
    Serial.print("Menu ");
    Serial.println(menu);
}

void DecMenu()
{
    if (0 == (int)menu)
    {
        menu = static_cast<teMenus>(MENU_MAX - 1);
    }
    else
    {
        menu = static_cast<teMenus>(menu - 1);
    }
    DisplayCurrent();
    Serial.print("Menu ");
    Serial.println(menu);
}

void IncValue()
{
    Serial.print("Value ");
  
    switch (menu)
    {
    case MENU_RH:
        rh += 1.0f;
        if (rh > 100.0f)
        {
            rh = 100.0f;
        }
        Serial.println(rh);
        break;

    case MENU_TEMPERATURE:
        temperature += 1.0f;
        if (temperature > 50.0f)
        {
            temperature = 50.0f;
        }
        Serial.println(temperature);
        break;
    }

    DisplayCurrent();
}

void DecValue()
{
    Serial.print("Value ");
    
    switch (menu)
    {
    case MENU_RH:
        rh -= 1.0f;
        if (rh < 0.0f)
        {
            rh = 0.0f;
        }
        Serial.println(rh);
        break;

    case MENU_TEMPERATURE:
        temperature -= 1.0f;
        if (temperature < -20.0f)
        {
            temperature = -20.0f;
        }
        Serial.println(temperature);
        break;
    }

    DisplayCurrent();
}

teButtons lastButton = BTN_NONE;
uint32_t lastButtonChange = 0;
uint32_t upticks = 0;

void loop() 
{
    auto button = GetButton();
    int heldTime = 0;
    if (button != lastButton)
    {
        lastButtonChange = upticks;
        lastButton = button;
        digitalWrite(LCD_BL, HIGH); // Turn on backlight
    }
    else
    {
        heldTime = (upticks - lastButtonChange);
        Serial.print("Held time ");
        Serial.println(heldTime);
        if (heldTime < 10)
        {
            button = BTN_NONE;
        }
    }
    
    switch (button)
    {
    case BTN_LEFT:  DecMenu();  break;
    case BTN_RIGHT: IncMenu();  break;
    case BTN_UP:    IncValue(); break;
    case BTN_DOWN:  DecValue(); break;

    case BTN_NONE:
        if (heldTime > 300) // 30 sec
        {
            digitalWrite(LCD_BL, LOW); // Turn off backlight
        }
        break;
    }

    upticks++;
    delay(100);
}
