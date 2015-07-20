# Arduino ESP8266 Blynk 

## Use the Blynk IoT platform to create an Internet connected temperature sensor

This sketch will regularly measure the temperature and the humidity of the DHT22 sensor and push it
to the Blynk IoT platform (http://www.blynk.cc/). We can use the Blynk phone app to monitor the temperature
and humidity wherever we are.

The sketch will measure the temperature and humidity every minute and send a running average of the last 15
samples to the blync server.

A blynk project was created on an iPhone which displays the temperature and humidity in different forms,
as gauge, as value and as graph. Next to that a watchdog led is shown which blinks every second. A real
led is also present connected to the arduino which should blink with the same frequency. This provides
an easy way to see if the project is alive. Another virtual led in the blynk project will light up when
there was an error reading the temperature or humidity from the DHT22. Before I was using the Adafruit DHT
library which was given a lot of (crc) errors in combination with the DHT22. It looks like the timing is
slighty off. I did not have the same issues with the DHT sensor library (see external references). 
Furthermore the number of samples the mean was based is displayed. This was also used to investigate the
stability of the sensor library. Normally this value is 15, the size of the running average buffer, but 
can become less when samples are missing when the temperature or humidity cannot be read from the DHT
sensor. This last value is pulled from the Blynk project to wake it up after it got disconnected when
the iPhone goes to sleep.
