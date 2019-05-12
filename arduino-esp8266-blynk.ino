/*
 * Arduino ESP8266 Blynk - Use the Blynk IoT platform to create an Internet connected temperature sensor
 *
 * This sketch will regularly measure the temperature and the humidity of the DHT22 sensor and the light 
 * intensity as measured by a photoresistor and push it to the Blynk IoT platform (http://www.blynk.cc/). 
 * We can use the Blynk phone app to monitor the temperature, humidity and light intensity wherever we are.
 * The sketch will measure the temperature, humidity and light intensity every minute and send it to the 
 * blync server as well as a running average of the last 15 samples.
 * A blynk project was created on an iPhone which displays the temperature, humidity and light intensity in 
 * different forms, as gauge, as value and as graph. Next to that a watchdog led is shown which blinks every 
 * second, a real led is also present connected to the arduino which should blink with the same frequency. 
 * This provides an easy way to see if the project is alive. Another virtual led in the blynk project will 
 * light up when there was an error reading the temperature or humidity from the DHT22. 
 * The number of samples the mean is based on is displayed. This is used to investigate the
 * stability of the sensor library. Normally this value is 15, the size of the running average buffer, but 
 * can become less when samples are missing when the temperature or humidity cannot be read from the DHT
 * sensor. 
 *  
 * Please note that you have to replace the values in the file authorization.h with your own values!
 * 
 * (c) 2015-2019 Jurgen Smit. All rights reserved.
 *
 * Circuit: 
 *
 * - NodeMCU 0.9 module
 * - DHT22 Temperature sensor connected to digital pin D1 (GPIO5)
 * - Photoresistor connected to analog pin A0 (ADC0)
 * - An optional LED connected to digital pin D4 (GPIO2) that will blink every second
 * 
 * External References:
 * 
 * - WeeESP8266 - An ESP8266 library for Arduino: https://github.com/itead/ITEADLIB_Arduino_WeeESP8266
 * - Blynk library: https://github.com/blynkkk/blynk-library 
 * - Adafruit DHT Sensor Library: https://github.com/adafruit/DHT-sensor-library
 * - Adafruit Unified Sensor Driver: https://github.com/adafruit/Adafruit_Sensor
 * - SimpleTimer Library for Arduino: http://playground.arduino.cc/Code/SimpleTimer
 * 
 * Blynk Project:
 * 
 * - Gauge: Average Temperature - V0 - Push
 * - Gauge: Average Humidity - V1 - Push
 * - Gauge: Average Light Intensity - V2 - Push
 * - Display: Current Temperature - V3 - Push
 * - Display: Current Humidity - V4 - Push
 * - Display: Current Light Intensity - V5 - Push
 * - Led: Watchdog - V30 - Push
 * - Led: Error reading DTH22 - V31 - Push
 * - Display: Number of valid samples in mean values - V29 - Push
 */

// Uncomment the following lines for debugging purposes when needed
//#define BLYNK_DEBUG
//#define BLYNK_PRINT Serial  

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <DHT.h>
#include <SimpleTimer.h>

#include "authorization.h"

#define DHT_PIN               5         // Arduino pin connected to the temperature and humidity sensor
#define DHT_TYPE              DHT22     // The type of DHT sensor, supported are DHT11, DHT22 and AM2302
#define WATCHDOG_PIN          4         // Watchdog led which should blink every second
#define PHOTORESISTOR_PIN     0         // Arduino pin connected to the photoresistor

#define VLED_WATCHDOG         30        // Blynk LED to indicate heart beat 
#define VLED_ERROR            31        // Blynk LED to indicate an error 

#define VPORT_AVGTEMPERATURE  0         // Blynk value for the average temperature
#define VPORT_AVGHUMIDITY     1         // Blynk value for the average humidity
#define VPORT_AVGLIGHT        2         // Blynk value for the average light intensity
#define VPORT_TEMPERATURE     3         // Blynk value for the current temperature
#define VPORT_HUMIDITY        4         // Blynk value for the current humidity
#define VPORT_LIGHT           5         // Blynk value for the current light intensity
#define VPORT_SAMPLECOUNT     29        // Blynk value that represents the number of values that
                                        // was used to calculate the average values

#define LED_ON                255       // Value to turn LED on
#define LED_OFF               0         // Value to turn LED off

#define SAMPLE_INTERVAL       15        // Send the running average of 15 minutes to the cloud

DHT dht(DHT_PIN, DHT_TYPE);
SimpleTimer timer;

// Buffers to calculate the average values
float temperatureBuffer[SAMPLE_INTERVAL];
float humidityBuffer[SAMPLE_INTERVAL];
float lightBuffer[SAMPLE_INTERVAL];

void setup() {
  // Watchdog LED
  pinMode(WATCHDOG_PIN, OUTPUT);
  
  // Temperature and humidity sensor
  dht.begin();
  
  // Connection to the Blynk server
  Blynk.begin(BLYNK_TOKEN, WIFI_SSID, WIFI_PASSWORD);
  while (!Blynk.connect()) {
    // Wait until connected
  }

  // Running buffer to calculate the aveage values
  for(int i = 0; i <= SAMPLE_INTERVAL; i ++) {
    temperatureBuffer[i] = humidityBuffer[i] = lightBuffer[i] = NAN;
  }

  // Initial sending of the sensor values
  sampleSensorValues();

  // Sample the sensors every minute
  timer.setInterval(60L * 1000L, sampleSensorValues);
  
  // Toggle the watchdog LED every second
  timer.setInterval(1000L, watchDog);
}

