#include <Arduino.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <DHT.h>
#include <HTTPClient.h>

#include "config.h"

// NTP Client
WiFiUDP ntpUDP;
NTPClient ntpClient(ntpUDP);

// DHT Sensor
DHT dht(DHTPIN, DHTTYPE);

HTTPClient http;

/*
  Function to set up the connection to the WiFi AP
*/
void setupWiFi() {
    Serial.print("Connecting to '");
    Serial.print(WIFI_SSID);
    Serial.print("' ...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("connected");

    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    randomSeed(micros());
}

/*
  Function to format a unix timestamp as a human-readable time
*/
String formatTime(unsigned long rawTime) {
    unsigned long localTime = rawTime + (TZ_OFFSET * 3600);

    unsigned long hours = (localTime % 86400L) / 3600;
    String hoursStr = hours < 10 ? "0" + String(hours) : String(hours);

    unsigned long minutes = (localTime % 3600) / 60;
    String minuteStr = minutes < 10 ? "0" + String(minutes) : String(minutes);

    unsigned long seconds = localTime % 60;
    String secondStr = seconds < 10 ? "0" + String(seconds) : String(seconds);

    return hoursStr + ":" + minuteStr + ":" + secondStr;
}

// Function to submit metrics to locally run Influx
void submitData(unsigned long ts, float cels, float hum, float hic) {
    // Build body
    String body = String("temperature value=") + cels + " " + ts + "\n" +
                  "humidity value=" + hum + " " + ts + "\n" +
                  "index value=" + hic + " " + ts + "\n";

    // TODO add auth/https
    http.begin(String("http://") + HOST + ":" + PORT + "/write?db=" + DB_NAME + "&precision=s");
    http.addHeader("Content-Type", "--data-binary");

    int httpCode = http.POST(body);
    if (httpCode > 0) {
        Serial.printf("POST...  Code: %d  Response: %s", httpCode, http.getString().c_str());
        Serial.println();
    } else {
        Serial.printf("POST... Error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
}

/*
  Function called at boot to initialize the system
*/
void setup() {
    // start the serial output at 115,200 baud
    Serial.begin(115200);

    // connect to WiFi
    setupWiFi();

    // Initialize a NTPClient to get time
    ntpClient.begin();

    // start the DHT sensor
    dht.begin();
}

/*
  Function called in a loop to read temp/humidity and submit them to hosted metrics
*/
void loop() {
    // reconnect to WiFi if required
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect();
        yield();
        setupWiFi();
    }

    // update time via NTP if required
    while (!ntpClient.update()) {
        yield();
        ntpClient.forceUpdate();
    }

    // get current timestamp
    unsigned long ts = ntpClient.getEpochTime();

    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)

    // Read humidity
    float humidity = dht.readHumidity();
    yield();

    // Read temperature as Celsius (the default)
    float celcius = dht.readTemperature();
    yield();

    // Read temperature as Fahrenheit (isFahrenheit = true)
    float fahrenheit = dht.readTemperature(true);
    yield();

    // Check if any reads failed and exit early (to try again).
    if (isnan(humidity) || isnan(celcius) || isnan(fahrenheit)) {
        Serial.println(F("Failed to read from DHT sensor!"));
        return;
    }

    // Compute heat index in Fahrenheit (the default)
    float hif = dht.computeHeatIndex(fahrenheit, humidity);
    // Compute heat index in Celsius (isFahrenheit = false)
    float hic = dht.computeHeatIndex(celcius, humidity, false);

    // output readings on Serial connection
    Serial.println(
            formatTime(ts) +
            "  Humidity: " + humidity + "%" +
            "  Temperature: " + celcius + "°C " + fahrenheit + "°F" +
            "  Heat index: " + hic + "°C " + hif + "°F"
    );

    yield();
    submitData(ts, celcius, humidity, hic);

    // wait 30s, then do it again
    delay(30 * 1000);
}