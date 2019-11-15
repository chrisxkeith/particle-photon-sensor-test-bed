#include "sensordata.h"

#include <limits.h>

SensorData::SensorData(int pin, String name, double factor) {
    this->pin = pin;
    this->name = name;
    this->factor = factor;
    this->lastVal = INT_MIN;
    pinMode(pin, INPUT);
}

String SensorData::getName() { return name; }

void SensorData::sample() {
    if (pin >= A0 && pin <= A5) {
        lastVal = analogRead(pin);
    } else {
        lastVal = digitalRead(pin);
    }
}

int SensorData::getValue() {
    return applyFactor(lastVal);
}

String SensorData::buildValueString() {
    return String(getValue());
}
    int SensorData::applyFactor(int val) {
    return val * factor;
}
