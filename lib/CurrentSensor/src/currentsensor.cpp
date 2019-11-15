// Please credit : chris.keith@gmail.com
 
#include "currentsensor.h"
#include <limits.h>

void CurrentSensor::log(String msg) {
  Particle.publish("Current Sensor", msg);
}
 
unsigned long CurrentSensor::minSecToMillis(unsigned long minutes, unsigned long seconds) {
  return (minutes * 60 * 1000) + (seconds * 1000);
}
 
// Check if two longs are within 'epsilon' of each other.
bool CurrentSensor::insideRangeL(long v1, long v2, long epsilon) {
  return abs(v1 - v2) < epsilon;
}
 
// Check if two doubles are within 'epsilon' of each other.
bool CurrentSensor::insideRangeD(double v1, double v2, double epsilon) {
  return abs(v1 - v2) <= epsilon;
}
 
// Check if two doubles are NOT within 'epsilon' of each other.
bool CurrentSensor::outsideRangeD(double v1, double v2, double epsilon) {
  return !insideRangeD(v1, 2, epsilon);
}
 
void CurrentSensor::logAmps(double amps) {
  if (insideRangeD(amps, previousAmps, 0.5)) {
    return;
  }
  log(String(amps));
  previousAmps = amps;
}
 
double CurrentSensor::doSample() {
  int minVal = 1023;
  int maxVal = 0;
  const int iters = 500; // get this number of readings 1 ms apart.
  for (int i = 0; i < iters; i++) {
    int sensorValue = analogRead(pin);
    minVal  = min(sensorValue, minVal);
    maxVal  = max(sensorValue, maxVal);
    delay(1L);
  }
  int amplitude = maxVal - minVal;
 
  // Noise ??? causing amplitude to never be zero. Kludge around it.
  if (amplitude < 18) { // 18 > observed max noise.
    amplitude = 0;
  }
 
  // Arduino at 5.0V, analog input is 0 - 1023.
  double peakVoltage = (5.0 / 1024) * amplitude / 2;
  double rms = peakVoltage / 1.41421356237;
 
// Regarding the current sensor, the sensor we both have is configured in such
// a way that when 1V of AC is passing through it on the output side, it means
// that 30A is passing through it on the input side.  So, measure the amount
// of AC voltage by computing the Root Mean Squared, and then multiply the
// number of volts by 30 to arrive at the number of amps.
 
  double amps = rms * 30;
  double watts = peakVoltage * amps;
  logAmps(amps);
  return amps;
}
unsigned long CurrentSensor::waitForPowerChange(bool insideRange, const unsigned long timeoutDelta) {
  unsigned long start = millis();
  while (true) {
    if (insideRange) {
      sample();
      if (!insideRangeD(getValue(), AMPS, AMPS * 0.9)) {
        break;
      }
    } else {
      if (!outsideRangeD(getValue(), AMPS, AMPS * 0.9)) {
        break;
      }
    }
    delay(minSecToMillis(0, 1));
    if (millis() - start > timeoutDelta) {
      return TIMEOUT_INDICATOR;
    }
  }
  return millis() - start;
}
 
unsigned long CurrentSensor::waitForPowerOn(const long timeoutDelta) {
  return waitForPowerChange(false, timeoutDelta);
}
 
unsigned long CurrentSensor::waitForPowerOff(const long timeoutDelta) {
  return waitForPowerChange(true, timeoutDelta);
}
 
void CurrentSensor::doRun() {
                  // Assume that sensor unit has been initialized while dryer is off.
  waitForPowerOn(ULONG_MAX);
                // Assume that the only power-on interval that takes 'wrinkleGuardOn' milliseconds
                // is indeed the wrinkle guard interval (and not some other stage in the cycle).
                // TODO : If this doesn't work, try checking for both on and off wrinkle guard intervals.
  unsigned long onInterval;
  while (true) {
    onInterval = waitForPowerOff(ONE_HOUR);
    if (onInterval == TIMEOUT_INDICATOR) {
      log("doRun: TIMEOUT 1.");
      return;
    }
    // Allow 3 seconds for delta check.
    if (insideRangeL(onInterval, wrinkleGuardOn, 3000)) {
      break;
    }
    if (waitForPowerOn(ONE_HOUR) == TIMEOUT_INDICATOR) {
      log("doRun: TIMEOUT 2.");
      return;
    }
  }
  if (waitForPowerOff(ONE_HOUR) == TIMEOUT_INDICATOR) {
    log("doRun: TIMEOUT 3.");
    return;
  }
                // Now it's at the beginning of the second wrinkle guard off cycle.
  while (insideRangeL(onInterval, wrinkleGuardOn, 3000)) {
    delay(wrinkleGuardOff - warningInterval);
    String m("Dryer will start 15 second tumble cycle in ");
    m.concat(String(warningInterval / 1000));
    log(m);
    unsigned long offInterval = waitForPowerOn(ONE_HOUR); 
    if (offInterval == TIMEOUT_INDICATOR) {
      log("doRun: TIMEOUT 4.");
      return;
    }
    onInterval = waitForPowerOff(ONE_HOUR);
    if (onInterval == TIMEOUT_INDICATOR) {
      log("doRun: TIMEOUT 5.");
      return;
    }
  }
}
 
int CurrentSensor::applyFactor(int val) {
    return val * factor;
}
 
CurrentSensor::CurrentSensor(int pin, String name, double factor) {
  this->pin = pin;
  this->name = name;
  this->factor = factor;
  this->lastVal = INT_MIN;
  pinMode(pin, INPUT);
}
 
String CurrentSensor::getName() { return name; }
 
int CurrentSensor::getValue() {
    return applyFactor(lastVal);
}
 
String CurrentSensor::buildValueString() {
    return String(getValue());
}
 
void CurrentSensor::sample() {
  doSample();
}
