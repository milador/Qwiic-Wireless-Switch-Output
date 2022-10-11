/** ************************************************************************
 * File Name: Qwiic_Wireless_Switch_Output_Software.ino 
 * Title: Qwiic Wireless Switch Output Software
 * Developed by: Milador
 * Version Number: 1.0 (24/6/2022)
 * Github Link: https://github.com/milador/Qwiic-Wireless-Switch-Output
 ***************************************************************************/

#include <Wire.h>
#include "SparkFun_TCA9534.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <M5StickCPlus.h>


#define OUTPUT_ADDR          0x26      //TCA9534 output switch module I2C Address 
#define NUM_SWITCH           2         //Switch A and B
#define TO_SLEEP_TIME        60        //Go to sleep if no data was sent in 60 seconds
#define TO_WAKE_TIME         180       //Wake up every 3 minutes 
#define S_TO_MS_FACTOR       1000
#define US_TO_S_FACTOR       1000000
#define SLEEP_MODE_ENABLED   true      //Set to false to disable sleep mode

TCA9534 outputModule;                  //Output 

// Set these to your desired credentials.
const char *ssid = "SwitchOutput";
const char *password = "12345678";

String g_switchMessage;        //Custom message 
int g_pageNumber;
unsigned long currentTime = 0, switchDataTime = 0;

bool g_outputPinMode[NUM_SWITCH] = {GPIO_OUT, GPIO_OUT};

String g_switchConnectionMessage;        //Qwiic modules connection state message 
int g_moduleConnectionState;

WiFiServer server(80);


void setup() {

  Serial.begin(115200);
  M5.begin();                                                                  //Starts M5Stack
  Serial.println();
  Serial.println("Configuring access point...");

  // You can remove the password parameter if you want the AP to be open.
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  server.begin();
  Serial.println("Server started");
  showIntro();                                                                 //Show intro page
  delay(5);   
  busExpandSetup();                                                            //Setup bus expander 
  delay(5);   
  showMode(myIP);
  delay(5000);
  initBatterySaver();
}

void loop() {

  if(SLEEP_MODE_ENABLED) { batterySaver(); } // Sleep mode          
  WiFiClient client = server.available();   // listen for incoming clients

  if (client) {                             // if you get a client,
    Serial.println("New Client.");          // print a message out the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        if (c == '\n') {                    // if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();
            client.print("<head><style>.center {display: flex; justify-content: center; align-items: center; height: 200px; border: 3px solid blue; }</style></head>");
            // the content of the HTTP response follows the header:
            //client.print("Click <a href=\"/A\">here</a> to activate Switch A.<br>");
            //client.print("Click <a href=\"/B\">here</a> to activate Switch B.<br>");
            client.print("<h2 style=\"text-align:center\">Qwiic Wireless Switch Output</h2><br>");
            client.print("<div class=\"center\"><button onclick=\"location.href='/A'\" style=\"height:200px;width:200px\" type=\"button\">Switch A</button>");
            client.print("<button onclick=\"location.href='/B'\" style=\"height:200px;width:200px\" type=\"button\">Switch B</button></div><br>");
            client.print("<p style=\"text-align:center\">More information at milador.ca</p><br>");

            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          } else {    // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }

        // Check to see if the client request was "GET /A" or "GET /B":
        if (currentLine.endsWith("GET /A")) {
          outputModule.digitalWrite(0, HIGH);
          delay(100);
          outputModule.digitalWrite(0, LOW);
        }
        if (currentLine.endsWith("GET /B")) {
          outputModule.digitalWrite(1, HIGH);
          delay(100);
          outputModule.digitalWrite(1, LOW);
        }
        switchDataTime = millis();  
      }
    }
    // close the connection:
    client.stop();
    Serial.println("Client Disconnected.");
  }
}


//***INITIALIZE BATTERY SAVER FUNCTION***//
void initBatterySaver() {
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_37,LOW);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_39,LOW);
  currentTime = switchDataTime = millis();
}

