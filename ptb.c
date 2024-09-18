#include <RTCZero.h>
#include <NTPClient.h>
#include <TimeoutCallback.h>
#include <FlashAsEEPROM.h>
#include <FlashStorage.h>
#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include <Servo.h>
#include <INA226.h>

/*
* Revision History:
*
*  v1.0 - Initial Release - Originally written for Uno R4 WiFi but ported over to
                            Nano 33 IoT.
   v1.1 - RTC Upgrade     - Use RTCZero and NTP for charge HVTB countdown
                            Useful in system that constantly reboots
*
  Sketch generated by the Arduino IoT Cloud Thing "Untitled"
  https://create.arduino.cc/cloud/things/014449b7-8bb1-411f-b898-3d4987721fcd 

  Arduino IoT Cloud Variables description

  The following variables are automatically generated and updated when changes are made to the Thing

  String uptime_cloud;
  float bus_voltage_cloud;
  float elapsed_percent_cloud;
  bool get_uptime_cloud;
  bool pb_bv_cloud;
  bool pb_cloud;
  bool reset_cloud;

  Variables which are marked as READ/WRITE in the Cloud Thing will also have functions
  which are called when their values are changed from the Dashboard.
  These functions are generated with the Thing and added at the end of this sketch.
*/

#include "thingProperties.h"

#define HOURS_IN_A_DAY           (24)
#define SECONDS_IN_A_MINUTE      (60)
#define SECONDS_IN_A_HOUR        (3600)
#define SECONDS_IN_A_DAY         (86400)
#define MS_IN_ONE_SEC            (1000)
#define MS_IN_ONE_MINUTE         (60 * MS_IN_ONE_SEC)
#define MILLISECONDS_IN_A_DAY    (SECONDS_IN_A_DAY * MS_IN_ONE_SEC)
#define CHARGE_HVTB_RATE         (1  * MILLISECONDS_IN_A_DAY)
#define TEN_SEC_PERIODIC_RATE    (10 * MS_IN_ONE_SEC)
#define NO_WIFI_RESET_TIMEOUT    (5  * MS_IN_ONE_MINUTE)

Servo           myservo;
int             loop_count       = 0;
int             btn_press_cnt    = 0;
int             wloss_cnt        = 0;
int             resetPin         = 2;
int             ledPin           = 3;
int             servoPin         = 15;
bool            reset_countdown  = false;
unsigned long   charge_hvtb_ts   = 0;
unsigned long   ts_startup_time  = 0;
unsigned long   next_charge_ts   = 0; 
float           temp_elapsed     = 0.0f;

//Uptime timekeeping
RTCZero         rtc_zero; 
char            Uptime_Str[37]; 

int             wifiStatus = WL_IDLE_STATUS;
WiFiUDP         Udp;
NTPClient       timeClient(Udp);

//INA226 for battery voltage monitoring
INA226          INA(0x40);              //both address bits A0 and A1 are grounded

//for the periodic
void            cb_charge_hvtb();               //Triggering CHARGE HVTB
void            cb_10sec_periodic();            //Triggering 10 second periodic
void            cb_no_wifi_reset();             //NO WIFI reset
TimeoutCallback timeout0(CHARGE_HVTB_RATE,      cb_charge_hvtb);
TimeoutCallback timeout1(TEN_SEC_PERIODIC_RATE, cb_10sec_periodic);
TimeoutCallback timeout2(NO_WIFI_RESET_TIMEOUT, cb_no_wifi_reset); 

// Reserve a portion of flash memory to store an "int" variable
FlashStorage(hvtb_press_cnt_flash,  int);
FlashStorage(wifi_loss_cnt_flash,   int);
FlashStorage(charge_hvtb_ts_flash,  unsigned long);

/************************************************************************************/
/********FUNCTIONS ******************************************************************/
/************************************************************************************/

//this reset will require a GPIO to set the RESET pin on the ARDUINO
void push_reset(void) {
  Serial.println("RESET ATTEMPT");
  delay(1000);
  digitalWrite(resetPin, LOW);
  pinMode(resetPin, OUTPUT); 
}

