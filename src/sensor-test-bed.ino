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
        delay(1000); // will be rate-limited if we send more than 1 per second.
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
  private:
    MicroOLED oled;

  public:
    bool    weighted = false;

    OLEDWrapper() {
        oled.begin();    // Initialize the OLED
        oled.clear(ALL); // Clear the display's internal memory
        oled.display();  // Display what's in the buffer (splashscreen)
        delay(1000);     // Delay 1000 ms
        oled.clear(PAGE); // Clear the buffer.
    }

    void display(String title, int font)
    {
      oled.clear(PAGE);
      oled.setFontType(font);
      oled.setCursor(0, 0);
      oled.print(title);
      oled.display();
    }

    bool errShown = false;
    void verify(int xStart, int yStart, int xi, int yi) {
      if (!errShown && (xi >= oled.getLCDWidth() || yi >= oled.getLCDHeight())) {
        String json("{");
        JSonizer::addSetting(json, "xStart", String(xStart));
        JSonizer::addSetting(json, "yStart", String(yStart));
        JSonizer::addSetting(json, "xi", String(xi));
        JSonizer::addSetting(json, "yi", String(yi));
        json.concat("}");
        Utils::publish("super-pixel coordinates out of range", json);
        errShown = true;
      }
    }

   void superPixel(int xStart, int yStart, int xSuperPixelSize, int ySuperPixelSize, int pixelVal,
         int left, int right, int top, int bottom) {
     int pixelSize = xSuperPixelSize * ySuperPixelSize;
     if (pixelVal < 0) {
       pixelVal = 0;
     } else if (pixelVal >= pixelSize) {
       pixelVal = pixelSize - 1;
     }
     for (int xi = xStart; xi < xStart + xSuperPixelSize; xi++) {
       for (int yi = yStart; yi < yStart + ySuperPixelSize; yi++) {
         verify(xStart, yStart, xi, yi);

         // Value between 1 and pixelSize - 2,
         // so pixelVal of 0 will have all pixels off
         // and pixelVal of pixelSize - 1 will have all pixels on. 
         int r = (rand() % (pixelSize - 2)) + 1;
         if (weighted) {
            float xfactor;
            if (xi >= xStart + (xSuperPixelSize / 2)) {
                xfactor = left;
            } else {
                xfactor = right;
            }
            float yfactor;
            if (yi >= yStart + (ySuperPixelSize / 2)) {
                yfactor = top;
            } else {
                yfactor = bottom;
            }
            // how much the adjacent super-pixels will affect the current one.
            float weight = ((xfactor + yfactor) / 2.0);

            // width/height size of super-pixel.
            float size = ((xSuperPixelSize + ySuperPixelSize) / 2.0);

            // distance of this pixel from the center of the super-pixel.
            float xdistance = abs(xi - (xStart + (xSuperPixelSize / 2.0)));
            float ydistance = abs(yi - (yStart + (ySuperPixelSize / 2.0)));
            float distance = (xdistance + ydistance) / 2.0;
            int r = rand() * (pixelVal+ (weight * (distance / size)));
         }
         if (r < pixelVal) { // lower value maps to white pixel.
           oled.pixel(xi, yi);
         }
       }
     }
   }

    void publishJson() {
        String json("{");
        JSonizer::addFirstSetting(json, "getLCDWidth()", String(oled.getLCDWidth()));
        JSonizer::addSetting(json, "getLCDHeight()", String(oled.getLCDHeight()));
        json.concat("}");
        Utils::publish("OLED", json);
    }

    void testPattern() {
      int xSuperPixelSize = 6;	
      int ySuperPixelSize = 6;
      int pixelSize = xSuperPixelSize * ySuperPixelSize; 
      float diagonalDistance = sqrt((float)(xSuperPixelSize * xSuperPixelSize + ySuperPixelSize * ySuperPixelSize));
      float factor = (float)pixelSize / diagonalDistance;
      int pixelVals[64];
      for (int i = 0; i < 64; i++) {
        int x = (i % 8);
        int y = (i / 8);
        pixelVals[i] = (int)(round(sqrt((float)(x * x + y * y)) * factor));
      }
      displayArray(xSuperPixelSize, ySuperPixelSize, pixelVals);
      delay(5000);
      this->weighted = true;
      displayArray(xSuperPixelSize, ySuperPixelSize, pixelVals);
      delay(5000);	
    }

    void displayArray(int xSuperPixelSize, int ySuperPixelSize, int pixelVals[]) {
      oled.clear(PAGE);
      for (int i = 0; i < 64; i++) {
        int x = (i % 8) * xSuperPixelSize;
        int y = (i / 8) * ySuperPixelSize;
        int left = x;
        int right = x;
        int top = y;
        int bottom = y;
        if (x > 0) {
          left = pixelVals[i - 1];
        }
        if (x < 7) {
          right = pixelVals[i + 1];
        }
        if (y > 0) {
          top = pixelVals[i - 8];
        }
        if (y < 7) {
          top = pixelVals[i + 8];
        }
        // This (admittedly confusing) switcheroo of x and y axes is to make the orientation
        // of the sensor (with logo reading correctly) match the orientation of the OLED.
        superPixel(y, x, ySuperPixelSize, xSuperPixelSize, pixelVals[i],
            left, right, top, bottom);
      }
      oled.display();
    }
};
OLEDWrapper oledWrapper;

