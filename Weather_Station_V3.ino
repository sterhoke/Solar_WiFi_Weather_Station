//======================================================================================//
//                                                                                      //
//                 Solar WiFi Weather Station V3.0 Firmware                             //
//                                                                                      //
//             Developed by Debasish Dutta, Last Update: 30.03.2021                     //                 
//                                                                                      //
//======================================================================================// 
    
    #include <BME280I2C.h>
    #include "Adafruit_SI1145.h" 
    #include <BH1750.h>     
    #include <DallasTemperature.h> 
    #include <OneWire.h>   
    #include "Wire.h"    
    #include <WiFi.h>      
    #include <BlynkSimpleEsp32.h>   
    #include "esp_deep_sleep.h" //Library needed for ESP32 Sleep Functions  
    #include <PubSubClient.h>

  //=================== Pin assignment definitions ==========================================
 
    #define WIND_SPD_PIN 14
    #define RAIN_PIN     25
    #define WIND_DIR_PIN 35
    #define VOLT_PIN     33 
    #define TEMP_PIN 4  // DS18B20 hooked up to GPIO pin 4

  //=======================================================================================
  
    WiFiClient client;
    BME280I2C bme;
    Adafruit_SI1145 uv = Adafruit_SI1145();
    BH1750 lightMeter(0x23);    
    OneWire oneWire(TEMP_PIN);
    DallasTemperature sensors(&oneWire);
    
  //=========================Declaring Variables and Constants ================================== 

  
  // Variables used in reading temp,pressure and humidity (BME280)
    float temperature, humidity, pressure;
    
 // Variables used in reading UV Index (Si1145)    
    float UVindex;  
    
 // Variables used in reading Lux Level( BH1750 ) 
    float lux;

 // Variables used in calculating the windspeed 
    volatile unsigned long timeSinceLastTick = 0;
    volatile unsigned long lastTick = 0;    
    float windSpeed;
    
 // Variables used in calculating the wind direction 
    int vin; 
    String windDir = "";
        
 // Variables and constants used in tracking rainfall
    #define S_IN_DAY   86400
    #define S_IN_HR     3600
    #define NO_RAIN_SAMPLES 2000    
    volatile long rainTickList[NO_RAIN_SAMPLES];
    volatile int rainTickIndex = 0;
    volatile int rainTicks = 0;
    int rainLastDay = 0;
    int rainLastHour = 0;
    int rainLastHourStart = 0;
    int rainLastDayStart = 0;
    long secsClock = 0;

 // Variables used in calculating the battery voltage 
    float batteryVolt;   
    float Vout = 0.00;
    float Vin = 0.00;
    float R1 = 27000.00; // resistance of R1 (27K) // You can also use 33K 
    float R2 = 100000.00; // resistance of R2 (100K) 
    int val = 0;     

//=========================Deep Sleep Time ================================================ 

 //const int UpdateInterval = 1 * 60 * 1000000;  // e.g. 0.33 * 60 * 1000000; // Sleep time  
 //const int UpdateInterval = 15 * 60 * 1000000;  // e.g. 15 * 60 * 1000000; // // Example for a 15-Min update interval 15-mins x 60-secs * 10000

 //========================= Enable Blynk or Thingspeak ===================================
  
 // configuration control constant for use of either Blynk or Thingspeak
 //const String App = "BLYNK";         //  alternative is line below
 //const String App = "Thingspeak"; //  alternative is line above   
   const String App = "MQTT"; //  alternative is line above

 //========================= Variables for wifi server setup =============================
     
  // Your WiFi credentials.
  // Set password to "" for open networks.
  char ssid[] = "Home"; // WiFi Router ssid
  char pass[] = "thisisonly4meandnooneelse"; // WiFi Router password

  // copy it from the mail received from Blynk
  char auth[] = "XXXX"; 
  
  // Thingspeak Write API
  const char* server = "api.thingspeak.com";
  const char* api_key = "XXXX"; // API write key 
  
