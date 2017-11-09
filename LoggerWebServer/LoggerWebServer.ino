/*
  WiFi Web Server LED Blink

  A simple web server that lets you blink an LED via the web.
  This sketch will create a new access point (with no password).
  It will then launch a new server and print out the IP address
  to the Serial monitor. From there, you can open that address in a web browser
  to turn on and off the LED on pin 13.

  If the IP address of your shield is yourAddress:
    http://yourAddress/H turns the LED on
    http://yourAddress/L turns it off

  created 25 Nov 2012
  by Tom Igoe
  adapted to WiFi AP by Adafruit
 */

#include <SPI.h>
#include <WiFi101.h>
#include <Adafruit_SPIFlash.h>
#include <Adafruit_SPIFlash_FatFs.h>
#include <RTCZero.h>
#include <elapsedMillis.h>
#include <string.h>

#include "arduino_secrets.h"

#define FLASH_TYPE     SPIFLASHTYPE_W25Q16BV  // Flash chip type.
                                              // If you change this be
                                              // sure to change the fatfs
                                              // object type below to match.

#define FLASH_SS       SS1                    // Flash chip SS pin.
#define FLASH_SPI_PORT SPI1                   // What SPI port is Flash on?

#define FILE_NAME      "data.csv"

#define READ_BUFFER_SIZE 1024
char read_buffer[READ_BUFFER_SIZE];

Adafruit_SPIFlash flash(FLASH_SS, &FLASH_SPI_PORT);     // Use hardware SPI 

// Alternatively you can define and use non-SPI pins!
//Adafruit_SPIFlash flash(SCK1, MISO1, MOSI1, FLASH_SS);

// Finally create an Adafruit_M0_Express_CircuitPython object which gives
// an SD card-like interface to interacting with files stored in CircuitPython's
// flash filesystem.
Adafruit_W25Q16BV_FatFs fatfs(flash);

//RTC config
RTCZero rtc;
int AlarmTime;

///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;                // your network key Index number (needed only for WEP)

int led =  LED_BUILTIN;
int status = WL_IDLE_STATUS;
int wifiEnablePin=11;

WiFiServer server(80);


#define ACCESS_POINT_TIMEOUT_MILLIS 120000
#define LOGGING_SLEEP_TIME 5 // in seconds
int dataIndex = 0;
bool logging = false;
bool access_point_connected = false;
elapsedMillis since_access_point_disconnect_millis;

void setup() {
  rtc.begin();

  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  //while (!Serial) {
  //  ; // wait for serial port to connect. Needed for native USB port only
  //}
  
  // Initialize flash library and check its chip ID.
  if (!flash.begin(FLASH_TYPE)) {
    Serial.println(F("Error, failed to initialize flash chip!"));
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(200);                        // wait for a second
    digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
    delay(200);
    while(1);
  }
  if (!fatfs.begin()) {
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(200);                       // wait for a second
    digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
    delay(200);
    while(1);
  }
  

  Serial.println(F("Access Point Web Server"));
  pinMode(led, OUTPUT);      // set the LED pin mode
  
  WiFi.setPins(6,10,9,wifiEnablePin);   //CS, IRQ,  RST, EN

  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println(F("WiFi shield not present"));
    // don't continue
    while (true);
  }

  // by default the local IP address of will be 192.168.1.1
  // you can override it with the following:
  // WiFi.config(IPAddress(10, 0, 0, 1));

  // print the network name (SSID);
  Serial.print(F("Creating access point named: "));
  Serial.println(ssid);

  // Create open network. Change this line if you want to create an WEP network:
  status = WiFi.beginAP(ssid);
  if (status != WL_AP_LISTENING) {
    Serial.println(F("Creating access point failed"));
    // don't continue
    while (true);
  }

  // wait 1 second for connection:
  delay(1000);

  // start the web server on port 80
  server.begin();

  // you're connected now, so print out the status
  printWiFiStatus();
}


