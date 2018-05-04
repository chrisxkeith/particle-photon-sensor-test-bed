// https://opensource.org/licenses/MIT

const unsigned long ONE_DAY_IN_MILLISECONDS = 24 * 60 * 60 * 1000;             
unsigned long lastSync = millis();

class SensorData {
  public:
    int pin;
    String name;
    bool isAnalog;
    int lastVal;
    int minVal;
    int maxVal;
    double sum;
    int sumCount; // for calculating average
    
    SensorData(int pin, String name, bool isAnalog) {
        this->pin = pin;
        this->name = name;
        this->isAnalog = isAnalog;
        resetVals();
        pinMode(pin, INPUT);
    }
    
    void sample() {
        if (isAnalog) {
            lastVal = analogRead(pin);
        } else {
            lastVal = digitalRead(pin);
        }
        minVal = min(lastVal, minVal);
        maxVal = max(lastVal, minVal);
        sum += lastVal;
        sumCount++;
    }
    
    void resetVals() {
        lastVal = INT_MIN;
        minVal = INT_MAX;
        maxVal = INT_MIN;
        sum = 0.0;
        sumCount = 0;
    }
};

class SensorTestBed {
  private:
    // Change this to sample at an appropriate rate.
    const static int eventIntervalInSeconds = 2 * 60;
    
    // Change these depending on how many sensors you want to report data from.
    // Names should be unique for reporting purposes.
    const static int nSensors = 2;
    SensorData sensors[ nSensors ] = {
         SensorData(A0, "Sound A0 sensor", true)
        ,SensorData(D3, "Water Level D3 sensor", false)
    };
    
    void publish(String event, String data) {
      Particle.publish(event, data, 1, PRIVATE);
    }

    String buildValueString(int v) {
        String s = String("|");
        s.concat(Time.format(Time.now(), TIME_FORMAT_ISO8601_FULL));
        s.concat("|");
        s.concat(String(v));
        return s;
    }

  public:
	SensorTestBed() { }

    int publishSensorData() {
        for (int i = 0; i < nSensors; i++) {
            sensors[i].sample();
            publish(sensors[i].name, buildValueString(sensors[i].lastVal));
            sensors[i].resetVals();
        }
        // Report on even intervals starting at midnight.
        return eventIntervalInSeconds - (Time.now() % eventIntervalInSeconds);
    }
};

SensorTestBed sensorTestBed;

void setup() {
    Serial.begin(9600);
    sensorTestBed = SensorTestBed();
    Particle.syncTime();
}

void loop() {
    if (millis() - lastSync > ONE_DAY_IN_MILLISECONDS) {
        Particle.syncTime();
        lastSync = millis();
    }
    delay(sensorTestBed.publishSensorData() * 1000);
}
