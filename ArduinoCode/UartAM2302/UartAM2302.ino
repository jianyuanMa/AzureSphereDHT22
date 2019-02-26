#include <DHT.h>
#include <DHT_U.h>
#include <SoftwareSerial.h>

#define DHTPIN 2
#define DHTTYPE DHT22

SoftwareSerial azureSerial(0, 1);
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  // put your setup code here, to run once:
  azureSerial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  dht.begin();
  azureSerial.println("Read Temperature Sensor Start!");
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(2000);
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t) ) {
    azureSerial.println(F("Failed to read from DHT sensor!"));
    return;
  }
  azureSerial.print(h);
  azureSerial.print(F(","));
  azureSerial.print(t);
  azureSerial.flush();
}
