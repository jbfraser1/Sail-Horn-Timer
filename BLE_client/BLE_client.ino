/**
 * A BLE client example that is rich in capabilities.
 * There is a lot new capabilities implemented.
 * author unknown
 * updated by chegewara
 */

#include "BLEDevice.h"
//#include "BLEScan.h"
#include <stdlib.h>
#include <TM1637TinyDisplay.h>


#define GOBUTTON_PIN 36
#define DISP_CLK 4
#define DISP_DIO 5

bool isGoButtonPressed = false;
unsigned long lastGoButtonPress;
unsigned long lastGoButtonDown;
int displayedSeconds;
int remainingSeconds;

// The remote service we wish to connect to.
static BLEUUID serviceUUID("6dd77192-0d0c-49c4-a6ab-4d2c4eece29f");
// The characteristic of the remote service we are interested in.
static BLEUUID   timeCharacteristicUUID("6dd77193-0d0c-49c4-a6ab-4d2c4eece29f");
static BLEUUID   buttonCharacteristicUUID("6dd77194-0d0c-49c4-a6ab-4d2c4eece29f");

static boolean foundServer = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pTimeCharacteristic;
static BLERemoteCharacteristic* pButtonCharacteristic;
static BLEAdvertisedDevice* myDevice;


const int brightness = BRIGHT_HIGH;
//int brightness = BRIGHT_7;
//int brightness = BRIGHT_0;

TM1637TinyDisplay display(DISP_CLK, DISP_DIO);

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {

    
    remainingSeconds = pData[1]*256 + pData[0];

        Serial.print("Notify callback for characteristic ");
    Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.print(" of data length ");
    Serial.println(length);
    Serial.print("data: ");
    Serial.println(remainingSeconds);
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("onDisconnect");
  }
};

bool connectToServer() {
    Serial.print("Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());
    
    BLEClient*  pClient  = BLEDevice::createClient();
    Serial.println(" - Created client");

    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remove BLE Server.
    pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
    Serial.println(" - Connected to server");

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find our service UUID: ");
      Serial.println(serviceUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our service");


    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pTimeCharacteristic = pRemoteService->getCharacteristic(timeCharacteristicUUID);
    pButtonCharacteristic = pRemoteService->getCharacteristic(buttonCharacteristicUUID);
    if (pTimeCharacteristic == nullptr) {
      Serial.print("Failed to find time characteristic UUID: ");
      Serial.println(timeCharacteristicUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found time characteristic");

    if (pButtonCharacteristic == nullptr) {
      Serial.print("Failed to find button characteristic UUID: ");
      Serial.println(buttonCharacteristicUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found button characteristic");

    // Read the value of the characteristic.
    if(pTimeCharacteristic->canRead()) {

      uint8_t* pData = (uint8_t*)pTimeCharacteristic->readValue().c_str();
      Serial.print("The time characteristic value was: ");
      Serial.print(pData[1]*256 + pData[0]);
      Serial.println();
    }

    if(pTimeCharacteristic->canNotify())
      pTimeCharacteristic->registerForNotify(notifyCallback);

    connected = true;
    return true;
}
/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {

      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      foundServer = true;
      doScan = true;

    } // Found our server
  } // onResult
}; // MyAdvertisedDeviceCallbacks

void goButtonPress(){
    Serial.println("Go button pressed.");
    
    pButtonCharacteristic->writeValue(1, 1);
}

void checkGoButton(){

  // With move to BLE, need to  remove some of the debounce logic from this.
  if(digitalRead(GOBUTTON_PIN) == HIGH){
    if( !isGoButtonPressed) {
      isGoButtonPressed = true;
      if(lastGoButtonPress < (millis() - 20)
        && lastGoButtonDown < (millis() - 20)){
          lastGoButtonPress = millis();
          goButtonPress();
      }
    }
    lastGoButtonDown = millis();
  } else {
    if(lastGoButtonPress < (millis() - 20)
      && lastGoButtonDown < (millis() - 20)){
        isGoButtonPressed = false;
    }
  }
}


void writeTime(bool force){
  if(force || displayedSeconds != getTotalSeconds()){ 
    displayedSeconds = getTotalSeconds();
    display.showString(" ",1,0);
    display.showNumberDec( getMinutes(), 0b11100000, false, 1, 1);
    display.showNumber( getSeconds(), true, 2, 2);
  }
}


int getMinutes(){
  return getTotalSeconds() / 60;
}

int getSeconds(){
  return getTotalSeconds() % 60;
}

int getTotalSeconds(){
  // we expect the timer to sound when zero is first display, not at the end of zero.
  return remainingSeconds;
}


void setup() {
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");
  BLEDevice::init("");


  display.clear();
  display.setBrightness(brightness);


  pinMode(GOBUTTON_PIN, INPUT);
  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 5 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);
} // End of setup.


// This is the Arduino main loop function.
void loop() {

  // If the flag "foundServer" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are 
  // connected we set the connected flag to be true.
  if (foundServer == true) {
    if (connectToServer()) {
      Serial.println("We are now connected to the BLE Server.");
    } else {
      Serial.println("We have failed to connect to the server; there is nothin more we will do.");
    }
    foundServer = false;
  }

  // If we are connected to a peer BLE Server, update the characteristic each time we are reached
  // with the current time since boot.
  if (connected) {
    //String newValue = "Time since boot: " + String(millis()/1000);
    //Serial.println("Setting new characteristic value to \"" + newValue + "\"");
    
    // Set the characteristic's value to be the array of bytes that is actually a string.
    //pRemoteCharacteristic->writeValue(newValue.c_str(), newValue.length());
  } else if(doScan){
    BLEDevice::getScan()->start(0);  // this is just eample to start scan after disconnect, most likely there is better way to do it in arduino
  }
  checkGoButton();
  
  writeTime(false);

  
  //delay(1000); // Delay a second between loops.
}