/*
 * This function is called every second. It will blink a hardware led and a virtual led 
 * on the iPhone dashboard
 */
void watchDog() {
  BLYNK_LOG("Watch Dog");
  if(digitalRead(WATCHDOG_PIN)) {
    digitalWrite(WATCHDOG_PIN, 0);
    Blynk.virtualWrite(VLED_WATCHDOG, LED_ON);
  }
  else {
    digitalWrite(WATCHDOG_PIN, 1);
    Blynk.virtualWrite(VLED_WATCHDOG, LED_OFF);
  }
}

/*
 * Calculate the mean of the given values. Missing values (NAN) are ignored. If there
 * are no valid values at all the function will return NAN as mean.
 */
float calculateMean(float values[], int n) {
  float mean = NAN;
  
  float sum = 0;
  float numberOfValues = 0;
  for(int i = 0; i < n; i ++) {
    if(!isnan(values[i])) {
      sum += values[i];
      numberOfValues++;      
    }
  }

  if(numberOfValues > 0) {
    mean = sum / numberOfValues;
  }

  return mean;
}

int sampleIndex = 0;
long totalSamples = 0;
float temperature = NAN;
float humidity = NAN;
float light = NAN;

/*
 * Sample the sensor values and send them to the Blynk server
 */
void sampleSensorValues() {

  BLYNK_LOG("Sample Sensor Values...");

  if(dht.read()) {
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
    light = analogRead(PHOTORESISTOR_PIN);

    Blynk.virtualWrite(VLED_ERROR, LED_OFF);

    BLYNK_LOG("    Temperature: " + String(temperature));
    BLYNK_LOG("    Humidity: " + String(humidity));
    BLYNK_LOG("    Light: " + String(light));
  }
  else {
    BLYNK_LOG("Error reading sensor values!");
    Blynk.virtualWrite(VLED_ERROR, LED_ON);
  }

  temperatureBuffer[sampleIndex] = temperature;
  humidityBuffer[sampleIndex] = humidity;
  lightBuffer[sampleIndex] = light;

  sampleIndex ++;
  if(sampleIndex >= SAMPLE_INTERVAL) {
    sampleIndex = 0;
  }

  sendHumidity();
  sendTemperature();
  sendLight();
  sendSampleCount();
}

/*
 * Send the temperature (current and average) to the blynk server. 
 */
void sendTemperature() {
  BLYNK_LOG("Send Temperature");
  float meanTemperature = calculateMean(temperatureBuffer, SAMPLE_INTERVAL);
  if(!isnan(meanTemperature)) {
    Blynk.virtualWrite(VPORT_AVGTEMPERATURE, meanTemperature);
  }
  if(!isnan(temperature)) {
    Blynk.virtualWrite(VPORT_TEMPERATURE, temperature);
  }
}

/*
 * Send the humidity (current and average) to the blynk server. 
 */
void sendHumidity() {
  BLYNK_LOG("Send Humidity");
  float meanHumidity = calculateMean(humidityBuffer, SAMPLE_INTERVAL);
  if(!isnan(meanHumidity)) {
    Blynk.virtualWrite(VPORT_AVGHUMIDITY, meanHumidity);
  }
  if(!isnan(humidity)) {
    Blynk.virtualWrite(VPORT_HUMIDITY, humidity);
  }
}

/*
 * Send the light intensity (current and average) to the blynk server. 
 */
void sendLight() {
  BLYNK_LOG("Send Light");
  float meanLight = calculateMean(lightBuffer, SAMPLE_INTERVAL);
  if(!isnan(meanLight)) {
    Blynk.virtualWrite(VPORT_AVGLIGHT, meanLight);
  }
  if(!isnan(light)) {
    Blynk.virtualWrite(VPORT_LIGHT, light);
  }
}

/*
 * This function is invoked when the Blynk server request the value for
 * the running average of the temperature.
 */
BLYNK_READ(VPORT_AVGTEMPERATURE) {
  sendTemperature();
}

/*
 * This function is invoked when the Blynk server request the value for
 * the running average of the humidity.
 */
BLYNK_READ(VPORT_AVGHUMIDITY) {
  sendHumidity();
}

/*
 * This function is invoked when the Blynk server request the value for
 * the running average of the light intensity.
 */
BLYNK_READ(VPORT_AVGLIGHT) {
  sendLight();
}

/*
 * This function is invoked when the Blynk server request the value for
 * the current temperature.
 */
BLYNK_READ(VPORT_TEMPERATURE) {
  sendTemperature();
}

/*
 * This function is invoked when the Blynk server request the value for
 * the current humidity.
 */
BLYNK_READ(VPORT_HUMIDITY) {
  sendHumidity();
}

/*
 * This function is invoked when the Blynk server request the value for
 * the current light intensity.
 */
BLYNK_READ(VPORT_LIGHT) {
  sendLight();
}

/*
 * This function is invoked when the Blynk server request the value the number of valid 
 * samples in the buffers to calculate the average values.
 */
BLYNK_READ(VPORT_SAMPLECOUNT) {
  sendSampleCount();
}

/*
 * Send the number of valid values in the running average buffers.
 */
void sendSampleCount() {
  BLYNK_LOG("Send Sample Count");
  int sampleCount = 0;
  for(int i = 0; i < SAMPLE_INTERVAL; i ++) {
    if(!isnan(temperatureBuffer[i]) && !isnan(humidityBuffer[i])) {
      sampleCount++;
    }
  }
  Blynk.virtualWrite(VPORT_SAMPLECOUNT, sampleCount);
}

void loop() {
  Blynk.run();
  timer.run();
}
