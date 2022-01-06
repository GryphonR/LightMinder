#define DEBUG 1

#define LIGHT_LEVEL_CHECK_INTERVAL 1000     // Check Ambient Light Level every x ms
#define LIGHT_FILTER_LENGTH 20              // Number of readings to average ambient light level over

#define LIGHT_TRIGGER_LEVEL_UPPER 600       // ADC Count for ambient light higher schmitt triger band
#define LIGHT_TRIGGER_LEVEL_LOWER 500       // ADC Count for ambient light lower schmitt triger band


#define VOLTAGE_CHECK_INTERVAL 100          // Check Battery Voltage every x ms
#define VOLTAGE_FILTER_LENGTH 15          // Number of readings to average voltage level over

#define VOLTAGE_COUNT_CAL 0.0192

#define VOLTAGE_TRIGGER_LEVEL_UPPER 12.6       // Voltage for higher schmitt triger band
#define VOLTAGE_TRIGGER_LEVEL_LOWER 11.7       // Voltage for lower schmitt triger band

#define FLASH_DURATION 500                  // Duration of light flash in ms

#define OVERRIDE_TIMER 500                  // How long to wait for light to come back on for override functionality

//States
#define SWITCHED_OFF 0
#define SWITCH_ON_LIGHT_ON 1
#define SWITCH_ON_LIGHT_OFF 2
#define FORCE_ON 3
#define FLASH 4