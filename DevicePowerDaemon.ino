#define BUFF_SIZE 128
#define VERBOSE true

#define MINIMUM_FIRMWARE_VERSION "0.6.6"
#define MODE_LED_BEHAVIOUR "MODE"

#define MIN_POWER_LEVEL 0
#define MAX_POWER_LEVEL 100

#define BLUEFRUIT_SPI_CS   8
#define BLUEFRUIT_SPI_IRQ  7
#define BLUEFRUIT_SPI_RST  4

#define POWER_LEVEL_1_PIN   5
#define POWER_LEVEL_2_PIN   6
#define POWER_LEVEL_3_PIN   9
#define POWER_LEVEL_4_PIN  10
#define POWER_LEVEL_5_PIN  11
#define POWER_LEVEL_PIN_NUM 5

#define TARGET_RSSI -60
#define WORST_RSSI -80
#define BEST_RSSI -40

#define BLE_CMD_OK "OK"
#define BLE_CMD_DELAY 50

#define ERR_BLE_INIT_FAIL 1

#define BUILTIN_LED 13

//#include "Adaruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"

Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

int current_power_level = 0;
/*T I M E R S*/
int level_send_timer = 0;
int adjust_tx_power_timer = 0;
/*************/
bool hasPrintedConnectionSuccess = false;

int powerLevelIndicator[POWER_LEVEL_PIN_NUM] = {POWER_LEVEL_1_PIN,
                                                POWER_LEVEL_2_PIN,
                                                POWER_LEVEL_3_PIN,
                                                POWER_LEVEL_4_PIN,
                                                POWER_LEVEL_5_PIN
                                                };

void setup() {
  //wait for Serial terminal to open on the host
  while(!Serial);
  
  //setup indicator lights and ground them
  for(int i = 0; i < POWER_LEVEL_PIN_NUM; i++) {
    pinMode(powerLevelIndicator[i], OUTPUT);
    analogWrite(powerLevelIndicator[i], 0);
  }

  //start printing to serial and display some status info
  Serial.begin(115200);
  Serial.println(F("Device Control Device Power Debug Interface"));
  Serial.println(F("-------------------------------------------"));
  if(!ble.begin()) {
    Serial.println(F("[ ERR ] Could not init BLE interface"));
    blink_error(ERR_BLE_INIT_FAIL);
  } else {
    Serial.println(F("[ OK  ] BLE interface initialized"));
    setTXPower(-4);
    int txpower = getTXPower();
    Serial.print("Transmitting at ");
    Serial.print(txpower);
    Serial.println(" dBm");
  }

  ble.echo(false);
  ble.verbose(false);
}

void loop() {
  //wait for a connection to the bluetooth device
  bool hasPrintedConnectionError = false;
  while(!ble.isConnected()){
    hasPrintedConnectionSuccess = false;
    if(!hasPrintedConnectionError) {
      Serial.println(F("Waiting for BLE connection"));
      hasPrintedConnectionError = true;
    }
    delay(250);
  }
  hasPrintedConnectionError = false;

  //light up the connection status LED
  if(ble.isVersionAtLeast(MINIMUM_FIRMWARE_VERSION)) {
    if(!hasPrintedConnectionSuccess) {
      Serial.print(F("Connected to device\n"));
      int signalStrength = getRSSI();
      if(signalStrength != -1) {
        Serial.print(F("Signal strength is "));
        Serial.print(signalStrength);
        Serial.println(F(" dBm"));
      }
      hasPrintedConnectionSuccess = true;
      /*//check power level every 2 minutes
      if(adjust_tx_power_timer == 0) {
        Serial.println(F("Adjusting TX Power"));
        adjustTXPower();
        int txPower = getTXPower();
        int currentRSSI = getRSSI();
        Serial.print(F("TX power is now "));
        Serial.println(txPower);
        Serial.print(F("New RSSI is "));
        Serial.println(currentRSSI);
      }
      adjust_tx_power_timer += 1;
      adjust_tx_power_timer %= 1200;
      */
    }
    ble.sendCommandCheckOK("AT+HWModeLED=" MODE_LED_BEHAVIOUR);
  }
  
  //check for power level set request
  ble.println("AT+BLEUARTRX");
  ble.readline();
  if(strncmp(ble.buffer, "SET", 3) == 0) {
    int size_of_buffer = strlen(ble.buffer);
    char num_part[size_of_buffer - 4];
    for(int i = 4; i < size_of_buffer; i++) {
      num_part[i - 4] = ble.buffer[i];
    }
    int power_level_request = atoi(num_part);
    if(strcmp(num_part, "0") != 0 && power_level_request == 0) {
      Serial.print(F("ERROR: cannot set power level to "));
      Serial.print(num_part);
      Serial.println();
    } else {
      bool success = set_device_power_level(power_level_request);
      if(success) {
        Serial.print("Changed power level to ");
        Serial.print(power_level_request);
        Serial.println();
        ble.print("AT+BLEUARTTX=");
        ble.print("OK ");
        ble.println(current_power_level);
      }
      else {
        ble.print("AT+BLEUARTTX=");
        ble.println("FAIL");
      }
    }
  } else if(strncmp(ble.buffer, "RQT", 3) == 0) {
      Serial.println(F("Got power request"));
      ble.print("AT+BLEUARTTX=");
      ble.println(current_power_level);
  } else if(strcmp(BLE_CMD_OK, ble.buffer) != 0) {
      Serial.print(F("Unknown command "));
      Serial.println(ble.buffer);
  }
  
  delay(100);

}

