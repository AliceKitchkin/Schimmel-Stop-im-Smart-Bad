

#include <Arduino.h>

// WIFI
#include <ESP8266WiFi.h>
#include "./WiFiCredentials.h"

// DHT
#include <DHT_U.h>
#include <DHT.h>

#define DHTPIN 12     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22 // DHT 22  (AM2302), AM2321

// DHT Objekt erstellen
DHT dht(DHTPIN, DHTTYPE);

// InfluxDB
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

#define INFLUXDB_URL "https://XXX.de"
#define INFLUXDB_TOKEN "XXX"
#define INFLUXDB_ORG "dm"
#define INFLUXDB_BUCKET "arduino"
#define DEVICE "ESP8266"

// Daten die in die InfluxDB geschrieben werden
Point sensor("dht22");

// Time zone info
#define TZ_INFO "GMT+1"

InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);

/*
In der setup()-Funktion wird die serielle Kommunikation mit einer Baudrate von 115200 initialisiert.
Dann wird die setup_wifi()-Funktion aufgerufen, um eine Verbindung zum WiFi-Netzwerk herzustellen.
*/
void setup()
{
  Serial.print("Connecting");
  Serial.begin(115200);
  Serial.println();

  dht.begin();

  WiFi.begin(SSID, PASSWORD);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());

  // Hinzufügen von Tags zur späteren, eindeutigen Identifizierung
  sensor.addTag("device", DEVICE);

  // Festlegen der Zeitzone, benötigt von InfluxDB
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  // Testen ob die Verbindung zum Server möglich ist
  if (client.validateConnection())
  {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  }
  else
  {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }
}

void loop()
{
  // Überprüfen ob mit Wlan verbunden
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi connection lost!");
    return;
  }

  // Lesen des Sensors und speichern der Daten in der Datenbank
  readSensor();

  // 10s warten
  Serial.println("Wait 10s");
  delay(10000);
}

void readSensor()
{
  // Zwischen den Messungen einige Sekunden warten. 
  // Das Auslesen von Temperatur oder Luftfeuchtigkeit dauert etwa 250 Millisekunden! 
  // Sensorwerte können auch bis zu 2 Sekunden „alt“ sein (es ist ein sehr langsamer Sensor)

  float h = dht.readHumidity();
  // Temperatur in Celsius lesen (Standardeinstellung)
  float t = dht.readTemperature();

  // Prüfen Sie, ob ein Lesevorgang fehlgeschlagen ist und vorzeitiges Beenden um es erneut zu versuchen.
  if (isnan(h) || isnan(t))
  {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  sensor.addField("humidity", h);
  sensor.addField("temperature", t);
  sensor.addField("heat_index", dht.computeHeatIndex(t, h, false));

  // Ausgeben des Datensatzes über die Serielle Schnittstelle
  Serial.print("Writing: ");
  Serial.println(sensor.toLineProtocol());

  // Schreiben des Datensatzes in die Datenbank, Fehlermeldung falls es fehlschlägt
  if (!client.writePoint(sensor))
  {
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
  }
}