//***BATTERY SAVER FUNCTION***//
void batterySaver() {
  currentTime = millis();
  if (currentTime > (switchDataTime + (TO_SLEEP_TIME * S_TO_MS_FACTOR))) {
    M5.Axp.DeepSleep(SLEEP_SEC(TO_WAKE_TIME));
  }
}

//*** SHOW INTRO PAGE***//
void showIntro() {

  g_pageNumber = 0;                                  //Set intro page number 

  M5.Lcd.setRotation(3);

  M5.Lcd.fillScreen(WHITE);
  delay(200);
  M5.Lcd.fillScreen(RED);
  delay(200);
  M5.Lcd.fillScreen(GREEN);
  delay(200);
  M5.Lcd.fillScreen(BLUE);
  delay(200);
  M5.Lcd.fillScreen(BLACK);
  
  M5.Lcd.setTextColor(WHITE);

  M5.Lcd.setTextSize(2);                        
  M5.Lcd.drawCentreString("Milador.ca",130,40,2);

  M5.Lcd.setTextSize(1);
  M5.Lcd.drawCentreString("Qwiic Wireless Switch Output",130,80,1);

  delay(3000);
}

//*** SHOW CONNECTION STATE***//
void showConnection(int connectionState) {

  M5.Lcd.setRotation(3);
  M5.Lcd.setTextSize(1);                    // Select the font
  //Display connection status based on code
  if(connectionState==0){
    M5.Lcd.drawRect(2, 100,238, 30, RED);
    M5.Lcd.setTextColor(RED); 
    g_switchConnectionMessage = "Error: I2C not detected";
    Serial.println(g_switchConnectionMessage);
    M5.Lcd.drawCentreString(g_switchConnectionMessage,120,105,2);// Display connection state
  } else if(connectionState==1){
    M5.Lcd.drawRect(2, 100,238, 30, BLUE);
    M5.Lcd.setTextColor(BLUE); 
    g_switchConnectionMessage = "Success: I2C is detected";
    Serial.println(g_switchConnectionMessage);
    M5.Lcd.drawCentreString(g_switchConnectionMessage,120,105,2);// Display connection state
  }
    
}

//*** SHOW MODE PAGE***//
void showMode(IPAddress ip){

  g_pageNumber = 1;

  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);                      //Black background
  M5.Lcd.drawRect(2, 2, 238, 30, BLUE);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(1);
  M5.Lcd.drawCentreString(ipToString(ip),120,5,4);
  
  showModeInfo();
  showConnection(g_moduleConnectionState);
}

String ipToString(IPAddress ip){
  String s="";
  for (int i=0; i<4; i++)
    s += i  ? "." + String(ip[i]) : String(ip[i]);
  return s;
}

//*** SHOW MODE INFO***//
void showModeInfo() {

  String switchAText = "Swich A";
  String switchBText = "Swich B";
  M5.Lcd.setRotation(3);
  M5.Lcd.drawRect(2, 35, 238, 62, WHITE);
  M5.Lcd.setTextColor(WHITE); 
  M5.Lcd.setTextSize(1);

  M5.Lcd.drawCentreString(switchAText,120,44,2);
  M5.Lcd.drawCentreString(switchBText,120,67,2);
}

//***BUS EXPANDER SETUP FUNCTION***//
void busExpandSetup() {
  //Initialize the switch pins as outputs
  Wire.begin();
  while (1) { //Output module is successfully detected 
    if (outputModule.begin(Wire,OUTPUT_ADDR) == true ) {
      g_switchConnectionMessage = "Success: I2C is detected";
      Serial.println(g_switchConnectionMessage);
      g_moduleConnectionState = 1;
      break;
    } else {
      g_switchConnectionMessage = "Success: I2C not detected";
      Serial.println(g_switchConnectionMessage);
      g_moduleConnectionState = 0;
      delay(3000);
    }
  }
  outputModule.pinMode(g_outputPinMode);
}
