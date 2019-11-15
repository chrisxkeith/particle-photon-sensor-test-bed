#include "application.h"

class SensorData {
  private:
    int     pin;
    String  name;
    double  factor; // apply to get human-readable values, e.g., degrees F
    int     lastVal;
    int     applyFactor(int val);

  public:
    SensorData(int pin, String name, double factor);    
    String getName();
    void sample();    
    int getValue();
    String buildValueString();
};