//========================= Setup Function ================================================ 

  void setup() 
    {
      Serial.begin(115200);
      delay(25);
      Serial.println("\nWeather station powered on.\n");
      Wire.begin(); 
      Serial.println("\nWire.\n");
      sensors.begin(); 
      Serial.println("\nsensors.\n");
  //  Wire.begin(22, 21); // for BH1750  
      bme.begin(); // 0x76 is the address of the BME280 module
      Serial.println("\nbme.\n");
      uv.begin(0x60);  // 0x60 is the address of the GY1145 module 
      Serial.println("\nuv.\n");    
      wifi_connect(); 
    
      // Wind speed sensor setup. The windspeed is calculated according to the number
      //  of ticks per second. Timestamps are captured in the interrupt, and then converted
      //  into mph. 
      pinMode(WIND_SPD_PIN, INPUT);     // Wind speed sensor
      attachInterrupt(digitalPinToInterrupt(WIND_SPD_PIN), windTick, FALLING);
    
      // Rain sesnor setup. Rainfall is tracked by ticks per second, and timestamps of
      //  ticks are tracked so rainfall can be "aged" (i.e., rain per hour, per day, etc)
      pinMode(RAIN_PIN, INPUT);     // Rain sensor
      attachInterrupt(digitalPinToInterrupt(RAIN_PIN), rainTick, FALLING);
      // Zero out the timestamp array.
      for (int i = 0; i < NO_RAIN_SAMPLES; i++) rainTickList[i] = 0;      
     
     // ESP32 Deep SLeep Mode
     // esp_deep_sleep_enable_timer_wakeup(UpdateInterval);
     // Serial.println("Going to sleep now...");
     // esp_deep_sleep_start();     
    }
//================================ Loop Function ==============================================  

      void loop() 
    {
      
     Read_Sensors_Data();  // Read all the Sensors 
     printdata();          // Print all the sensors data on the serial monitor
     Send_Data();          // Upload all the sensors data on the Internet ( Blynk App or Thingspeak )

    }

//============================ Connect to WiFi Network =========================================

void wifi_connect()
{
  if (App == "BLYNK")  // for posting datas to Blynk App
  { 
    Blynk.begin(auth, ssid, pass);
  } 
  else if ((App == "Thingspeak") || (App == "MQTT"))  // for posting datas to Thingspeak website
  {
     WiFi.begin(ssid, pass);
     Serial.println("\nWifi begin\n");
     while (WiFi.status() != WL_CONNECTED) 
     {
            delay(500);
            Serial.print(".");
     }
     Serial.println("");
     Serial.println("WiFi connected");  
  } 
  else 
  {
    WiFi.begin(ssid, pass);
    Serial.print(App);
    Serial.println(" is not a valid application");
  }
  
}

//===================================================================================================

//           Read Sensors Data ( BME280, Si1145,BH1750, Bat. Voltage, Wind Sensors, Rain Gauge )

//==================================================================================================


   void Read_Sensors_Data()
{
   // Reading BME280 sensor
   Serial.println("\bme read.\n");
    bme.read(pressure, temperature, humidity, BME280::TempUnit_Celsius, BME280::PresUnit_Pa); 


 //***************************************************************************
Serial.println("\DS18 read.\n");
 // Reading DS18B20 sensor
     sensors.requestTemperatures(); 


 //***************************************************************************
  // Reading GY1145 UV sensor
Serial.println("\1145 read.\n");
    UVindex = uv.readUV();
   // the index is multiplied by 100 so to get the
   // integer index, divide by 100!
      UVindex /= 100.0;     
  //**********************************************************************************
 /*// Reading BH1750 sensor  
  lux = lightMeter.readLightLevel();
 
  */
//**********************************************************************************
// Reading Battery Level in %
Serial.println("\batt read.\n");
   val = analogRead(VOLT_PIN);//reads the analog input
   Vout = (val * 3.3 ) / 4095.0; // formula for calculating voltage out 
   batteryVolt = Vout * ( R2+R1) / R2 ; // formula for calculating voltage in   
  

//**********************************************************************************
// Read Weather Meters Datas ( Wind Speed, Rain Fall and Wind Direction )

      static unsigned long outLoopTimer = 0;
      static unsigned long wundergroundUpdateTimer = 0;
      static unsigned long clockTimer = 0;
      static unsigned long tempMSClock = 0;
    
      // Create a seconds clock based on the millis() count. We use this
      //  to track rainfall by the second. We've done this because the millis()
      //  count overflows eventually, in a way that makes tracking time stamps
      //  very difficult.
      tempMSClock += millis() - clockTimer;
      clockTimer = millis();
      while (tempMSClock >= 1000)
      {
        secsClock++;
        tempMSClock -= 1000;
      }
      
      // This is a once-per-second timer that calculates and prints off various
      //  values from the sensors attached to the system.
      if (millis() - outLoopTimer >= 2000)
      {
        outLoopTimer = millis();
        // Windspeed calculation, in mph. timeSinceLastTick gets updated by an
        //  interrupt when ticks come in from the wind speed sensor.
        if (timeSinceLastTick != 0) windSpeed = 1000.0/timeSinceLastTick;

       // Calculate the wind direction and display it as a string.
        windDirCalc();       
     
     rainLastHour = 0;
     rainLastDay = 0;
  // If there are any captured rain sensor ticks...
     if (rainTicks > 0)
     {
      // Start at the end of the list. rainTickIndex will always be one greater
     //  than the number of captured samples.
      int i = rainTickIndex-1;
    
    // Iterate over the list and count up the number of samples that have been
    //  captured with time stamps in the last hour.
       while ((rainTickList[i] >= secsClock - S_IN_HR) && rainTickList[i] != 0)
          {
            i--;
            if (i < 0) i = NO_RAIN_SAMPLES-1;
            rainLastHour++;
          }
    
          // Repeat the process, this time over days.
          i = rainTickIndex-1;
          while ((rainTickList[i] >= secsClock - S_IN_DAY) && rainTickList[i] != 0)
          {
            i--;
            if (i < 0) i = NO_RAIN_SAMPLES-1;
            rainLastDay++;
          }
          rainLastDayStart = i;
        }
      }  
   }

   
     
   // Keep track of when the last tick came in on the wind sensor.
    void windTick(void)
    {
      timeSinceLastTick = millis() - lastTick;
      lastTick = millis();
    }
    
    // Capture timestamp of when the rain sensor got tripped.
    void rainTick(void)
    {
      rainTickList[rainTickIndex++] = secsClock;
      if (rainTickIndex == NO_RAIN_SAMPLES) rainTickIndex = 0;
      rainTicks++;
    }
    
