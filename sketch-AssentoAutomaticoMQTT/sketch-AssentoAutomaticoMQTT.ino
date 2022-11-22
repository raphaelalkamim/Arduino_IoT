#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <TZ.h>
#include <FS.h>
#include <LittleFS.h>
#include <CertStoreBearSSL.h>
#include <Ultrasonic.h>
#include <Servo.h>

//Pinos trigger e echo
#define pino_trigger 12 //D6 MISO
#define pino_echo 13 //D7 MOSI
#define pino_servo 14 //D5 SCK

//Inicializa o sensor
Ultrasonic ultrasonic(pino_trigger, pino_echo);
Servo servo;

// A single, global CertStore which can be used by all connections.
// Needs to stay live the entire time any of the WiFiClientBearSSLs
// are present.
BearSSL::CertStore certStore;

// Atualizar com os valores da rede Wi-Fi que será connectado e com o Cluster do MQTT Client
const char* ssid = "The_Simpsons";
const char* password = "AbrahamSimpson99*";
const char* mqtt_server = "cb78222242b4451a85d1fb1fcc19cc83.s2.eu.hivemq.cloud";

WiFiClientSecure espClient;
PubSubClient* client;
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (500)
char msg[MSG_BUFFER_SIZE];
int value = 0;

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print("connecting...");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setDateTime() {
  // You can use your own timezone, but the exact time is not used at all.
  // Only the date is needed for validating the certificates.
  configTime(TZ_Europe_Berlin, "pool.ntp.org", "time.nist.gov");

  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(100);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println();

  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.printf("%s %s", tzname[0], asctime(&timeinfo));
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if the first character is present
  if ((char)payload[0] != NULL) {
    digitalWrite(LED_BUILTIN, LOW);  // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is active low on the ESP-01)
    delay(500);
    digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
  } else {
    digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
  }
}

void reconnect() {
  // Loop until we’re reconnected
  while (!client->connected()) {
    Serial.print("Attempting MQTT connection…");
    String clientId = "ESP8266Client - MyClient";
    // Attempt to connect
    // Insert your password
    if (client->connect(clientId.c_str(), "IoTRaphaelEThiago", "t8YM6pufcp_gKXT")) {
      Serial.println("connected");
      // Once connected, publish an announcement…
      client->publish("test", "hello world");
      // … and resubscribe
      client->subscribe("test");
    } else {
      Serial.print("failed, rc = ");
      Serial.print(client->state());
      Serial.println(" try again in 10 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  delay(500);
  // When opening the Serial Monitor, select 9600 Baud
  Serial.begin(115200);
  delay(500);

  LittleFS.begin();
  setup_wifi();
  setDateTime();

  pinMode(LED_BUILTIN, OUTPUT);  // Initialize the LED_BUILTIN pin as an output

  // you can use the insecure mode, when you want to avoid the certificates
  //espclient->setInsecure();

  int numCerts = certStore.initCertStore(LittleFS, PSTR("/certs.idx"), PSTR("/certs.ar"));
  Serial.printf("Number of CA certs read: %d\n", numCerts);
  if (numCerts == 0) {
    Serial.printf("No certs found. Did you run certs-from-mozilla.py and upload the LittleFS directory before running?\n");
    return;  // Can't connect to anything w/o certs!
  }

  BearSSL::WiFiClientSecure* bear = new BearSSL::WiFiClientSecure();
  // Integrate the cert store with this connection
  bear->setCertStore(&certStore);

  client = new PubSubClient(*bear);

  client->setServer(mqtt_server, 8883);
  client->setCallback(callback);

  servo.attach(pino_servo);
  servo.write(0);
  delay(100);
  Serial.println("Processando dados...");
}

void loop() {
  if (!client->connected()) {
    reconnect();
  }
  client->loop();

  unsigned long now = millis();
  if (now - lastMsg > 2000) {
    float cmMsec;
    long microsec = ultrasonic.timing();
    cmMsec = ultrasonic.convert(microsec, Ultrasonic::CM);
    Serial.print("Distancia em cm: ");
    Serial.print(cmMsec);
    Serial.println();
    if (cmMsec < 10) {
      servo.write(180);
      lastMsg = now;
      ++value;
      snprintf(msg, MSG_BUFFER_SIZE, "Assento foi aberto! #%ld", value);
      Serial.print("Publish message: ");
      Serial.println(msg);
      client->publish("AssentoAutomatico:Sensor1", msg);
      delay(3000);
    } else {
      servo.write(0);
      delay(2000);
    }
  }
}
