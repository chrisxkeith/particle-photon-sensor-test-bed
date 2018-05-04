// https://opensource.org/licenses/MIT

const unsigned long ONE_DAY_IN_MILLISECONDS = 24 * 60 * 60 * 1000;             
unsigned long lastSync = millis();

enum aggregationType { LAST, MIN, MAX, AVERAGE };

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
    aggregationType aggType;
    
    SensorData(int pin, String name, bool isAnalog, aggregationType aggType) {
        this->pin = pin;
        this->name = name;
        this->isAnalog = isAnalog;
        this->aggType = aggType;
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

    int getVal() {
        switch (aggType) {
            case LAST : return lastVal;
            case MIN : return minVal;
            case MAX : return maxVal;
            case AVERAGE : return (int)(sum / sumCount);
            default : return lastVal;
        }
    }
};

class SensorTestBed {
  private:
    // Change this to sample at an appropriate rate.
    const static int publishIntervalInSeconds = 60;
    int sampleIntervalInSeconds = 1;
    
    // Change these depending on how many sensors you want to report data from.
    // Names should be unique for reporting purposes.
    const static int nSensors = 2;
    SensorData sensors[ nSensors ] = {
         SensorData(A0, "Sound A0 sensor", true, AVERAGE)
        ,SensorData(D3, "Water Level D3 sensor", false, LAST)
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

	void sample() {
        for (int i = 0; i < nSensors; i++) {
            sensors[i].sample();
        }
    }

    void publish() {
        for (int i = 0; i < nSensors; i++) {
            publish(sensors[i].name, buildValueString(sensors[i].getVal()));
            sensors[i].resetVals();
        }
    }

  public:
	SensorTestBed() {
	    if (sampleIntervalInSeconds > publishIntervalInSeconds) {
	        sampleIntervalInSeconds = publishIntervalInSeconds;
	    }
	}

    void sampleSensorData() {
        // publish one right away to verify that things might be working.
        sample();
        publish();
        while (true) {
            // Report on even intervals starting at midnight.
            int nextPublish = publishIntervalInSeconds - (Time.now() % publishIntervalInSeconds);
            while (nextPublish > 0) {
                sample();
                delay(min(sampleIntervalInSeconds, nextPublish) * 1000);
                nextPublish -= sampleIntervalInSeconds;
            }
            publish();
        }
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
    sensorTestBed.sampleSensorData();
}
