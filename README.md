# Arduino ESP8266 Blynk 

## Use the Blynk IoT platform to create an Internet connected temperature sensor

This sketch will regularly measure the temperature and the humidity of the DHT22 sensor and the light intensity as measured by a photoresistor and push it to the Blynk IoT platform (http://www.blynk.cc/). 

We can use the Blynk phone app to monitor the temperature, humidity and light intensity wherever we are.

The sketch will measure the temperature, humidity and light intensity every minute and send it to the Blynk server as well as a running average of the last 15 samples.

A blynk project was created on an iPhone which displays the temperature, humidity and light intensity in different forms, as gauge, as value and as graph. Next to that a watchdog led is shown which blinks every second, a real led is also present connected to the arduino which should blink with the same frequency. This provides an easy way to see if the project is alive. Another virtual led in the blynk project will light up when there was an error reading the temperature or humidity from the DHT22. 
 
The number of samples the mean is based on is displayed. This is used to investigate the stability of the sensor library. Normally this value is 15, the size of the running average buffer, but can become less when samples are missing when the temperature or humidity cannot be read from the DHT sensor.