void loop() {
  //----------------------------------------------------------------------------
  if (logging) {
    // do the logging procedure
    delay(50);
   
    File data = fatfs.open(FILE_NAME, FILE_WRITE);
    delay(100);
    data.println(dataIndex);
    delay(100);
    data.close();
    delay(100);
    dataIndex++;


    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(50);                         // wait a bit
    digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
    delay(50);  

   //delay(SLEEP_TIME*1000);

    AlarmTime += LOGGING_SLEEP_TIME; // Adds 5 seconds to alarm time
    AlarmTime = AlarmTime % 60; // checks for roll over 60 seconds and corrects
    rtc.setAlarmSeconds(AlarmTime); // Wakes at next alarm time, i.e. every 5 secs

    rtc.enableAlarm(rtc.MATCH_SS);   // Match seconds only
    rtc.attachInterrupt(alarmMatch); // Attach function to interupt
    rtc.standbyMode();               // Sleep until next alarm match
    
  }
  //----------------------------------------------------------------------------
  // Access Point is active
  else{
    // compare the previous status to the current status
    if (status != WiFi.status()) {
      // it has changed update the variable
      status = WiFi.status();

      if (status == WL_AP_CONNECTED) {
        access_point_connected = true;
        
        byte remoteMac[6];
        // a device has connected to the AP
        Serial.print(F("Device connected to AP, MAC address: "));
        WiFi.APClientMacAddress(remoteMac);
        Serial.print(remoteMac[5], HEX);
        Serial.print(":");
        Serial.print(remoteMac[4], HEX);
        Serial.print(":");
        Serial.print(remoteMac[3], HEX);
        Serial.print(":");
        Serial.print(remoteMac[2], HEX);
        Serial.print(":");
        Serial.print(remoteMac[1], HEX);
        Serial.print(":");
        Serial.println(remoteMac[0], HEX);
      } else {
        // a device has disconnected from the AP, and we are back in listening mode
        Serial.println(F("Device disconnected from AP"));
        access_point_connected = false;
        since_access_point_disconnect_millis = 0;
      }
    }
    //--------------------------------------------------------------------------
    if (access_point_connected){
      WiFiClient client = server.available();   // listen for incoming clients
      if (client) {                             // if you get a client,
        Serial.println(F("new client:"));    // print a message out the serial port
        char request_buffer[1024];           // make a buffer to hold the request
        //pause a bit to allow buffers to fill
        delay(100);
        int nbytes = min(client.available(),READ_BUFFER_SIZE - 1);  //leave room for null terminator
        nbytes = client.readBytesUntil('\n', read_buffer, nbytes);  //read into buffer
        read_buffer[nbytes] = 0;             //add null terminator
        strcpy(request_buffer, read_buffer); //save the request line
        Serial.print(F("  request: "));Serial.println(request_buffer);
        //readout and discard headers
        Serial.println(F("  headers:"));
        while(true) {
          nbytes = min(client.available(),READ_BUFFER_SIZE - 1);      //leave room for null terminator
          if (nbytes == 0){ break;}
          nbytes = client.readBytesUntil('\n', read_buffer, nbytes);  //read into buffer
          read_buffer[nbytes] = 0; //add null terminator
          Serial.print(F("    "));Serial.println(read_buffer);
        }
        //parse the request
        char *p;
        //  1st strtok iteration
        p = strtok(request_buffer," ");
        if (strcmp(p,"GET") == 0){
          //  2nd strtok iteration
          p = strtok(NULL," ");
          if (strcmp(p,"/") == 0){
            Serial.println(F("displaying HTML for path '/': "));
            displayHTML_MAIN(client);
          } else if (strcmp(p,"/DATA") == 0){
            fetchDATA_CSV(client);
          } else if (strcmp(p,"/SET_TIME") == 0){
            Serial.println(F("WARNING: path 'SET_TIME' not yet implemented!"));
            displayHTML_MAIN(client);
          }
          else{
            Serial.print(F("WARNING: invalid path: "));Serial.println(p);
          }
        } else{
          Serial.print(F("WARNING: unhandled HTTP request command: "));Serial.println(p);
        }
        // close the connection:
        client.stop();
        Serial.println(F("client disconnected"));
      }
    }
    //--------------------------------------------------------------------------
    // no device connected yet
    else{
      if (since_access_point_disconnect_millis > ACCESS_POINT_TIMEOUT_MILLIS){
        //shutdown the Access Point and go into logging mode
        Serial.println(F("Shutting down Acccess Point and going into low power logging mode"));
        WiFi.end();
        logging = true;
      }
    }
  }
  // end Access Point
  //----------------------------------------------------------------------------
}

void printWiFiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print(F("SSID: "));
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print(F("IP Address: "));
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print(F("signal strength (RSSI):"));
  Serial.print(rssi);
  Serial.println(F(" dBm"));
  // print where to go in a browser:
  Serial.print(F("To see this page in action, open a browser to http://"));
  Serial.println(ip);

}

String fetchRTCTimeStamp(){
  String ts = "";
  ts += rtc.getYear();
  ts += "-";
  ts += rtc.getMonth();
  ts += "-";
  ts += rtc.getDay();
  ts += " ";
  ts += rtc.getHours();
  ts += ":";
  ts += rtc.getMinutes();
  ts += ":";
  ts += rtc.getSeconds();
  return ts;
}

void displayHTML_MAIN(WiFiClient client){
  //display the page
  // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)*/
  // and a content-type so the client knows what's coming, then a blank line:
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-type:text/html"));
  client.println();
  // the content of the HTTP response follows the header:
  client.print(F("<!doctype html>\n<html lang=\"en-US\">\n<head>\n"));
  client.print(F("<div>RTC Time: "));
  client.print(fetchRTCTimeStamp());
  client.print(F("&nbsp;<span><a href=\"/SET_TIME\">SET TIME</a></span></div>\n"));
  client.print(F("<div><a href=\"/DATA\">FETCH DATA</a></div>\n"));
  client.print(F("</body>\n</html>\n"));
}

void fetchDATA_CSV(WiFiClient client){
  Serial.println(F("Fetching data."));
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-type:text/plain"));
  client.println();
  File dataFile = fatfs.open(FILE_NAME, FILE_READ);
  if (dataFile) {
    // File was opened, now print out data character by character until at the
    // end of the file.
    Serial.println(F("Opened file, printing contents below:"));
    int nbytes     = min(dataFile.available(),READ_BUFFER_SIZE - 1); //leave room for null terminator
    while (nbytes) {
      // Use the read function to read into buffer
      dataFile.read(read_buffer, nbytes);
      // append a null terminator for string printing
      read_buffer[nbytes] = 0;
      client.print(read_buffer);
      nbytes     = min(dataFile.available(),READ_BUFFER_SIZE - 1);   //leave room for null terminator
    }
  }
  else {
    Serial.println(F("Failed to open data file! Does it exist?"));
    client.print(F("ERROR: failed to open data file\n"));
  }
}

void alarmMatch() // Do something when interrupt called
{
  Serial.println(F("RTC Alarm Match"));
}
