#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define BUTTON 12
#define PRESSURE 34
#define BUZZER 27
#define TEMP_PIN 4

const char* ssid = "Wifi-Name";
const char* password = "Password";

OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);

void connectWiFi()
{
    WiFi.begin(ssid, password);

    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.println("WiFi Connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
}
void setup()
{
    Serial.begin(115200);
    pinMode(BUTTON, INPUT_PULLUP);
    pinMode(PRESSURE, INPUT);
    pinMode(BUZZER, OUTPUT);
    sensors.begin();
    connectWiFi();
    Serial.println("ESP32 is working");
}
void loop()
{
}
