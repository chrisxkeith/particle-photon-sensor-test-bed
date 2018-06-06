// https://opensource.org/licenses/MIT

class TimeSync {
    private:
        const unsigned long ONE_DAY_IN_MILLISECONDS = 24 * 60 * 60 * 1000;
        unsigned long lastSync = millis();
    public:
        void sync() {
            if (millis() - lastSync > ONE_DAY_IN_MILLISECONDS) {
                Particle.syncTime();
                lastSync = millis();
            }
        }
};

class SensorData {
  private:
    int pin;
    String name;
    bool isAnalog;
    double factor; // apply to get human-readable values, e.g., degrees F

    int lastVal;
    int minVal;
    int maxVal;
    double sum;
    int sumCount; // for calculating average

  public:
    SensorData(int pin, String name, bool isAnalog, double factor) {
        this->pin = pin;
        this->name = name;
        this->isAnalog = isAnalog;
        this->factor = factor;
        resetVals();
        pinMode(pin, INPUT);
    }
    
    String getName() { return name; }

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

    int applyFactor(int val) {
        return val * factor;
    }

    String buildValueString() {
        String s = String("|");
        s.concat(Time.format(Time.now(), TIME_FORMAT_ISO8601_FULL));
        s.concat("|");
        s.concat(String(applyFactor(lastVal)));
        s.concat("|");
        s.concat(String(applyFactor(minVal)));
        s.concat("|");
        s.concat(String(applyFactor(maxVal)));
        s.concat("|");
        s.concat(String((int)applyFactor(sum / sumCount)));
        s.concat("|");
        s.concat(String(sumCount));
        s.concat("|");
        s.concat(String(factor));
        return s;
    }
};

class SensorTestBed {
  private:
    // Change this to sample at an appropriate rate.
    const static int publishIntervalInSeconds = 2 * 60;
    int sampleIntervalInSeconds = 15;
    
    // Change these depending on how many sensors you want to report data from.
    // Names should be unique for reporting purposes.
    const static int nSensors = 1;
    SensorData sensors[ nSensors ] = {
         SensorData(A0, "Thermistor 02 sensor:", true, 0.022)
//         , SensorData(A0, "Thermistor 01 sensor:", true, 0.024)
    };
    
    void publish(String event, String data) {
      Particle.publish(event, data, 1, PRIVATE);
    }

	void sample() {
        for (int i = 0; i < nSensors; i++) {
            sensors[i].sample();
        }
    }

    void publish() {
        for (int i = 0; i < nSensors; i++) {
            publish(sensors[i].getName(), sensors[i].buildValueString());
            sensors[i].resetVals();
        }
    }

  public:
	SensorTestBed() {
	    if (sampleIntervalInSeconds > publishIntervalInSeconds) {
	        sampleIntervalInSeconds = publishIntervalInSeconds;
	    }
	}

    void sampleSensorData(TimeSync timeSync) {
        // publish one right away to verify that things might be working.
        sample();
        publish();
        while (true) {
            // Report on even intervals starting at midnight.
            int nextPublish = publishIntervalInSeconds - (Time.now() % publishIntervalInSeconds);
            while (nextPublish > 0) {
                timeSync.sync();
                sample();
                delay(min(sampleIntervalInSeconds, nextPublish) * 1000);
                nextPublish -= sampleIntervalInSeconds;
            }
            publish();
        }
    }
};

SensorTestBed sensorTestBed;
const String githubHash = "to be manually inserted after git push";

void setup() {
    Serial.begin(9600);
    sensorTestBed = SensorTestBed();
    Particle.syncTime();
}

void loop() {
    Particle.variable("GitHubHash", githubHash);
    sensorTestBed.sampleSensorData(TimeSync());
}
