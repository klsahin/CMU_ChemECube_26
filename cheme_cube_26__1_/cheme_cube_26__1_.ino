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

// --- Dual Sensor Instances ---
Adafruit_BME280 bme0; // Primary Sensor  (Address 0x76)
Adafruit_BME280 bme1; // Secondary Sensor (Address 0x77)

// --- Thresholds & Timers ---
const float h_threshold = 70.0;     // % Relative Humidity limit
const float p_threshold = 105000.0; // Pascals (~15.23 psi absolute)
const float temp_max = 60.0;        // °C
const float temp_min = -20.0;       // °C

const unsigned long pressure_wait_time = 10UL * 60UL * 1000UL; // 10 minutes in ms
bool pressure_safe = true;
unsigned long delayTime = 1000;     // 1 second main loop delay

void setup() {
    Serial.begin(9600);
    while (!Serial); // Wait for Serial console connection
    Serial.println(F("Dual BME280 Pump & Sensor Controller Test"));

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

    // Initialize Sensor 0 (0x76)
    if (!bme0.begin(0x76)) {
        Serial.println(F("Error: Could not find Sensor 0 at address 0x76!"));
        while (1); // Halt execution
    }

    // Initialize Sensor 1 (0x77)
    if (!bme1.begin(0x77)) {
        Serial.println(F("Error: Could not find Sensor 1 at address 0x77!"));
        while (1); // Halt execution
    }

    Serial.println(F("Setup Complete. Both Sensors Ready."));
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
        Serial.println(F("LOCKOUT: High pressure threshold exceeded for >10 mins. Restart system."));
        delay(1000);
        return;
    }

    // 3. Read Pressure across both sensors (OR Logic Check)
    float p0 = bme0.readPressure();
    float p1 = bme1.readPressure();

    bool high_pressure_detected = (p0 > p_threshold) || (p1 > p_threshold);

    // 4. Over-Pressure Monitoring & 10-Minute Lockout
    if (high_pressure_detected) {
        unsigned long pressure_time = millis();

        while (high_pressure_detected) {
            digitalWrite(LED_PRESSURE, HIGH); // Illuminate warning LED
            
            // Shut off pumps immediately during high pressure warning
            digitalWrite(PUMP1_PIN, LOW);
            digitalWrite(PUMP2_PIN, LOW);

            delay(500);

            // Re-read both sensors
            p0 = bme0.readPressure();
            p1 = bme1.readPressure();
            high_pressure_detected = (p0 > p_threshold) || (p1 > p_threshold);

            // Check if high-pressure condition has persisted for > 10 minutes
            if ((millis() - pressure_time) > pressure_wait_time) {
                Serial.println(F("ERROR: HIGH PRESSURE PERSISTED FOR 10 MINUTES! LOCKOUT TRIPPED."));
                pressure_safe = false;
                digitalWrite(PUMP1_PIN, LOW);
                digitalWrite(PUMP2_PIN, LOW);
                return;
            }
        }
        // Pressure returned to safe levels across both sensors before timeout
        digitalWrite(LED_PRESSURE, LOW);
    }

    // 5. Turn ON Pumps when system is active and pressure is safe
    digitalWrite(PUMP1_PIN, HIGH);
    digitalWrite(PUMP2_PIN, HIGH);

    // 6. Read Primary Sensor (bme0) for Humidity and Temperature Checks
    float temp = bme0.readTemperature();
    float humidity = bme0.readHumidity();

    // Humidity Threshold Evaluation
    if (humidity > h_threshold) {
        digitalWrite(LED_HUMIDITY, HIGH);
    } else {
        digitalWrite(LED_HUMIDITY, LOW);
    }

    // Temperature Range Evaluation
    if (temp > temp_max || temp < temp_min) {
        digitalWrite(LED_TEMP, HIGH);
    } else {
        digitalWrite(LED_TEMP, LOW);
    }

    // Output Telemetry to Serial Monitor
    readSensors(p0, p1, temp, humidity);

    delay(delayTime);
}

// Helper Function for Console Telemetry
void readSensors(float p0, float p1, float temp, float humidity) {
    Serial.print(F("Temp (S0): ")); Serial.print(temp); Serial.print(F(" °C | "));
    Serial.print(F("Humidity (S0): ")); Serial.print(humidity); Serial.println(F(" %"));

    Serial.print(F("Pressure S0: ")); Serial.print(p0 / 100.0F); Serial.print(F(" hPa | "));
    Serial.print(F("S1: ")); Serial.print(p1 / 100.0F); Serial.println(F(" hPa"));

    Serial.println();
}