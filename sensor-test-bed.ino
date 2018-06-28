// This #include statement was automatically added by the Particle IDE.
#include <SparkFunMicroOLED.h>
// https://learn.sparkfun.com/tutorials/photon-oled-shield-hookup-guide
#include <math.h>

// https://opensource.org/licenses/MIT

const String githubHash = "to be manually inserted after git push";

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

MicroOLED oled;

class OLEDWrapper {
    public:
        OLEDWrapper() {
            oled.begin();    // Initialize the OLED
            oled.clear(ALL); // Clear the display's internal memory
            oled.display();  // Display what's in the buffer (splashscreen)
            delay(1000);     // Delay 1000 ms
            oled.clear(PAGE); // Clear the buffer.
        }

    void printTitle(String title, int font)
    {
      oled.clear(PAGE);
      oled.setFontType(font);
      oled.setCursor(0, 0);
      oled.print(title);
      oled.display();
    }
};

OLEDWrapper oledWrapper;

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
    String unit; // for display

  public:
    SensorData(int pin, String name, bool isAnalog, double factor, String unit) {
        this->pin = pin;
        this->name = name;
        this->isAnalog = isAnalog;
        this->factor = factor;
        this->unit = unit;
        resetVals();
        pinMode(pin, INPUT);
    }
    
    String getName() { return name; }
    String getLastVal() {
        String s = String(applyFactor(lastVal));
        s.concat(unit);
        return s;
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

    int applyFactor(int val) {
        return val * factor;
    }

    String buildValueString() {
        String s = String("|");
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

    String buildPublishString() {
        String s = String("|");
        s.concat(Time.format(Time.now(), TIME_FORMAT_ISO8601_FULL));
        s.concat(buildValueString());
        return s;
    }
};

int publishIntervalInSeconds = 2 * 60;
int sampleIntervalInSeconds = 15;

class SensorTestBed {
  private:
    
    SensorData t1[ 2 ] = {
         SensorData(A0, "Thermistor 01 sensor:", true, 0.024, "F"),
         SensorData(A0, "", true, 1, "")
    };
    SensorData t2[ 2 ] = {
         SensorData(A0, "Thermistor 02 sensor:", true, 0.022, "F"),
         SensorData(A0, "", true, 1, "")
    };
    SensorData t3[ 2 ] = {
         SensorData(A0, "Thermistor 03 sensor:", true, 0.024, "F"),
         SensorData(A0, "", true, 1, "")
    };
    SensorData unknownID[ 2 ] = {
         SensorData(A0, "Unknown device id!", true, 1, ""),
         SensorData(A0, "", true, 1, "")
    };

    SensorData* getSensors() {
        String id = System.deviceID();
        if (id.equals("1c002c001147343438323536")) {
            return t1;
        }
        if (id.equals("300040001347343438323536")) {
            return t2;
        }
        if (id.equals("290046001147343438323536")) {
            return t3;
        }
        return unknownID;
    }
    
    void publish(String event, String data) {
      Particle.publish(event, data, 1, PRIVATE);
    }

	void sample() {
	    for (SensorData* sensor = getSensors(); !sensor->getName().equals(""); sensor++) {
            sensor->sample();
        }
    }

    void publish() {
	    for (SensorData* sensor = getSensors(); !sensor->getName().equals(""); sensor++) {
            publish(sensor->getName(), sensor->buildPublishString());
        }
    }

    void display() {
        oledWrapper.printTitle(String(Time.format(Time.now(), "%I:%M:%S %p (GMT)")), 1);
        delay(10000);
	    for (SensorData* sensor = getSensors(); !sensor->getName().equals(""); sensor++) {
            oledWrapper.printTitle(sensor->getLastVal(), 3);
            delay(10000);
        }
    }

    void resetVals() {
	    for (SensorData* sensor = getSensors(); !sensor->getName().equals(""); sensor++) {
            sensor->resetVals();
        }
    }

  public:
	SensorTestBed() {
	}

    void sampleSensorData(TimeSync timeSync) {
        // publish one right away to verify that things might be working.
        sample();
        publish();
        display();
        resetVals();
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
            display();
            resetVals();
        }
    }
};

SensorTestBed sensorTestBed;

int setPublish(String command) {
    int temp = command.toInt();
    if (temp > 0) {
        publishIntervalInSeconds = temp;
        return 1;
    }
    return -1;
}

int setSample(String command) {
    int temp = command.toInt();
    if (temp > 0) {
        sampleIntervalInSeconds = temp;
        return 1;
    }
    return -1;
}

void setup() {
    Serial.begin(9600);
    sensorTestBed = SensorTestBed();
    Particle.syncTime();
    Particle.variable("GitHubHash", githubHash);
    Particle.variable("PublishSecs", publishIntervalInSeconds);
    Particle.variable("SampleSecs", sampleIntervalInSeconds);
    Particle.function("SetPublish", setPublish);
    Particle.function("SetSample", setSample);
}

void loop() {
    sensorTestBed.sampleSensorData(TimeSync());
}