//CHARGE HVTB
void push_charge_hvtb(void) {
  charge_hvtb_ts = timeClient.getEpochTime();
  next_charge_ts = compute_next_charge_time();
  timeout0.setDuration(next_charge_ts * MS_IN_ONE_SEC);
  timeout0.restart();
  myservo.write(47);           //X angle is less than 45 (90-X) - Dial this value in.
  delay(300);
  myservo.write(179);
  btn_press_cnt++;
  hvtb_press_cnt_flash.write(btn_press_cnt);
  charge_hvtb_ts_flash.write(charge_hvtb_ts);
  //delay(200);
}

//charge HVTB timeout
void cb_charge_hvtb() {
  push_charge_hvtb();
  loop_count++;
  calculateUptimeAndPost("[TO]");
}

//read bus voltage and update elapsed time 
void cb_10sec_periodic() {
  digitalWrite(ledPin, HIGH);
  
  timeout1.restart();

  //update RTC
  timeClient.update();
  
  //get bus voltage
  bus_voltage_cloud      =  INA.getBusVoltage_mV();

  //compute time left until charge button press
  temp_elapsed           =  timeClient.getEpochTime() - charge_hvtb_ts;
  elapsed_percent_cloud  = (temp_elapsed * 100.0f) / (float)SECONDS_IN_A_DAY;
  
  Serial.print("Uptime C: ");
  Serial.print(timeClient.getEpochTime() - ts_startup_time);
  Serial.print(" TE: ");
  Serial.print(temp_elapsed);
  Serial.print(" EPC: ");
  Serial.print(elapsed_percent_cloud);
  Serial.print(" CHTS: ");
  Serial.println(charge_hvtb_ts);

  digitalWrite(ledPin, LOW);
}

//no wifi watchdog timeout
void cb_no_wifi_reset() {
  timeout2.restart();
  
  if((WiFi.status() != WL_CONNECTED) && (reset_countdown == true)) {
    wloss_cnt++;
    wifi_loss_cnt_flash.write(wloss_cnt);
    delay(200);    //wait for flash to finish?
    push_reset();
  }
  else if(WiFi.status() != WL_CONNECTED)
  {
    reset_countdown = true;
    Serial.println("No WiFi Watchdog Countdown TRIGGERED...");
  }
  else
  {
    reset_countdown = false;
    Serial.println("No WiFi Watchdog INACTIVE...");
  }
}

void calculateUptimeAndPost(String prepend) { 
  
  sprintf(Uptime_Str, "%lu sec. [CHTS: %lu]", (timeClient.getEpochTime() - ts_startup_time), charge_hvtb_ts);

  uptime_cloud = prepend + " " + Uptime_Str;
}

unsigned long compute_next_charge_time(void) {
  signed long ret = 0;
  
  ret = SECONDS_IN_A_DAY - (timeClient.getEpochTime() - charge_hvtb_ts);
  if((ret <= 0) || (ret > SECONDS_IN_A_DAY))
  {
    ret = SECONDS_IN_A_DAY;
  }

  Serial.print("compute_next_charge_time() ret: ");
  Serial.println(ret);
    
  return (unsigned long)ret;
}

