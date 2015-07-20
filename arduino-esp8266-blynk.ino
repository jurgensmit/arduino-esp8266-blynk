/*
 * Arduino ESP8266 Blynk - Use the Blynk IoT platform to create an Internet connected temperature sensor
 *
 * This sketch will regularly measure the temperature and the humidity of the DHT22 sensor and push it
 * to the Blynk IoT platform (http://www.blynk.cc/). We can use the Blynk phone app to monitor the temperature
 * and humidity wherever we are.
 * The sketch will measure the temperature and humidity every minute and send a running average of the last 15
 * samples to the blync server.
 * A blynk project was created on an iPhone which displays the temperature and humidity in different forms,
 * as gauge, as value and as graph. Next to that a watchdog led is shown which blinks every second. A real
 * led is also present connected to the arduino which should blink with the same frequency. This provides
 * an easy way to see if the project is alive. Another virtual led in the blynk project will light up when
 * there was an error reading the temperature or humidity from the DHT22. Before I was using the Adafruit DHT
 * library which was given a lot of (crc) errors in combination with the DHT22. It looks like the timing is
 * slighty off. I did not have the same issues with the DHT sensor library (see external references). 
 * Furthermore the number of samples the mean was based is displayed. This was also used to investigate the
 * stability of the sensor library. Normally this value is 15, the size of the running average buffer, but 
 * can become less when samples are missing when the temperature or humidity cannot be read from the DHT
 * sensor. This last value is pulled from the Blynk project to wake it up after it got disconnected when
 * the iPhone goes to sleep.
 * 
 * Please note that you have to replace the values in the file authorization.h with your own values!
 * 
 * (c) 2015 Jurgen Smit. All rights reserved.
 *
 * Circuit: 
 *
 * - ESP8266 Wifi Module connected to the second serial port of the Arduino.
 * - DHT22 Temperature sensor connected to pin 4
 * - An optional LED connected to digital pin 9 that can be controlled from blynk
 * - An optional LED connected to digital pin 8 that will blink every second
 * 
 * External References:
 * 
 * - WeeESP8266 - An ESP8266 library for Arduino: https://github.com/itead/ITEADLIB_Arduino_WeeESP8266
 * - Blynk library: https://github.com/blynkkk/blynk-library 
 * - DHT22 Sensor Library: https://github.com/nethoncho/Arduino-DHT22
 * - SimpleTimer Library for Arduino: http://playground.arduino.cc/Code/SimpleTimer
 * 
 * Blynk Project:
 * 
 * - Gauge: Temperature - V0 - Push
 * - Gauge: Humidity - V1 - Push
 * - Display: Temperature - V4 - Push
 * - Led: Watchdog - V30 - Push
 * - Led: Error reading DTH22 - V31 - Push
 * - Display: Humidity - V5 - Push
 * - Display: Number of valid samples in mean temperature and humidity - V29 - Refresh every 30 seconds
 * - Graph: Temperature - V2 - Push
 * - Graph: Humidity - V3 - Push
 * - Slider: Led - D9
 */

//#define BLYNK_DEBUG
//#define BLYNK_PRINT Serial   

#include <ESP8266.h>
#include <BlynkSimpleShieldEsp8266.h>
#include <DHT22.h>
#include <SimpleTimer.h>

#include "authorization.h"

#define EspSerial           Serial1   // Use Serial1 to communicate to the ESP8266
#define DHT_PIN             4         // Arduino pin connected to the DHT temperature and humidity sensor
#define WATCHDOG_PIN        8         // Watchdog led which should blink every second

#define SAMPLE_INTERVAL     15        // Send the running average of 15 minutes to the cloud

ESP8266 wifi(EspSerial);
DHT22 dht(DHT_PIN);
SimpleTimer timer;

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
const char auth[] = BLYNK_TOKEN;

float temperatureBuffer[SAMPLE_INTERVAL];
float humidityBuffer[SAMPLE_INTERVAL];

void setup() {
  pinMode(WATCHDOG_PIN, OUTPUT);

  // Initialize the serial ports
  EspSerial.begin(115200);  // Set ESP8266 baud rate
  delay(10);
  Serial.begin(115200);
  delay(10);

  // Initialize the Blynk libraries
  Blynk.begin(auth, wifi, WIFI_SSID, WIFI_PASSWORD);

  // Initialize the running buffer
  for(int i = 0; i <= SAMPLE_INTERVAL; i ++) {
    temperatureBuffer[i] = humidityBuffer[i] = NAN;
  }

  // Initial sending of the sensor values
  sampleSensorValues();

  // Sample the sensors every minute
  timer.setInterval(60L * 1000L, sampleSensorValues);
  
  // Toggle a led every second
  timer.setInterval(1000L, watchDog);
}

