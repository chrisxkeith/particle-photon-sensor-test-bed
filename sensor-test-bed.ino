// Please credit chris.keith@gmail.com
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

String sensor0Data = String("no sensor0");
String sensor1Data = String("no sensor1");
String sensor2Data = String("no sensor2");
String sensor3Data = String("no sensor2");
String sensor4Data = String("no sensor2");
String sensor5Data = String("no sensor2");

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

    bool sample() {
        int nextVal;
        if (isAnalog) {
            nextVal = analogRead(pin);
        } else {
            nextVal = digitalRead(pin);
        }
        bool changed = ((lastVal != INT_MIN) && (applyFactor(lastVal) != applyFactor(nextVal)));
        lastVal = nextVal;
        minVal = min(lastVal, minVal);
        maxVal = max(lastVal, minVal);
        sum += lastVal;
        sumCount++;
        return changed;
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
int sampleIntervalInSeconds = 1;

class SensorTestBed {
  private:
    
    SensorData t1[ 2 ] = {
         SensorData(A0, "Thermistor 01 sensor:", true, 0.036, "F"),
         SensorData(A0, "", true, 1, "")
    };
    SensorData t2[ 2 ] = {
         SensorData(A0, "Thermistor 02 sensor:", true, 0.022, "F"),
         SensorData(A0, "", true, 1, "")
    };
    SensorData t3[ 2 ] = {
         SensorData(A0, "Thermistor 03 sensor:", true, 0.024, "F"),
         // A2 belongs to OLED.
         // A3 belongs to SPI/I2C.
         // A5 belongs to SPI/I2C.
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

	bool sample() {
	    bool changed = false;
	    for (SensorData* sensor = getSensors(); !sensor->getName().equals(""); sensor++) {
            if (sensor->sample()) {
                changed = true;
            }
        }
        return changed;
    }

    void publish() {
        int i = 0;
	    for (SensorData* sensor = getSensors(); !sensor->getName().equals(""); sensor++) {
            publish(sensor->getName(), sensor->buildPublishString());
            switch (i++) {
               case  0 : sensor0Data = sensor->buildPublishString(); break;
               case  1 : sensor1Data = sensor->buildPublishString(); break;
               case  2 : sensor2Data = sensor->buildPublishString(); break;
               case  3 : sensor3Data = sensor->buildPublishString(); break;
               case  4 : sensor4Data = sensor->buildPublishString(); break;
               case  5 : sensor5Data = sensor->buildPublishString(); break;
            }
        }
    }

    void displayTime() {
        oledWrapper.printTitle(String(Time.format("%H:%M:%S UTC")), 1);
        delay(5000);
    }

    void displayValues() {
        bool first = true;
	    for (SensorData* sensor = getSensors(); !sensor->getName().equals(""); sensor++) {
	        if (first) {
	            first = false;
	        } else {
                oledWrapper.printTitle(String("0000000000"), 3);
                delay(5000);
	        }
            oledWrapper.printTitle(sensor->getLastVal(), 3);
            delay(5000);
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
        displayTime();
        displayValues();
        resetVals();
        while (true) {
            // Report on even intervals starting at midnight.
            int nextPublish = publishIntervalInSeconds - (Time.now() % publishIntervalInSeconds);
            while (nextPublish > 0) {
                timeSync.sync();
                // If any sensor data changed, display it immediately.
                if (sample()) {
                    displayValues();
                }
                delay(min(sampleIntervalInSeconds, nextPublish) * 1000);
                nextPublish -= sampleIntervalInSeconds;
            }
            publish();
            displayTime();
            displayValues();
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
    Particle.variable("sensor0Data", sensor0Data);
    Particle.variable("sensor1Data", sensor1Data);
    Particle.variable("sensor2Data", sensor2Data);
    Particle.variable("sensor3Data", sensor3Data);
    Particle.variable("sensor4Data", sensor4Data);
    Particle.variable("sensor5Data", sensor5Data);
    Particle.function("SetPublish", setPublish);
    Particle.function("SetSample", setSample);
}

void loop() {
    sensorTestBed.sampleSensorData(TimeSync());
}
