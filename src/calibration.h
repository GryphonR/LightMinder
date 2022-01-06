#define DEBUG 1

#define LIGHT_LEVEL_CHECK_INTERVAL 1000     // Check Ambient Light Level every x ms
#define LIGHT_FILTER_LENGTH 20              // Number of readings to average ambient light level over

#define LIGHT_TRIGGER_LEVEL_UPPER 600       // ADC Count for ambient light higher schmitt triger band
#define LIGHT_TRIGGER_LEVEL_LOWER 500       // ADC Count for ambient light lower schmitt triger band


#define VOLTAGE_CHECK_INTERVAL 100          // Check Battery Voltage every x ms
#define VOLTAGE_FILTER_LENGTH 15          // Number of readings to average voltage level over

// #define VOLTAGE_COUNT_CAL 0.015947 //16.33/1024
#define VOLTAGE_COUNT_CAL 0.01609045 //16.01/995

#define TRIGGER_POINT_6_12_VOLTS 7      // boundary for detecing system voltage level on startup

#define VOLTAGE_12_TRIGGER_LEVEL_UPPER 12.6       // Voltage for 12v higher schmitt triger band
#define VOLTAGE_12_TRIGGER_LEVEL_LOWER 11.7       // Voltage for 12v lower schmitt triger band

#define VOLTAGE_6_TRIGGER_LEVEL_UPPER 6.6         // Voltage for higher schmitt triger band
#define VOLTAGE_6_TRIGGER_LEVEL_LOWER 5.7         // Voltage for lower schmitt triger band

#define LIGHT_LEVEL_CHANGE_TIME 10                // mS between PWM light output level changes
#define LIGHT_LEVEL_CHANGE_STEP 3              // PWM counts to adjust light output level by each step

#define FLASH_DURATION 800                  // Duration of light flash in ms

#define OVERRIDE_TIMER 500                  // How long to wait for light to come back on for override functionality