/*
 * This function is called every second. It will blink a hardware led and a virtual led 
 * on the iPhone dashboard
 */
void watchDog() {
  digitalWrite(WATCHDOG_PIN, !digitalRead(WATCHDOG_PIN));
  Blynk.virtualWrite(30, digitalRead(WATCHDOG_PIN));
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

/*
 * Sample the sensor values and send them to the Blynk server
 */
void sampleSensorValues() {

  float temperature = NAN;
  float humidity = NAN;

  DHT22_ERROR_t errorCode = dht.readData();
  if(errorCode == DHT_ERROR_NONE) {
    temperature = dht.getTemperatureC();
    humidity = dht.getHumidity();
  }

  temperatureBuffer[sampleIndex] = temperature;
  humidityBuffer[sampleIndex] = humidity;

  sampleIndex ++;
  if(sampleIndex >= SAMPLE_INTERVAL) {
    sampleIndex = 0;
  }

  sendTemperature();
  sendHumidity();
  sendSampleCount();

  if(isnan(temperature) || isnan(humidity)) {
      Blynk.virtualWrite(31, HIGH);
  }
  else {
      Blynk.virtualWrite(31, LOW);
  }
}

/*
 * Send the temperature to the blynk server. We have to send it several times 
 * as we can only bind a (virtual) port to a single widget in the Blynk project.
 */
void sendTemperature() {
  float meanTemperature = calculateMean(temperatureBuffer, SAMPLE_INTERVAL);
  if(!isnan(meanTemperature)) {
    Blynk.virtualWrite(0, meanTemperature);
    Blynk.virtualWrite(2, meanTemperature);
    Blynk.virtualWrite(4, meanTemperature);
  }
}

/*
 * Send the humidity to the blynk server. We have to send it several times 
 * as we can only bind a (virtual) port to a single widget in the Blynk project.
 */
void sendHumidity() {
  float meanHumidity = calculateMean(humidityBuffer, SAMPLE_INTERVAL);
  if(!isnan(meanHumidity)) {
    Blynk.virtualWrite(1, meanHumidity);
    Blynk.virtualWrite(3, meanHumidity);
    Blynk.virtualWrite(5, meanHumidity);
  }
}

/*
 * This function is invoked when the Blynk server request the value for
 * the virtual port V0. It returns the running average of the temperature.
 */
BLYNK_READ(0) {
  sendTemperature();
}

/*
 * This function is invoked when the Blynk server request the value for
 * the virtual port V2. It returns the running average of the temperature.
 */
BLYNK_READ(2) {
  sendTemperature();
}

/*
 * This function is invoked when the Blynk server request the value for
 * the virtual port V4. It returns the running average of the temperature.
 */
BLYNK_READ(4) {
  sendTemperature();
}

/*
 * This function is invoked when the Blynk server request the value for
 * the virtual port V1. It returns the running average of the humidity.
 */
BLYNK_READ(1) {
  sendHumidity();
}

/*
 * This function is invoked when the Blynk server request the value for
 * the virtual port V3. It returns the running average of the humidity.
 */
BLYNK_READ(3) {
  sendHumidity();
}

/*
 * This function is invoked when the Blynk server request the value for
 * the virtual port V5. It returns the running average of the humidity.
 */
BLYNK_READ(5) {
  sendHumidity();
}

/*
 * This function is invoked when the Blynk server request the value for
 * the virtual port V29. It returns the number of valid samples in the temperature
 * and humidity buffers.
 */
BLYNK_READ(29) {
  sendSampleCount();
}

/*
 * Send the number of valid values in the running average buffers.
 */
void sendSampleCount() {
  int sampleCount = 0;
  for(int i = 0; i < SAMPLE_INTERVAL; i ++) {
    if(!isnan(temperatureBuffer[i]) && !isnan(humidityBuffer[i])) {
      sampleCount++;
    }
  }
  Blynk.virtualWrite(29, sampleCount);
}

void loop() {
  Blynk.run();
  timer.run();
}

