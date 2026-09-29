#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// --- Pin Definitions ---
#define SWITCH_PIN       5   // On/Off Switch
#define LED_POWER        6   // System Power LED
#define LED_PRESSURE     7   // High Pressure Warning LED
#define LED_HUMIDITY     8   // High Humidity Warning LED
#define LED_TEMP         9   // Temperature Out-of-Bounds LED

#define PUMP1_PIN        3   // Pump 1 Driver Pin
#define PUMP2_PIN        4   // Pump 2 Driver Pin

#define SEALEVELPRESSURE_HPA (1013.25)

// --- Global Sensor Instance ---
Adafruit_BME280 bme; // I2C Interface

// --- Thresholds & Timers ---
const float h_threshold = 70.0;     // % Relative Humidity limit
const float p_threshold = 6894.76; // Pascals (~1 psi)
const float temp_max = 60.0;        // °C
const float temp_min = -20.0;       // °C

const unsigned long pressure_wait_time = 10UL * 60UL * 1000UL; // 10 minutes in ms
bool pressure_safe = true;
unsigned long delayTime = 1000;     // 1 second main loop delay

void setup() {
    Serial.begin(9600);
    while (!Serial); // Wait for Serial console connection
    Serial.println(F("BME280 Pump & Sensor Controller Test"));

    // Configure Indicator LEDs
    pinMode(LED_POWER, OUTPUT);
    pinMode(LED_PRESSURE, OUTPUT);
    pinMode(LED_HUMIDITY, OUTPUT);
    pinMode(LED_TEMP, OUTPUT);

    // Configure Switch & Pumps
    pinMode(SWITCH_PIN, INPUT_PULLUP);
    pinMode(PUMP1_PIN, OUTPUT);
    pinMode(PUMP2_PIN, OUTPUT);

    // Initial Output States (All OFF)
    digitalWrite(LED_POWER, LOW);
    digitalWrite(LED_PRESSURE, LOW);
    digitalWrite(LED_HUMIDITY, LOW);
    digitalWrite(LED_TEMP, LOW);
    digitalWrite(PUMP1_PIN, LOW);
    digitalWrite(PUMP2_PIN, LOW);

    // Initialize BME280 Sensor
    if (!bme.begin(0x76)) { // Default I2C address is 0x76 (or 0x77)
        Serial.println(F("Error: Could not find a valid BME280 sensor! Check wiring."));
        while (1); // Halt execution
    }

    Serial.println(F("Setup Complete. System Ready."));
}

void loop() {
    // 1. Check On/Off Switch State (Active LOW with INPUT_PULLUP)
    bool switch_state = (digitalRead(SWITCH_PIN) == LOW);

    if (!switch_state) {
        // System is turned OFF: shutdown pumps and clear indicators
        digitalWrite(PUMP1_PIN, LOW);
        digitalWrite(PUMP2_PIN, LOW);
        digitalWrite(LED_POWER, LOW);
        digitalWrite(LED_PRESSURE, LOW);
        digitalWrite(LED_HUMIDITY, LOW);
        digitalWrite(LED_TEMP, LOW);
        delay(200);
        return;
    }

    // Indicate active system power
    digitalWrite(LED_POWER, HIGH);

    // 2. Check Pressure Safety Lockout
    if (!pressure_safe) {
        digitalWrite(PUMP1_PIN, LOW);
        digitalWrite(PUMP2_PIN, LOW);
        digitalWrite(LED_PRESSURE, HIGH); // Keep pressure error LED latched ON
        Serial.println(F("LOCKOUT: Pressure remained high for >10 mins. Restart system."));
        delay(1000);
        return;
    }

    // 3. Turn ON Pumps
    digitalWrite(PUMP1_PIN, HIGH);
    digitalWrite(PUMP2_PIN, HIGH);

    // 4. Read Sensor Metrics
    float temp = bme.readTemperature();
    float pressure = bme.readPressure(); // Value in Pascals
    float humidity = bme.readHumidity();

    // 5. Humidity Threshold Evaluation
    if (humidity > h_threshold) {
        digitalWrite(LED_HUMIDITY, HIGH);
    } else {
        digitalWrite(LED_HUMIDITY, LOW);
    }

    // 6. Over-Pressure Monitoring Loop
    if (pressure > p_threshold) {
        unsigned long pressure_time = millis();

        while (pressure > p_threshold) {
            digitalWrite(LED_PRESSURE, HIGH); // Illuminate warning LED
            
            // Periodically re-evaluate sensor pressure
            delay(500);
            pressure = bme.readPressure();

            // Check if high-pressure condition has persisted for > 10 minutes
            if ((millis() - pressure_time) > pressure_wait_time) {
                Serial.println(F("ERROR: PRESSURE TOO HIGH FOR 10 MINUTES! SHUTTING DOWN."));
                pressure_safe = false;
                digitalWrite(PUMP1_PIN, LOW);
                digitalWrite(PUMP2_PIN, LOW);
                return;
            }
        }
        // Pressure returned to safe levels before timeout
        digitalWrite(LED_PRESSURE, LOW);
    }

    // 7. Temperature Range Evaluation
    // Triggers if temperature is OUTSIDE the safe bounds (< -20°C or > 60°C)
    if (temp > temp_max || temp < temp_min) {
        digitalWrite(LED_TEMP, HIGH);
    } else {
        digitalWrite(LED_TEMP, LOW);
    }

    // Print active values to Serial Monitor
    readSensors();

    delay(delayTime);
}

// Helper Function for Console Telemetry
void readSensors() {
    Serial.print(F("Temperature = "));
    Serial.print(bme.readTemperature());
    Serial.println(F(" °C"));

    Serial.print(F("Pressure = "));
    Serial.print(bme.readPressure() / 100.0F);
    Serial.println(F(" hPa"));

    Serial.print(F("Approx. Altitude = "));
    Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
    Serial.println(F(" m"));

    Serial.print(F("Humidity = "));
    Serial.print(bme.readHumidity());
    Serial.println(F(" %"));

    Serial.println();
}