// reading wind direction
    void windDirCalc()
    {
      
  vin = analogRead(WIND_DIR_PIN);

  if      (vin < 150) windDir="202.5";
  else if (vin < 300) windDir = "180";
  else if (vin < 400) windDir = "247.5";
  else if (vin < 600) windDir = "225";
  else if (vin < 900) windDir = "292.5";
  else if (vin < 1100) windDir = "270";
  else if (vin < 1500) windDir = "112.5";
  else if (vin < 1700) windDir = "135";
  else if (vin < 2250) windDir = "337.5";
  else if (vin < 2350) windDir = "315";
  else if (vin < 2700) windDir = "67.5";
  else if (vin < 3000) windDir = "90";
  else if (vin < 3200) windDir = "22.5";
  else if (vin < 3400) windDir = "45";
  else if (vin < 4000) windDir = "0";
  else windDir = "0";  
  }

 //======================  Print Data on Serial Monitor  ===============================================

  void printdata(){
   Serial.print("Air temperature [°C]: "); Serial.println(temperature);
   Serial.print("Humidity [%]: "); Serial.println(int(humidity));
   Serial.print("Barometric pressure [hPa]: "); Serial.println(pressure / 100);   
   Serial.print("UV: "); Serial.println(UVindex);   
 //  Serial.print("Light: ");  Serial.print(lux);  Serial.println(" lx");    
   Serial.print("Windspeed: "); Serial.print(windSpeed*2.4); Serial.println(" mph");     
   Serial.print("Wind dir: ");  Serial.print("  "); Serial.println(windDir);   
   Serial.print("Rainfall last hour: "); Serial.println(float(rainLastHour)*0.011, 3);
 //  Serial.print("Rainfall last day: ");  Serial.println(float(rainLastDay)*0.011, 3);
 //  Serial.print("Rainfall to date: ");   Serial.println(float(rainTicks)*0.011, 3);    
   Serial.print("Battery Level: "); Serial.println(batteryVolt);  
   Serial.print("Temperature in C: ");   Serial.println(sensors.getTempCByIndex(0));    //print the temperature in Celsius  
   Serial.print("Temperature in F: "); Serial.println((sensors.getTempCByIndex(0) * 9.0) / 5.0 + 32.0); //print the temperature in Fahrenheit  

  }
   
//======================Upload Sensors data to Blynk App or Thingspeak =================================

