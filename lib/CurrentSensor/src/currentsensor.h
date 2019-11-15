// Please credit : chris.keith@gmail.com
 
#include <limits.h>
#include "application.h"
 
class CurrentSensor {
  public:
    CurrentSensor(int pin, String name, double factor);
    String getName();
    void sample();
    int getValue();
    String buildValueString();
  private:
    int     pin;
    String  name;
    double  factor; // apply to get human-readable values, e.g., degrees F/amps
    int     lastVal;
 
    const unsigned long TIMEOUT_INDICATOR = ULONG_MAX;
    double previousAmps = -1.0;
    unsigned long previousLogTime = ULONG_MAX;
                    // Observed (approximate) amperage when dryer drum is turning.
                    // Also hair dryer on 'Lo'.
    double AMPS = 7.0;
                // When the dryer goes into wrinkle guard mode, it is off for 00:04:45,
                // then powers on for 00:00:15.
    unsigned long wrinkleGuardOff = minSecToMillis(4, 45);
    unsigned long wrinkleGuardOn = minSecToMillis(0, 15);
                    // Give onesself 30 seconds to get to the dryer.
    unsigned int warningInterval = minSecToMillis(0, 30);
 
    unsigned long waitForPowerChange(bool insideRange, const unsigned long timeoutDelta);
    unsigned long waitForPowerOn(const long timeoutDelta);
    unsigned long waitForPowerOff(const long timeoutDelta);
    void doRun();
    double doSample();
    void logAmps(double amps);
    void log(String msg);
    unsigned long minSecToMillis(unsigned long minutes, unsigned long seconds);
                // If any interval takes longer than an hour, something has gone wrong.
    const unsigned long ONE_HOUR = minSecToMillis(60, 0);
    bool insideRangeL(long v1, long v2, long epsilon);
    bool insideRangeD(double v1, double v2, double epsilon);
    bool outsideRangeD(double v1, double v2, double epsilon);
    int applyFactor(int val);
};
