#if defined(ESP32)
  #include <WiFi.h>
  #include <HTTPClient.h>
#elif defined(ARDUINO_UNOWIFIR4)
  #include <WiFiS3.h>
  #include <HttpClient.h>
#endif

#include <HX711.h>
#include <ArduinoJson.h>

// --- Wi-Fi Credentials ---
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// --- Supabase Configuration ---
// Your project REST URL (e.g., https://xyzprojectref.supabase.co/rest/v1/scale_readings)
const char* supabaseServer = "YOUR_PROJECT_REF.supabase.co";
const char* supabasePath = "/rest/v1/scale_readings";
const int   supabasePort = 443; // HTTPS
const char* apiKey = "YOUR_SUPABASE_ANON_KEY";

// --- HX711 Pin Mapping ---
const int LOADCELL_DOUT_PIN = 2;
const int LOADCELL_SCK_PIN = 3;

HX711 scale;

// Timing control
unsigned long lastPostTime = 0;
const unsigned long postInterval = 5000; // Post every 5 seconds

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // Initialize Scale
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(2280.f); // Replace with your scale's specific calibration factor
  scale.tare();            // Reset the scale to 0

  // Connect to Wi-Fi
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected!");
}

void loop() {
  // Check if it's time to send a new reading and scale data is ready
  if (millis() - lastPostTime >= postInterval) {
    lastPostTime = millis();

    if (scale.is_ready()) {
      float weight = scale.get_units(5); // Average of 5 readings
      Serial.print("Measured Weight: ");
      Serial.println(weight);

      sendDataToSupabase(weight);
    } else {
      Serial.println("HX711 not found or not ready.");
    }
  }
}

void sendDataToSupabase(float weightValue) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi disconnected. Reconnecting...");
    WiFi.begin(ssid, password);
    return;
  }

  // Create JSON payload: {"weight": 123.45}
  JsonDocument doc;
  doc["weight"] = weightValue;
  String jsonBody;
  serializeJson(doc, jsonBody);

#if defined(ESP32)
  // ESP32 Implementation using HTTPClient
  WiFiClientSecure client;
  client.setInsecure(); // Skip strict SSL certificate validation for simplicity
  HTTPClient http;

  String fullUrl = String("https://") + supabaseServer + supabasePath;
  
  if (http.begin(client, fullUrl)) {
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", apiKey);
    http.addHeader("Authorization", String("Bearer ") + apiKey);
    http.addHeader("Prefer", "return=minimal");

    int httpResponseCode = http.POST(jsonBody);

    if (httpResponseCode > 0) {
      Serial.printf("Supabase POST successful. Response code: %d\n", httpResponseCode);
    } else {
      Serial.printf("Error sending POST. Error: %s\n", http.errorToString(httpResponseCode).c_str());
    }
    http.end();
  } else {
    Serial.println("Unable to connect to Supabase endpoint.");
  }

#elif defined(ARDUINO_UNOWIFIR4)
  // Arduino Uno R4 WiFi Implementation
  WiFiSSLClient wifiClient;
  HttpClient httpClient = HttpClient(wifiClient, supabaseServer, supabasePort);

  httpClient.beginRequest();
  httpClient.post(supabasePath);
  httpClient.sendHeader("Content-Type", "application/json");
  httpClient.sendHeader("apikey", apiKey);
  httpClient.sendHeader("Authorization", String("Bearer ") + apiKey);
  httpClient.sendHeader("Content-Length", jsonBody.length());
  httpClient.beginBody();
  httpClient.print(jsonBody);
  httpClient.endRequest();

  int statusCode = httpClient.responseStatusCode();
  String response = httpClient.responseBody();

  Serial.print("Supabase Status code: ");
  Serial.println(statusCode);
#endif
}