class SensorData {
  private:
    int     pin;
    String  name;
    double  factor; // apply to get human-readable values, e.g., degrees F
    int     lastVal;
    double  accumulatedVals;
    int     nAccumulatedVals;
    int     lastSampleTime;

  public:
    SensorData(int pin, String name, double factor) {
        this->pin = pin;
        this->name = name;
        this->factor = factor;
        this->lastVal = INT_MIN;
        this->accumulatedVals = 0.0;
        this->nAccumulatedVals = 0;
        this->lastSampleTime = 0;
        pinMode(pin, INPUT);
    }
    
    String getName() { return name; }

    bool sample() {
        int nextSampledVal;
        if (pin >= A0 && pin <= A5) {
            nextSampledVal = analogRead(pin);
        } else {
            nextSampledVal = digitalRead(pin);
        }
        int now = Time.now();

        if (lastSampleTime == 0) {
            accumulatedVals += nextSampledVal;
            nAccumulatedVals++;
        } else if (now < lastSampleTime + 1) {
        // Take average of all samples over 1 second interval.
            accumulatedVals += nextSampledVal;
            nAccumulatedVals++;
            return false;
        }
        int nextVal = accumulatedVals / nAccumulatedVals;
        bool changed = ((applyFactor(lastVal) != applyFactor(nextVal)));
        lastVal = nextVal;
        accumulatedVals = lastVal;
        nAccumulatedVals = 1;
        lastSampleTime = now;
        return changed;
    }
    
    int applyFactor(int val) {
        return val * factor;
    }

    String buildValueString() {
        return String(applyFactor(lastVal));
    }
};

class SensorTestBed {
  private:

    int publishIntervalInSeconds = 5;
    int nextPublish = publishIntervalInSeconds - (Time.now() % publishIntervalInSeconds);

    // Names should unique for reporting purposes.
    // Names should also contain the string "sensor".
    // Also, a unique number is recommended, e.g., "Thermistor sensor 01".
    SensorData t1[ 2 ] = {
         SensorData(A0, "Thermistor 01 sensor:", 0.036),
         SensorData(A0, "", 1)
    };
    SensorData t2[ 2 ] = {
         SensorData(A0, "Thermistor 02 sensor:", 0.036),
         SensorData(A0, "", 1)
    };
    SensorData t5[ 2 ] = {
         SensorData(A0, "Thermistor 05 sensor:", 0.036),
         SensorData(A0, "", 1)
    };
    SensorData t6[ 2 ] = {
         SensorData(A0, "Thermistor 06 sensor:", 0.036),
         // A2 belongs to OLED.
         // A3 belongs to SPI/I2C.
         // A5 belongs to SPI/I2C.
         SensorData(A0, "", 1)
    };
    SensorData unknownID[ 2 ] = {
         SensorData(A0, "Unknown device id!", 1),
         SensorData(A0, "", 1)
    };

    String thermistor_test  = "1c002c001147343438323536";
    String thermistor2_test = "300040001347343438323536";
    String photon_05        = "19002a001347363336383438";
    String photon_06        = "290048001647363335343834";

    SensorData* getSensors() {
        String id = System.deviceID();
        if (id.equals(thermistor_test)) {
            return t1;
        }
        if (id.equals(thermistor2_test)) {
            return t2;
        }
        if (id.equals(photon_05)) {
            return t5;
        }
        if (id.equals(photon_06)) {
            return t6;
        }
        return unknownID;
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

    void displayValues() {
        bool first = true;
	    for (SensorData* sensor = getSensors(); !sensor->getName().equals(""); sensor++) {
	        if (first) {
	            first = false;
	        } else {
                oledWrapper.display(String("0000000000"), 3);
                delay(2000);
	        }
            oledWrapper.display(sensor->buildValueString(), 3);
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

    // For many sensors, sampling more than necessary shouldn't be a problem.
    // For sensors like a moisture sensor, however, sampling should only happen when necessary.
    // See https://learn.sparkfun.com/tutorials/soil-moisture-sensor-hookup-guide .
    void sampleSensorData() {
        // If any sensor data changed, display it immediately.
        if (sample()) {
            displayValues();
        }
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

int testPatt(String command) {	
  oledWrapper.testPattern();	
  return 1;	
}

void setup() {
    Particle.function("GetSettings", pubSettings);
    Particle.function("SetPublish",  setPublish);
    Particle.function("PublishVals", publishVals);
    Particle.function("RawPublish",  rawPublish);
    Particle.function("testPattern", testPatt);
    sensorTestBed.sampleSensorData();
    sensorTestBed.publish();
}

void loop() {
    timeSupport.handleTime();
    sensorTestBed.sampleSensorData();
}