/************************************************************************************/
/**********MEAT**********************************************************************/
/************************************************************************************/
void setup() {
  int breakout = 30;

  //status LED
  digitalWrite(ledPin, LOW);
  pinMode(ledPin, OUTPUT);

  Serial.begin(115200);
  delay(1500);

  //Configure Servo to push Charge Button
  myservo.attach(servoPin);  // attaches the servo

  //configure i2c
  Wire.begin();

  //start INA driver
  if (!INA.begin() )
  {
    uptime_cloud = "[could not connect INA226 to i2c bus. Fix and Reboot]\n";
  }

  // Defined in thingProperties.h
  initProperties();

  //obtain NVM counts
  btn_press_cnt  = hvtb_press_cnt_flash.read();
  wloss_cnt      = wifi_loss_cnt_flash.read();
  charge_hvtb_ts = charge_hvtb_ts_flash.read();

  Serial.print("setup() NVM CHTS TS: ");
  Serial.println(charge_hvtb_ts);

  // Connect to Arduino IoT Cloud
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  
  /*
     The following function allows you to obtain more information
     related to the state of network and IoT Cloud connection and errors
     the higher number the more granular information you’ll get.
     The default is 0 (only errors).
     Maximum is 4
 */
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();

  while((WiFi.status() != WL_CONNECTED) && (breakout > 0))
  {
    Serial.println("WiFi not connected, retry in 1s.");
    ArduinoCloud.update();
    breakout--;
    delay(1000);
  }

  rtc_zero.begin();
  timeClient.begin();
  timeClient.update();

  ts_startup_time = timeClient.getEpochTime();
  rtc_zero.setEpoch(ts_startup_time);

  //determine the initial charge time... check if charge_hvtb_ts is zero
  if(charge_hvtb_ts <= 0) {
    //this is probably the first time we're running it after flashing (NVM is wiped)
    //so put in a dummy charge_hvtb_ts value
    Serial.print("charge_hvtb_ts == 0, setting charge_hvtb_ts to: ");
    Serial.println(ts_startup_time);
    charge_hvtb_ts_flash.write(ts_startup_time);
  }
  else {
    Serial.print("charge_hvtb_ts: ");
    Serial.println(charge_hvtb_ts);
  }
  next_charge_ts = compute_next_charge_time();

  Serial.print(" Startup Time: ");
  Serial.println(ts_startup_time);

  //shift timeout in-case of reboot
  timeout0.setDuration(next_charge_ts * MS_IN_ONE_SEC);
  timeout0.start(); 
  timeout1.start();
  timeout2.start();

  uptime_cloud = "[PTB v.1.1] PB: " + String(btn_press_cnt) + " NWI: " + String(wloss_cnt) + " Epoch: " + String(ts_startup_time) + " NC_TS: " + String(next_charge_ts) + "\n";  
}

void loop() {
  ArduinoCloud.update();
  
  // Your code here 
  timeout0.loop();
  timeout1.loop();
  timeout2.loop();
}



/************************************************************************************/
/**********CLOUD STUFF***************************************************************/
/************************************************************************************/

/*
  Since PbCloud is READ_WRITE variable, onPbCloudChange() is
  executed every time a new value is received from IoT Cloud.
*/
void onPbCloudChange()  {
  // Add your code here to act upon PbCloud change
  if(pb_cloud == true) {
    digitalWrite(ledPin, HIGH);
    push_charge_hvtb();
    calculateUptimeAndPost("[PB]");
    digitalWrite(ledPin, LOW);
  }
}


/*
  Since GetUptimeCloud is READ_WRITE variable, onGetUptimeCloudChange() is
  executed every time a new value is received from IoT Cloud.
*/
void onGetUptimeCloudChange()  {
  // Add your code here to act upon GetUptimeCloud change
  if(get_uptime_cloud == true) {
    digitalWrite(ledPin, HIGH);
    calculateUptimeAndPost("[UPTIME]");
    digitalWrite(ledPin, LOW);
  }
}

/*
  Since PbBvCloud is READ_WRITE variable, onPbBvCloudChange() is
  executed every time a new value is received from IoT Cloud.
*/
void onPbBvCloudChange()  {
  // Add your code here to act upon PbBvCloud change
  if(pb_bv_cloud == true) {
    bus_voltage_cloud = INA.getBusVoltage_mV();
    uptime_cloud = "BUS VOLTAGE: " + String(bus_voltage_cloud) + "mV \n";
  }
}

/*
  Since ResetCloud is READ_WRITE variable, onResetCloudChange() is
  executed every time a new value is received from IoT Cloud.
*/
void onResetCloudChange()  {
  // Add your code here to act upon ResetCloud change
  if(reset_cloud == true) {
    push_reset();
  }
}














