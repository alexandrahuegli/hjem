#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "secrets.h"

// Hardware: drive the LED strip through a suitable MOSFET or LED-strip driver.
// Do not connect an LED strip directly to an ESP32 GPIO.
constexpr uint8_t LED_PIN = 3;

// Start conservatively. This is the maximum PWM level during the 09:00-18:00
// period, not the percentage of the daytime period.
constexpr uint8_t MAX_BRIGHTNESS_PERCENT = 40;

constexpr uint32_t PWM_FREQUENCY_HZ = 1000;
constexpr uint8_t PWM_RESOLUTION_BITS = 12;
constexpr uint32_t PWM_MAX_DUTY = (1UL << PWM_RESOLUTION_BITS) - 1;
constexpr uint32_t MAX_DUTY =
    (PWM_MAX_DUTY * MAX_BRIGHTNESS_PERCENT) / 100;


// Europe/Oslo, including daylight-saving time changes.
const char *TIME_ZONE = "CET-1CEST,M3.5.0,M10.5.0/3";
const char *NTP_SERVER_1 = "pool.ntp.org";
const char *NTP_SERVER_2 = "time.nist.gov";

constexpr uint32_t MORNING_START_SECONDS = 8UL * 60UL * 60UL;
constexpr uint32_t MORNING_END_SECONDS = 9UL * 60UL * 60UL;
constexpr uint32_t EVENING_START_SECONDS = 18UL * 60UL * 60UL;
constexpr uint32_t EVENING_END_SECONDS = 19UL * 60UL * 60UL;
constexpr uint32_t RAMP_DURATION_SECONDS = 60UL * 60UL;

constexpr uint32_t SCHEDULE_UPDATE_INTERVAL_MS = 1000;
constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 30000;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;

// Any time before this is treated as "not synchronized yet".
constexpr time_t MIN_VALID_EPOCH = 1700000000;

uint32_t lastScheduleUpdateMs = 0;
uint32_t lastWifiAttemptMs = 0;
uint32_t lastAppliedDuty = UINT32_MAX;
bool wifiAttemptInProgress = false;
bool pwmReady = false;

bool wifiCredentialsAreConfigured() {
  return strlen(WIFI_SSID) > 0 &&
         strlen(WIFI_PASSWORD) > 0 &&
         strcmp(WIFI_SSID, "YOUR_WIFI_SSID") != 0 &&
         strcmp(WIFI_PASSWORD, "YOUR_WIFI_PASSWORD") != 0;
}

uint32_t interpolateDuty(uint32_t from, uint32_t to, uint32_t elapsed,
                         uint32_t duration) {
  const int64_t change = static_cast<int64_t>(to) - from;
  const int64_t duty = static_cast<int64_t>(from) +
                       (change * elapsed) / duration;
  return static_cast<uint32_t>(duty);
}

uint32_t dutyForSecondsSinceMidnight(uint32_t secondsSinceMidnight) {
  if (secondsSinceMidnight < MORNING_START_SECONDS ||
      secondsSinceMidnight >= EVENING_END_SECONDS) {
    return 0;
  }

  if (secondsSinceMidnight < MORNING_END_SECONDS) {
    return interpolateDuty(0, MAX_DUTY,
                           secondsSinceMidnight - MORNING_START_SECONDS,
                           RAMP_DURATION_SECONDS);
  }

  if (secondsSinceMidnight < EVENING_START_SECONDS) {
    return MAX_DUTY;
  }

  return interpolateDuty(MAX_DUTY, 0,
                          secondsSinceMidnight - EVENING_START_SECONDS,
                          RAMP_DURATION_SECONDS);
}

bool readLocalTime(struct tm *localTime) {
  time_t now;
  time(&now);

  if (now < MIN_VALID_EPOCH) {
    return false;
  }

  localtime_r(&now, localTime);
  return true;
}

void applyDuty(uint32_t duty) {
  if (!pwmReady || duty == lastAppliedDuty) {
    return;
  }

  // The BC337/MOSFET stage is inverted: GPIO HIGH turns the LEDs off.
  const uint32_t gpioDuty = PWM_MAX_DUTY - duty;
  if (ledcWrite(LED_PIN, gpioDuty)) {
    lastAppliedDuty = duty;
  } else {
    Serial.println("PWM write failed; keeping the previous output level.");
  }
}
void beginWiFiConnection() {
  Serial.println("Starting Wi-Fi connection.");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWifiAttemptMs = millis();
  wifiAttemptInProgress = true;
}

void maintainWiFi() {
  if (!wifiCredentialsAreConfigured()) {
    return;
  }

  const uint8_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    if (wifiAttemptInProgress) {
      Serial.print("Wi-Fi connected. IP address: ");
      Serial.println(WiFi.localIP());
      wifiAttemptInProgress = false;
    }
    return;
  }

  if (wifiAttemptInProgress) {
    if (millis() - lastWifiAttemptMs < WIFI_CONNECT_TIMEOUT_MS) {
      return;
    }

    Serial.printf("Wi-Fi connection timed out (status %u); retrying later.\n",
                  status);
    WiFi.disconnect();
    wifiAttemptInProgress = false;
    lastWifiAttemptMs = millis();
    return;
  }

  if (millis() - lastWifiAttemptMs >= WIFI_RETRY_INTERVAL_MS) {
    beginWiFiConnection();
  }
}

void updateSchedule() {
  struct tm localTime;
  if (!readLocalTime(&localTime)) {
    // Fail dark until the clock is known. This prevents an unsynchronized
    // reboot from turning the aquarium light on at the wrong time.
    applyDuty(0);
    return;
  }

  const uint32_t secondsSinceMidnight =
      static_cast<uint32_t>(localTime.tm_hour) * 60UL * 60UL +
      static_cast<uint32_t>(localTime.tm_min) * 60UL +
      static_cast<uint32_t>(localTime.tm_sec);

  applyDuty(dutyForSecondsSinceMidnight(secondsSinceMidnight));
}

void setup() {
  Serial.begin(115200);

  pwmReady = ledcAttach(LED_PIN, PWM_FREQUENCY_HZ, PWM_RESOLUTION_BITS);
  if (!pwmReady) {
    Serial.println("Could not attach PWM to GPIO 3.");
    return;
  }

  applyDuty(0);

  if (!wifiCredentialsAreConfigured()) {
    Serial.println("Set WIFI_SSID and WIFI_PASSWORD before uploading.");
    Serial.println("The light will remain off until time is synchronized.");
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);

  // Configure NTP before starting the Wi-Fi connection.
  configTzTime(TIME_ZONE, NTP_SERVER_1, NTP_SERVER_2);
  beginWiFiConnection();
  Serial.println("Waiting for Wi-Fi/NTP time synchronization.");
}

void loop() {
  maintainWiFi();

  if (millis() - lastScheduleUpdateMs >= SCHEDULE_UPDATE_INTERVAL_MS) {
    lastScheduleUpdateMs = millis();
    updateSchedule();
  }

  delay(50);
}
