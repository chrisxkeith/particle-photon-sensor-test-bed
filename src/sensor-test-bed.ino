// Please credit chris.keith@gmail.com
const String githubHash = "to be replaced manually in build.particle.io after 'git push'";

// https://opensource.org/licenses/MIT

#include <limits.h>

class JSonizer {
  public:
    static void addFirstSetting(String& json, String key, String val) {
        json.concat("\"");
        json.concat(key);
        json.concat("\":\"");
        json.concat(val);
        json.concat("\"");
    }

    static void addSetting(String& json, String key, String val) {
        json.concat(",");
        addFirstSetting(json, key, val);
    }
    
    String toString(bool b) {
        if (b) {
            return "true";
        }
        return "false";
    }
};

class Utils {
  public:
    static int setInt(String command, int& i, int lower, int upper) {
        int tempMin = command.toInt();
        if (tempMin > lower && tempMin < upper) {
            i = tempMin;
            return 1;
        }
        return -1;
    }
    static void publish(String event, String data) {
        Particle.publish(event, data, 1, PRIVATE);
        // delay(1000); // will be rate-limited if we send more than 1 per second.
    }
};

class TimeSupport {
  private:
    const unsigned long ONE_DAY_IN_MILLISECONDS = 24 * 60 * 60 * 1000;
    unsigned long lastSync = millis(); // store millis() for lastSync as unsigned long
    String timeZoneString;

    String getSettings() {
        String json("{");
        JSonizer::addFirstSetting(json, "lastSync", String(lastSync));
        JSonizer::addSetting(json, "timeZoneOffset", String(timeZoneOffset));
        JSonizer::addSetting(json, "timeZoneString", String(timeZoneString));
        JSonizer::addSetting(json, "internalTime", nowZ());
        json.concat("}");
        return json;
    }

  public:
    int timeZoneOffset;

    TimeSupport(int timeZoneOffset, String timeZoneString) {
        this->timeZoneOffset = timeZoneOffset;
        this->timeZoneString = timeZoneString;
        Time.zone(timeZoneOffset);
        Particle.syncTime();
    }

    String timeStrZ(time_t t) {
        String fmt("%a %b %d %H:%M:%S ");
        fmt.concat(timeZoneString);
        fmt.concat(" %Y");
        return Time.format(t, fmt);
    }

    String nowZ() {
        return timeStrZ(Time.now());
    }

    void handleTime() {
        if (millis() - lastSync > ONE_DAY_IN_MILLISECONDS) {    // If it's been a day since last sync...
                                                                // Request time synchronization from the Particle Cloud
            Particle.syncTime();
            lastSync = millis();
        }
    }

    int setTimeZoneOffset(String command) {
        timeZoneString = "???";
        return Utils::setInt(command, timeZoneOffset, -24, 24);
    }

    void publishJson() {
        Utils::publish("TimeSupport", getSettings());
    }
};
TimeSupport    timeSupport(-8, "PST");

#include <SparkFunMicroOLED.h>
// https://learn.sparkfun.com/tutorials/photon-oled-shield-hookup-guide
#include <math.h>

class OLEDWrapper {
  public:
    MicroOLED oled;

    OLEDWrapper() {
        oled.begin();    // Initialize the OLED
        oled.clear(ALL); // Clear the display's internal memory
        oled.display();  // Display what's in the buffer (splashscreen)
        delay(1000);     // Delay 1000 ms
        oled.clear(PAGE); // Clear the buffer.
    }

    void display(String title, int font, uint8_t x, uint8_t y) {
        oled.clear(PAGE);
        oled.setFontType(font);
        oled.setCursor(x, y);
        oled.print(title);
        oled.display();
    }

    void display(String title, int font) {
        display(title, font, 0, 0);
    }

    void displayNumber(String s) {
        // To reduce OLED burn-in, shift the digits (if possible) on the odd minutes.
        int x = 0;
        if (Time.minute() % 2) {
            const int MAX_DIGITS = 4;
            if (s.length() < MAX_DIGITS) {
                const int FONT_WIDTH = 12;
                x += FONT_WIDTH * (MAX_DIGITS - s.length());
            }
        }
        display(s, 3, x, 0);
    }