void Send_Data()
{
// code block for uploading data to BLYNK App
  
  if (App == "BLYNK") { // choose application
   Blynk.virtualWrite(0,temperature );    // virtual pin 0 
   Blynk.virtualWrite(1, humidity ); // virtual pin 1
   Blynk.virtualWrite(2, pressure/100 );    // virtual pin 2
   Blynk.virtualWrite(3, UVindex);    // virtual pin 3
  // Blynk.virtualWrite(4, windSpeed*1.492 ); // virtual pin 4  
   Blynk.virtualWrite(4, windSpeed*2.4*4.5 ); // virtual pin 4 
   Blynk.virtualWrite(5, windDir);    // virtual pin 5 
   Blynk.virtualWrite(6, rainLastHour);    // virtual pin 6 
   Blynk.virtualWrite(7, batteryVolt);    // virtual pin 7 
   Blynk.virtualWrite(8, sensors.getTempCByIndex(0));    // virtual pin 8 
   delay(12*5000);
  } 
    
 // code block for uploading data to Thingspeak website
 
  else if (App == "Thingspeak") {
  // Send data to ThingSpeak 
  WiFiClient client;  
  if (client.connect(server,80)) {
  Serial.println("Connect to ThingSpeak - OK"); 
  Serial.println(""); 
  Serial.println("********************************************"); 
  String postStr = "";
  postStr+="GET /update?api_key=";
  postStr+=api_key;   
  postStr+="&field1=";
  postStr+=String(temperature);
  postStr+="&field2=";
  postStr+=String(humidity);
  postStr+="&field3=";
  postStr+=String(pressure/100);  
  postStr+="&field4=";
  postStr+=String(UVindex);  
  postStr+="&field5=";
  //postStr+=String(windSpeed*1.492); //speed in mph
  postStr+=String(windSpeed*2.4*4.5); //speed in Km/h
  postStr+="&field6=";
  postStr+=String(windDir);
  postStr+="&field7=";
  postStr+=String(float(rainTicks)*0.011, 3);  
  postStr+="&field8=";
  postStr+=String(batteryVolt);   
  postStr+="&field9=";
  postStr+=String(sensors.getTempCByIndex(0));   
  postStr+=" HTTP/1.1\r\nHost: a.c.d\r\nConnection: close\r\n\r\n";
  postStr+="";
  client.print(postStr);
  delay(5000);
 //*******************************************************************************

}
 while(client.available()){
    String line = client.readStringUntil('\r');
   // Serial.print(line);
  }
 } else if (App == "MQTT") {
  // Send data to MQTT 
  WiFiClient client;  
  if (client.connect(server,80)) {
  Serial.println("Connect to ThingSpeak - OK"); 
  Serial.println(""); 
  Serial.println("********************************************"); 
  // code block for publishing all data to MQTT

  client.publish("home/weather/solarweatherstation/tempc", temperature, 1);      // ,1 = retained
  delay(50);

  client.publish("home/weather/solarweatherstation/humi", humidity, 1);      // ,1 = retained
  delay(50);
/*
  client.publish("home/weather/solarweatherstation/abshpa", _measured_pres, 1);      // ,1 = retained
  delay(50);
 */
  client.publish("home/weather/solarweatherstation/relhpa", pressure/100, 1);      // ,1 = retained
  delay(50);

 if ( (volt > 3.3) && (volt < 4.25)) {          //check if batt still ok, if yes
        client.publish("home/weather/solarweatherstation/battv", batteryVolt, 1);      // ,1 = retained
  }
  delay(50);
/*
  char _DewpointTemperature[8];                                // Buffer big enough for 7-character float
  dtostrf(DewpointTemperature, 3, 1, _DewpointTemperature);               // Leave room for too large numbers!

  client.publish("home/weather/solarweatherstation/dewpointc", _DewpointTemperature, 1);      // ,1 = retained
  delay(50);
*/

  client.publish("home/weather/solarweatherstation/heatindexc", _HeatIndex, 1);      // ,1 = retained
  delay(50);

  char _accuracy_in_percent[8];                                // Buffer big enough for 7-character float
  dtostrf(accuracy_in_percent, 3, 0, _accuracy_in_percent);               // Leave room for too large numbers!

  client.publish("home/weather/solarweatherstation/accuracy", _accuracy_in_percent, 1);      // ,1 = retained
  delay(50);

  char _DewPointSpread[8];                                // Buffer big enough for 7-character float
  dtostrf(DewPointSpread, 3, 1, _DewPointSpread);               // Leave room for too large numbers!

  client.publish("home/weather/solarweatherstation/spreadc", _DewPointSpread, 1);      // ,1 = retained
  delay(50);

  char tmp1[128];
  ZambrettisWords.toCharArray(tmp1, 128);
  client.publish("home/weather/solarweatherstation/zambrettisays", tmp1, 1);
  delay(50);

  char tmp2[128];
  trend_in_words.toCharArray(tmp2, 128);
  client.publish("home/weather/solarweatherstation/trendinwords", tmp2, 1);
  delay(50);

  char _trend[8];                                // Buffer big enough for 7-character float
  dtostrf(pressure_difference[11], 3, 2, _trend);               // Leave room for too large numbers!

  client.publish("home/weather/solarweatherstation/trend", _trend, 1);      // ,1 = retained
  delay(50);
  client.print(postStr);
  delay(5000);
 //*******************************************************************************

}
 while(client.available()){
    String line = client.readStringUntil('\r');
   // Serial.print(line);
  }
 } 
 void connect_to_MQTT() {
  Serial.print("---> Connecting to MQTT, ");
  client.setServer(mqtt_server, 1883);
  
  while (!client.connected()) {
    Serial.println("reconnecting MQTT...");
    reconnect(); 
  }
  Serial.println("MQTT connected ok.");
} //end connect_to_MQTT  
}
//=============================End of the Program =================================


    
  
