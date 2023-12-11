#include "spo2_algorithm.h"
#include "MAX30105.h"
#define MAX_BRIGHTNESS 255

#include "spo2_algorithm.h"
#include "MAX30105.h"
#define MAX_BRIGHTNESS 255

int32_t bufferLength = 100; // data length
uint32_t irBuffer[100];     // infrared LED sensor data
uint32_t redBuffer[100];    // red LED sensor data
int8_t validSpO2, validHr;  // indicator to show if the SPO2 calculation is valid
// indicator to show if the heart rate calculation is valid

void getIrRed(MAX30105 &sensor, uint32_t &ir, uint32_t &red)
{
  ir = sensor.getIR();
  red = sensor.getRed();
}

void getSensorMax(MAX30105 &sensor, int32_t &hr, int32_t &spO2)
{
  // dumping the first 25 sets of samples in the memory and shift the last 75 sets of samples to the top
  for (byte i = 25; i < 100; i++)
  {
    redBuffer[i - 25] = redBuffer[i];
    irBuffer[i - 25] = irBuffer[i];
  }

  // take 25 sets of samples before calculating the heart rate.
  for (byte i = 75; i < 100; i++)
  {
    sensor.check(); // Check the sensor for new data

    redBuffer[i] = sensor.getRed();
    irBuffer[i] = sensor.getIR();
    sensor.nextSample(); // We're finished with this sample so move to next sample
  }

  // After gathering 25 new samples recalculate HR and SP02
  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spO2, &validSpO2, &hr, &validHr);
}