//checks if power level is valid (in range). if it is valid then
//the global power level is adjusted and the lights on the board
//are lit accordingly
//returns true if power set successful, false otherwise
bool set_device_power_level(int p) {
  if(p > MAX_POWER_LEVEL || p < MIN_POWER_LEVEL) { 
    Serial.print(F("Refusing to set power level "));
    Serial.print(p);
    Serial.print(F(" outside of range "));
    Serial.print(MIN_POWER_LEVEL);
    Serial.print(" - ");
    Serial.print(MAX_POWER_LEVEL);
    Serial.println();
    return false;
  }
  current_power_level = p;
  int stepSize = MAX_POWER_LEVEL / POWER_LEVEL_PIN_NUM;
  int highest_full_light = p / stepSize;
  int partial_power_level = p % stepSize;
  partial_power_level = map(partial_power_level, 0, stepSize, 0, 255);
  for(int i = 1; i <= POWER_LEVEL_PIN_NUM; i++) {
    if(i <= highest_full_light) {
      analogWrite(powerLevelIndicator[i-1], 255);
    } else if(i == highest_full_light + 1){
      analogWrite(powerLevelIndicator[i-1], partial_power_level);
    } else {
      analogWrite(powerLevelIndicator[i-1], 0);
    }
  }
  return true;
}

//copy name of connected device into parameter
//this does not work and is not important
void getConnectedDeviceName(char name[]){
  if(ble.isConnected()){
    ble.print("AT+BLEGETPEERADDR");
    delay(BLE_CMD_DELAY);
    ble.readline();
    strncpy(name, ble.buffer, 18);
  }
}

//returns the received signal strength indicator of the device
int getRSSI() {
  if(ble.isConnected()){
    ble.println("AT+BLEGETRSSI");
    //delay a little so the BLE has time to respond
    delay(BLE_CMD_DELAY);
    ble.readline();
    int level = atoi(ble.buffer);
    return level;
  }
  return -1;
}

//return the power that we are transmitting at in dBm
int getTXPower() {
  ble.println("AT+BLEPOWERLEVEL");
  //delay a little so the BLE has time to respond
  delay(50);
  ble.readline();
  int level = atoi(ble.buffer);
  return level;
}

//checks for valid TX powers. this is device specific
bool isValidTXPower(int p) {
  switch(p) {
    case -40:
    case -20:
    case -16:
    case -12:
    case  -8:
    case  -4:
    case   0:
    case   4:
        return true;
        break;
    default:
      return false;
  }
}

//modify the transmit power of the device, return true if successful
bool setTXPower(int p) {
  if(isValidTXPower(p)) {
    ble.print("AT+BLEPOWERLEVEL=");
    ble.println(p);
    delay(50);
    ble.readline();
    if(strcmp("OK", ble.buffer) == 0){
      return true;
    }
  }
  return false;
}

//automatically scale TX power based on RSSI
void adjustTXPower(){
  int currentRSSI = getRSSI();
  int txPower = getTXPower();
  if(TARGET_RSSI - currentRSSI > 10){
    bool set_success = false;
    while(!set_success && txPower < 4 && TARGET_RSSI - currentRSSI > 10) {
      set_success = setTXPower(txPower + 4);
      txPower += 4;
      delay(50);
      currentRSSI = getRSSI();
    }
    Serial.print(F("Adjusted TX power to "));
    Serial.println(txPower);
  }
}


///blink the builtin LED to indicate catastrophic failure
void blink_error(int err_code) {
  if(err_code == ERR_BLE_INIT_FAIL) {
    blink_three_burst();
  }
}

//helper function for blink_error()
void blink_three_burst() {
  digitalWrite(13, LOW);
  Serial.println(F("ERROR -- EXECUTION HALT"));
  while(1){
    for(int i = 1; i <= 3; i++) {
      digitalWrite(13, HIGH);
      delay(250);
      digitalWrite(13,LOW);
      delay(500);
    }
    delay(1500);
  }
}