    void publishJson() {
        String json("{");
        JSonizer::addFirstSetting(json, "getLCDWidth()", String(oled.getLCDWidth()));
        JSonizer::addSetting(json, "getLCDHeight()", String(oled.getLCDHeight()));
        json.concat("}");
        Utils::publish("OLED", json);
    }

    void clear() {
      oled.clear(ALL);
    }
};
OLEDWrapper oledWrapper;

class SensorData {
  private:
    int     lastVal;
  protected:
    int     pin;
    String  name;
    double  factor; // apply to get human-readable values, e.g., degrees F

  public:
    SensorData(int pin, String name, double factor) {
        this->pin = pin;
        this->name = name;
        this->factor = factor;
        this->lastVal = INT_MIN;
        pinMode(pin, INPUT);
    }
    
    String getName() { return name; }

    void sample() {
        if (pin >= A0 && pin <= A5) {
            lastVal = analogRead(pin);
        } else {
            lastVal = digitalRead(pin);
        }
    }
    
    int applyFactor(int val) {
        return val * factor;
    }

    int getValue() {
        return applyFactor(lastVal);
    }

    String buildValueString() {
        return String(getValue());
    }
};

class CurrentSensor : public SensorData {
  public:
    CurrentSensor(int pin, String name, double factor) :
        SensorData(pin, name, factor) {
    }
    void sample();
    double getDoubleValue() { return previousAmps; }
    int getValue() { return round(previousAmps); }

  private:
    // Observed (approximate) amperage when clothes dryer drum is turning.
    // Also hair dryer on 'Lo'.
    // double POWER_ON_AMPS = 7.0; TODO : will this be needed?

    double previousAmps = -1.0;
    long previousSampleTime = 0L;

    enum DryerState {
        noLoadBeingDried,
        inDryingCycle,
        inWrinkleGuardOffCycle,
        inWrinkleGuardOnCycle
    };
    // Assume that sensor unit has been initialized while dryer is off.
    DryerState dryerState = noLoadBeingDried;
    long lastStateChangeTime = 0L;

    int numWrinkleGuardCycles = 0;
    int cycleWhenMessageWasSent = 0;

    bool outsideRangeD(double v1, double v2, double epsilon) {
        return abs(v1 - v2) > epsilon;
    }
    enum PowerStateChange {
        power_off,
        power_on,
        no_change
    };
    PowerStateChange powerChanged(double amps);
    void checkState(PowerStateChange powerStateChange);
    void sendMessageIfNecessary();

    unsigned long minSecToMillis(unsigned long minutes, unsigned long seconds) {
    return (minutes * 60 * 1000) + (seconds * 1000);
    }
    const unsigned long WRINKLE_GUARD_OFF_CYCLE = minSecToMillis(4, 45);
    const unsigned long WRINKLE_GUARD_ON_CYCLE = minSecToMillis(0, 15);
    const unsigned long ONE_HOUR = minSecToMillis(60, 0);
};

CurrentSensor::PowerStateChange CurrentSensor::powerChanged(double amps) {
    if (outsideRangeD(amps, previousAmps, 0.5)) { // Need a change of at last .5 amps to be a 'real' (e.g., not noise) change.
        if (amps > previousAmps) {
            return power_on;
        }
        return power_off;
    }
    return no_change;
}

void CurrentSensor::sendMessageIfNecessary() {
    if (dryerState == inWrinkleGuardOffCycle &&
                cycleWhenMessageWasSent > numWrinkleGuardCycles &&
                millis() - lastStateChangeTime > WRINKLE_GUARD_OFF_CYCLE) {
        String msg("Dryer will start 15 second tumble cycle in ");
        msg.concat(0);
        msg.concat(" seconds.");
        Utils::publish(name, msg);
        cycleWhenMessageWasSent = numWrinkleGuardCycles;
    }
}

