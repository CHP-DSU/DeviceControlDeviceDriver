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

#define ERR_BLE_INIT_FAIL 1

//#include "Adaruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"

Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

int current_power_level = 0;
int level_send_timer = 0;
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

  Serial.begin(115200);
  Serial.println(F("Device Control Device Power Debug Interface"));
  Serial.println(F("-------------------------------------------"));
  if(!ble.begin()) {
    Serial.println(F("[ ERR ] Could not init BLE interface"));
    blink_error(ERR_BLE_INIT_FAIL);
  } else {
    Serial.println(F("[ OK  ] BLE interface initialized"));
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
    delay(500);
  }
  hasPrintedConnectionError = false;

  //light up the connection status LED
  if(ble.isVersionAtLeast(MINIMUM_FIRMWARE_VERSION)) {
    if(!hasPrintedConnectionSuccess) {
      Serial.println(F("Connected to device"));
      hasPrintedConnectionSuccess = true;
    }
    ble.sendCommandCheckOK("AT+HWModeLED=" MODE_LED_BEHAVIOUR);
  }
  
  //check for power level set request
  ble.println("AT+BLEUARTRX");
  ble.readline();
  if(strcmp(ble.buffer, "OK") != 0) {
    int power_level_request = atoi(ble.buffer);
    if(strcmp(ble.buffer, "0") != 0 && power_level_request == 0) {
      Serial.print("ERROR: cannot set power level to ");
      Serial.print(ble.buffer);
      Serial.println();
    } else {
      bool success = set_device_power_level(power_level_request);
      if(success) {
        Serial.print("Changed power level to ");
        Serial.print(power_level_request);
        Serial.println();
      }
    }
  }

  //send power usage
  if(level_send_timer == 0) {
    ble.print("AT+BLEUARTTX=");
    ble.println(current_power_level);
  }

  level_send_timer += 1;
  level_send_timer %= 20;
  delay(100);

}

bool set_device_power_level(int p) {
  if(p > MAX_POWER_LEVEL || p < MIN_POWER_LEVEL) { 
    Serial.print("Refusing to set power level ");
    Serial.print(p);
    Serial.print(" outside of range ");
    Serial.print(MIN_POWER_LEVEL);
    Serial.print(" - ");
    Serial.print(MAX_POWER_LEVEL);
    Serial.println();
    return false;
  }
  current_power_level = p;
  int highest_full_light = p / (MAX_POWER_LEVEL / POWER_LEVEL_PIN_NUM);
  int partial_power_level = p % (MAX_POWER_LEVEL / POWER_LEVEL_PIN_NUM);
  partial_power_level = map(partial_power_level, 0, (MAX_POWER_LEVEL / POWER_LEVEL_PIN_NUM), 0, 255);
  for(int i = 1; i <= POWER_LEVEL_PIN_NUM; i++) {
    if(i <= highest_full_light) {
      analogWrite(powerLevelIndicator[i-1], 255);
    } else if(i == highest_full_light + 1){
      analogWrite(powerLevelIndicator[i-1], partial_power_level);
    } else {
      analogWrite(powerLevelIndicator[i], 0);
    }
  }
  return true;
}

void blink_error(int err_code) {
  if(err_code == ERR_BLE_INIT_FAIL) {
    blink_three_burst();
  }
}

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
