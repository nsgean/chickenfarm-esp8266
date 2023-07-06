#include <Servo.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFiMulti.h>
#include <FirebaseArduino.h>
#include "HX711.h"

#define DOUT 12
#define CLK 13
#define NSERVO 14
#define FIREBASE_HOST "..."
#define FIREBASE_AUTH "..."
#define WIFI_SSID "..."
#define WIFI_PASSWORD "..."

HTTPClient http;
ESP8266WiFiMulti WiFiMulti;
Servo servomotor;
DynamicJsonBuffer jsonBuffer;
HX711 balanca;                    
float calibration_factor = 475030;

const char fingerprint[] PROGMEM = "...";

void setup() {
  Serial.begin(9600); 

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println(); 
  Serial.print("connecting");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("connected: ");
  Serial.println(WiFi.localIP());

  servomotor.attach(NSERVO);
  servomotor.write(50);

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);

  balanca.begin(DOUT, CLK);             
  balanca.tare();                   
  balanca.set_scale(calibration_factor); 
}

void loop() {
  int min = Firebase.getInt("Weight/min");
  float gram = (balanca.get_units() * 1000);

  Serial.println();                                       
  Serial.print("Peso: ");                                
  Serial.print(gram);                
  Serial.println();                 
  Serial.print("Min: ");                 
  Serial.print(min);                     

  saveData();

  if(gram < min) {
    releaseFood();
  }

  delay(15000);
}

void saveData() {
  float gram = (balanca.get_units() * 1000);

  JsonObject& gramObject = jsonBuffer.createObject();
  JsonObject& tempTime = gramObject.createNestedObject("timestamp");
  gramObject["avg"] = String(gram);
  tempTime[".sv"] = "timestamp";

  Firebase.push("Avg", gramObject);

  if (Firebase.failed()) {
    Serial.print("Failed:");
    Serial.println(Firebase.error());
    return;
  }
}

void releaseFood() {
  int max = Firebase.getInt("Weight/max");
  float gramStart = (balanca.get_units() * 1000);
  float actualGram = (balanca.get_units() * 1000);
  Serial.println();
  Serial.print("Soltar ração: ");

  servomotor.write(35);

  do {
    actualGram = (balanca.get_units() * 1000);
  } while (actualGram < max);

  servomotor.write(50);
  delay(5000);
  float gramEnd = (balanca.get_units() * 1000);
  float gramQnt = gramEnd - gramStart;
  Serial.print(gramEnd);
  Serial.print(", ");
  Serial.print(gramQnt);

  JsonObject& gramObject = jsonBuffer.createObject();
  JsonObject& tempTime = gramObject.createNestedObject("timestamp");
  gramObject["qnt"] = String(gramQnt);
  tempTime[".sv"] = "timestamp";

  Firebase.push("Qtt", gramObject);

  sendNotification();

  if (Firebase.failed()) {
    Serial.print("Failed:");
    Serial.println(Firebase.error());
    return;
  }
}

void sendNotification() {
  if ((WiFiMulti.run() == WL_CONNECTED)) {
    std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
    client->setFingerprint(fingerprint);

    HTTPClient http;

    http.begin(*client, "https://onesignal.com/api/v1/notifications");
    http.addHeader("Authorization", "Basic ...");  
    http.addHeader("accept", "application/json");                                               
    http.addHeader("content-type", "application/json");                                         

    int httpCode = http.POST("{\"included_segments\":[\"Subscribed Users\"],\"contents\":{\"en\":\"Houve um novo fornecimento de alimentos\",\"pt\":\"Houve um novo fornecimento de alimentos\"},\"name\":\"refood\",\"app_id\":\"...\"}");  
      
    if (httpCode > 0) {                                                                                                                                                                                   
      String payload = http.getString();
      Serial.println(httpCode);
      Serial.println(payload);
    } else {
      Serial.println(httpCode);
      Serial.println("Falha na requisição");
    }
    http.end();
  }
}