void CurrentSensor::checkState(PowerStateChange powerStateChange) {
    // Assume that the only power-on interval that takes 'wrinkleGuardOn' milliseconds
    // is indeed the wrinkle guard interval (and not some other stage in the cycle).
    switch(dryerState) {
        case noLoadBeingDried:
            dryerState = inDryingCycle;
            break;
        case inDryingCycle:
            dryerState = inWrinkleGuardOffCycle;
            break;
        case inWrinkleGuardOffCycle:
            // If power off for longer than wrinkle guard cycle, assume it's all done.
            // TODO : Do I need to add a fudge factor here?
            if (millis() - this->lastStateChangeTime > (WRINKLE_GUARD_OFF_CYCLE + WRINKLE_GUARD_ON_CYCLE)) {
                dryerState = noLoadBeingDried;
                numWrinkleGuardCycles = 0;
                cycleWhenMessageWasSent = 0;
            } else {
                dryerState = inWrinkleGuardOnCycle;
            }
            break;
        case inWrinkleGuardOnCycle:
            dryerState = inWrinkleGuardOffCycle;
            break;
        default:
            String err("Unknown dryerState: ");
            err.concat(dryerState);
            Utils::publish(name, err);
            break;
    }
    lastStateChangeTime = millis();
}

void CurrentSensor::sample() {
  int minVal = 1023;
  int maxVal = 0;

  // Assume we can read a voltage every 10 ms or so.
  // To avoid blocking the whole Photon for too long,
  // only read for 1/30 of a second,
  // roughly an interval of two 60 cycles/second.
  const int iters = 1000 / 30;
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
  PowerStateChange change = powerChanged(amps); 
  if (change != no_change) {
      checkState(change);
      previousAmps = amps;
      previousSampleTime = millis();
  }
  sendMessageIfNecessary();
}

class SensorTestBed {
  private:

    int publishIntervalInSeconds = 5;
    int nextPublish = publishIntervalInSeconds - (Time.now() % publishIntervalInSeconds);

    // Names should be unique for reporting purposes.
    // Names should contain the string "sensor".
    // A unique number is recommended (use Photon #), e.g., "Thermistor sensor 01".
    SensorData t1[ 2 ] = {
         SensorData(A0, "Thermistor sensor 1", 0.036),
         SensorData(A0, "", 1)
    };
    SensorData p6[ 2 ] = {
         SensorData(A0, "Current sensor 6", 0.036),
         SensorData(A0, "", 1)
    };
    SensorData unknownID[ 2 ] = {
         SensorData(A0, "Unknown device id!", 1),
         SensorData(A0, "", 1)
    };

    String thermistor_test  = "1c002c001147343438323536";
    String photon_06  = "290048001647363335343834";

    SensorData* getSensors() {
        String id = System.deviceID();
        if (id.equals(thermistor_test)) {
            return t1;
        }
        if (id.equals(photon_06)) {
            return p6;
        }
        return unknownID;
    }
    
	void sample() {
	    for (SensorData* sensor = getSensors(); !sensor->getName().equals(""); sensor++) {
            sensor->sample();
        }
    }

    // Publish at intervals relative to midnight (Unix epoch).
    // This makes it easier to compare sensor data from multiple Photons/devices.
    void setNextPublish() {
        int now = Time.now();
        nextPublish = now + (publishIntervalInSeconds - (now % publishIntervalInSeconds));
    }

  public:
	SensorTestBed() {
	    setNextPublish();
	}

    // Publish when changed, but not more often than minimum requested rate
    // (and never faster than 1/second, which is a Particle cloud restriction.)
    void publish() {
        String json("{");
        bool first = true;
	    for (SensorData* sensor = getSensors(); !sensor->getName().equals(""); sensor++) {
            Utils::publish(sensor->getName(), sensor->buildValueString());
            if (first) {
                first = false;
            } else {
                json.concat(",");
            }
            json.concat("\"");
            json.concat(sensor->getName());
            json.concat("\":\"");
            json.concat(sensor->buildValueString());
            json.concat("\"");
        }
        json.concat("}");
    }

    int getValue(String sensorName) {
	    for (SensorData* sensor = getSensors(); !sensor->getName().equals(""); sensor++) {
            if (sensor->getName().equals(sensorName)) {
                return sensor->getValue();
            }
        }
        return 999;
    }

