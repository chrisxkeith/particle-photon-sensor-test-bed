// Please credit chris.keith@gmail.com
// This #include statement was automatically added by the Particle IDE.
#include <SparkFunMicroOLED.h>
// https://learn.sparkfun.com/tutorials/photon-oled-shield-hookup-guide
#include <math.h>

// https://opensource.org/licenses/MIT

const String githubHash = "to be replaced manually in build.particle.io after 'git push'";

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
        return changed;
    }
    
    void resetVals() {
        lastVal = INT_MIN;
    }

    int applyFactor(int val) {
        return val * factor;
    }

    String buildValueString() {
        return String(applyFactor(lastVal));
    }
};

int publishIntervalInSeconds = 2 * 60;
int nextPublish = publishIntervalInSeconds - (Time.now() % publishIntervalInSeconds);
String internalTime = "";

class SensorTestBed {
  private:
    
    SensorData t1[ 3 ] = {
         SensorData(A0, "Thermistor 01 sensor:", true, 0.036, "F"),
         SensorData(A1, "Thermistor 01b sensor:", true, 0.036, "F"),
         SensorData(A0, "", true, 1, "")
    };
    SensorData t2[ 2 ] = {
         SensorData(A0, "Thermistor 02 sensor:", true, 0.036, "F"),
         SensorData(A0, "", true, 1, "")
    };
    SensorData t3[ 2 ] = {
         SensorData(A0, "Thermistor 03 sensor:", true, 0.036, "F"),
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

    void publish() {
        int i = 0;
	    for (SensorData* sensor = getSensors(); !sensor->getName().equals(""); sensor++) {
            publish(sensor->getName(), sensor->buildValueString());
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

	bool sample() {
	    bool changed = false;
	    for (SensorData* sensor = getSensors(); !sensor->getName().equals(""); sensor++) {
            if (sensor->sample()) {
                changed = true;
            }
        }
        return changed;
    }

    void display() {
        displayTime();
        displayValues();
        publish();
        resetVals();
    }

    void setNextPublish() {
        nextPublish = publishIntervalInSeconds - (Time.now() % publishIntervalInSeconds);
    }

    void sampleSensorData() {
        // If any sensor data changed, display it immediately.
        if (sample()) {
            display();
        } else if (nextPublish >= Time.now()) {
            display();
            setNextPublish();
        }
    }
};

SensorTestBed sensorTestBed;

int setPublish(String command) {
    int temp = command.toInt();
    if (temp > 0) {
        publishIntervalInSeconds = temp;
        sensorTestBed.setNextPublish();
        sensorTestBed.sample();
        return 1;
    }
    return -1;
}

// build.particle.io will think that this has timed out
// (and give the "Bummer..." error), but it does finish.
int displayVals(String command) {
    sensorTestBed.display();
    return 1;
}

void setup() {
    Serial.begin(9600);
    Particle.syncTime();
    Particle.variable("GitHubHash", githubHash);
    Particle.variable("PublishSecs", publishIntervalInSeconds);
    Particle.variable("InternalTime", internalTime);
    Particle.function("SetPublish", setPublish);
    Particle.function("DisplayVals", displayVals);

    sensorTestBed = SensorTestBed();
    sensorTestBed.sample();
    sensorTestBed.display();
}

TimeSync timeSync = TimeSync();
void loop() {
    timeSync.sync();
    internalTime = Time.format(Time.now(), TIME_FORMAT_ISO8601_FULL);
    sensorTestBed.sampleSensorData();
}
