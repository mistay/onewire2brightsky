
#include <EtherCard.h>
#define CS_PIN 10

#include <EEPROM.h>
#include <TrueRandom.h>
//#include "Timer.h"
//Timer t;


#include <OneWire.h>
OneWire ds(2);
OneWire ds2(3);
OneWire ds3(4);
OneWire ds4(5);
#define MAX_DS1820_SENSORS 12
byte addr[MAX_DS1820_SENSORS][8];
  
  
  
#include <avr/wdt.h>

// Universally administered and locally administered addresses are distinguished by setting the second least significant bit of the most significant byte of the address. If the bit is 0, the address is universally administered. If it is 1, the address is locally administered. In the example address 02-00-00-00-00-01 the most significant byte is 02h. The binary is 00000010 and the second least significant bit is 1. Therefore, it is a locally administered address.[3] The bit is 0 in all OUIs.
// so mac should be:
// x2-xx-xx-xx-xx-xx
// x6-xx-xx-xx-xx-xx
// xA-xx-xx-xx-xx-xx
// xE-xx-xx-xx-xx-xx
static byte mymac[] = { 0x06,0x02,0x03,0x04,0x05,0x06 };
char macstr[18];

byte Ethernet::buffer[700];
static uint32_t timer = 0;

const char website[] PROGMEM = "temp.brightsky.at";

int LED_LAN_RDY =2;
int LED_LAN_ROOCESSING =3;


int ResetGPIO = A3;


bool requestPending=false;
int heartbeatreset = 0;

static void my_callback (byte status, word off, word len) {
  requestPending=false;
  
  Serial.println(">>>");
  Ethernet::buffer[off+300] = 0;
  Serial.print((const char*) Ethernet::buffer + off);
  Serial.println("...");
  //wdt_reset();
}

uint16_t values[10];
uint8_t debounce[10];



int heartbeat=0;


void(* resetFunc) (void) = 0;

int HighByte, LowByte, TReading, SignBit, Tc_100, Whole, Fract;

void setup () {
  
  Serial.begin(115200);
  Serial.println(F("\nonewire2brightsky v20170208 starting..."));
  
  if (EEPROM.read(1) != '#') {
    Serial.println(F("\nWriting EEProm..."));
  
    EEPROM.write(1, '#');
    
    for (int i = 3; i < 6; i++) {
      EEPROM.write(i, TrueRandom.randomByte());
    }
  }
  
  // read 3 last MAC octets
  for (int i = 3; i < 6; i++) {
      mymac[i] = EEPROM.read(i);
  }
  
  Serial.print("MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.print(mymac[i], HEX);
        if (i<5)
          Serial.print(':');
    }
  Serial.println();
  
  Serial.print("Access Ethernet Controller... ");
  if (ether.begin(sizeof Ethernet::buffer, mymac, CS_PIN) == 0) {
    Serial.println(F("FAILED"));
  } else {
    Serial.println("DONE");
  }
  
  
  Serial.print("Requesting IP from DHCP Server... ");
  if (!ether.dhcpSetup()) {
    Serial.println(F("FAILED"));
  } else {
    Serial.println("DONE");
  }
  
  
  ether.printIp("IP:  ", ether.myip);
  ether.printIp("GW:  ", ether.gwip);  
  ether.printIp("DNS: ", ether.dnsip);  

  if (!ether.dnsLookup(website))
    Serial.println("DNS failed");
    
  ether.printIp("SRV: ", ether.hisip);
}



void loop () {
  
  ether.packetLoop(ether.packetReceive());
  if (millis() > timer) {
    timer = millis() + 10000;
    Serial.println();
    Serial.print("<<< REQ ");
    
    
    
    char tosend[100];
    sprintf(tosend, "?millis=%d&mac=%02X:%02X:%02X:%02X:%02X:%02X", timer, mymac[0], mymac[1], mymac[2], mymac[3], mymac[4], mymac[5]);
    
    char str_temp[6];
    
    
    float celsius = readtemperature(ds);
    if (celsius>-200) {
      dtostrf(celsius, 4, 2, str_temp);
      sprintf(tosend,"%s&t1=%s", tosend, str_temp);
    }  
    celsius = readtemperature(ds2);
    if (celsius>-200) {
      dtostrf(celsius, 4, 2, str_temp);
      sprintf(tosend,"%s&t2=%s", tosend, str_temp);
    }  
    celsius = readtemperature(ds3);
    if (celsius>-200) {
      dtostrf(celsius, 4, 2, str_temp);
      sprintf(tosend,"%s&t3=%s", tosend, str_temp);
    }  
    celsius = readtemperature(ds4);
    if (celsius>-200) {
      dtostrf(celsius, 4, 2, str_temp);
      sprintf(tosend,"%s&t4=%s", tosend, str_temp);
    }  
    
  
    
    
    ether.browseUrl(PSTR("/temperatures.php"), tosend, website, my_callback);
    
    Serial.print("/temperatures.php");
    Serial.write(tosend);
    
  }
}

float readtemperature(OneWire ds) {

  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  float celsius, fahrenheit;
  
  if ( !ds.search(addr)) {
    Serial.println("No more addresses.");
    Serial.println();
    ds.reset_search();
    delay(250);
    return -200;
  }
  
  Serial.print("ROM =");
  for( i = 0; i < 8; i++) {
    Serial.write(' ');
    Serial.print(addr[i], HEX);
  }

  if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      return -200;
  }
  Serial.println();
 
  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      Serial.println("  Chip = DS18S20");  // or old DS1820
      type_s = 1;
      break;
    case 0x28:
      Serial.println("  Chip = DS18B20");
      type_s = 0;
      break;
    case 0x22:
      Serial.println("  Chip = DS1822");
      type_s = 0;
      break;
    default:
      Serial.println("Device is not a DS18x20 family device.");
      return -200;
  } 

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end
  
  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);         // Read Scratchpad

  Serial.print("  Data = ");
  Serial.print(present, HEX);
  Serial.print(" ");
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.print(" CRC=");
  Serial.print(OneWire::crc8(data, 8), HEX);
  Serial.println();

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  celsius = (float)raw / 16.0;
  fahrenheit = celsius * 1.8 + 32.0;
  Serial.print("  Temperature = ");
  Serial.print(celsius);
  Serial.print(" Celsius, ");
  Serial.print(fahrenheit);
  Serial.println(" Fahrenheit");
  
  return celsius;

}