    void sampleSensorData() {
        sample();
        if (nextPublish <= Time.now()) {
            publish();
            setNextPublish();
        }
    }

    void publishJson() {
        int     i = 0;
        for (SensorData* sensor = getSensors(); !sensor->getName().equals(""); sensor++) {
            i++;
        }
        String  json("{");
        JSonizer::addFirstSetting(json, "numberOfDevices", String(i));
        JSonizer::addSetting(json, "publishIntervalInSeconds", String(publishIntervalInSeconds));
        JSonizer::addSetting(json, "nextPublish", timeSupport.timeStrZ(nextPublish));
        json.concat("}");
        Utils::publish("TestBed", json);
    }

    int setPublish(String command) {
        int temp = command.toInt();
        if (temp > 0) {
            publishIntervalInSeconds = temp;
            setNextPublish();
            return 1;
        }
        return -1;
    }
};

SensorTestBed sensorTestBed = SensorTestBed();

int setPublish(String command) {
    return sensorTestBed.setPublish(command);
}

class OLEDDisplayer {
  private:
    int   tempToBlinkInF = 90;  // If at this temperature or above, blink the temperature display.
    bool    invert = true;
  public:
    void display() {
        int temp = sensorTestBed.getValue("Thermistor sensor 1");
        if (temp >= tempToBlinkInF) {
          oledWrapper.oled.invert(invert);
          invert = !invert;
        } else {
            if (!invert) {
                // If going out of "blink" mode, make sure background is black.
                oledWrapper.oled.invert(false);
                invert = true;
            }
        }
        oledWrapper.displayNumber(String(temp));
        delay(500);
    }

    int setDispTemp(String cmd) {
      this->tempToBlinkInF = atoi(cmd);
      return 1;
    }

    void publishJson() {
      String json("{");
      JSonizer::addFirstSetting(json, "tempToBlinkInF", String(tempToBlinkInF));
      json.concat("}");
      Utils::publish("OLEDDisplayer json", json);
    }
};
OLEDDisplayer oledDisplayer;

int setDispTemp(String command) {
  return oledDisplayer.setDispTemp(command);
}

// build.particle.io may think that this has timed out if there are more than a couple of sensors
// (and give the "Bummer..." error), but it does finish.
int publishVals(String command) {
    sensorTestBed.publish();
    return 1;
}

String getOverallSettings() {
    String json("{");
    JSonizer::addFirstSetting(json, "githubHash", githubHash);
    json.concat("}");
    return json;
}

// getSettings() is already defined somewhere.
int pubSettings(String command) {
    if (command.compareTo("") == 0) {
        Utils::publish("Settings", getOverallSettings());
    } else if (command.compareTo("time") == 0) {
        timeSupport.publishJson();
    } else if (command.compareTo("testbed") == 0) {
        sensorTestBed.publishJson();
    } else if (command.compareTo("display") == 0) {
        oledDisplayer.publishJson();
    } else {
        Utils::publish("GetSettings bad input", command);
    }
    return 1;
}

int rawPublish(String command) {
    String  event(command);
    String  data(command);
    int     separatorIndex = command.indexOf(":");
    if (separatorIndex > 0) { // Not zero, need at least one character for event.
        event = command.substring(0, separatorIndex);
        data = command.substring(separatorIndex + 1);
    } else {
        int     separatorIndex = command.indexOf("=");
        if (separatorIndex > 0) { // Not zero, need at least one character for event.
          event = command.substring(0, separatorIndex);
          data = command.substring(separatorIndex + 1);
      }
    }
    Utils::publish(event, data);
    return 1;
}

void setup() {
    Particle.function("GetSettings", pubSettings);
    Particle.function("SetPublish",  setPublish);
    Particle.function("PublishVals", publishVals);
    Particle.function("RawPublish",  rawPublish);
    Particle.function("setDispTemp", setDispTemp);
    sensorTestBed.sampleSensorData();
    sensorTestBed.publish();
    oledDisplayer.display();
    Particle.publish("Debug", "Finished setup...", 1, PRIVATE);
}

void loop() {
    timeSupport.handleTime();
    sensorTestBed.sampleSensorData();
    oledDisplayer.display();
}
