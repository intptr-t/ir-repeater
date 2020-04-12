// 3rd includes
#include <Arduino.h>
#define ESP32
#include <M5StickC.h>
#undef min
// Network
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Arduino_JSON.h> // https://github.com/arduino-libraries/Arduino_JSON
// IRremoteESP8266
#include <IRsend.h>

#include <vector>

#define DEBUG

#if defined(DEBUG)
# define dbg_print    Serial.print
# define dbg_println  Serial.println
#else
# define dbg_print(...)
# define dbg_println(...)
#endif

#ifdef __cpp_char8_t
typedef char8_t utf8_char_t;
#else
typedef char utf8_char_t;
#endif

// LED
constexpr uint8_t LED_PIN = GPIO_NUM_10;
constexpr uint8_t LED_ON = LOW;
constexpr uint8_t LED_OFF = HIGH;
// Network
constexpr uint16_t SERVER_PORT = 80;
constexpr char *HOSTNAME = "ir-server";
// IR
//constexpr uint8_t IR_SEND_PIN = GPIO_NUM_9; // OK but low power
constexpr uint8_t IR_SEND_PIN = GPIO_NUM_33; // OK
//constexpr uint8_t IR_SEND_PIN = GPIO_NUM_26; // OK
constexpr int IR_FREQ_KHZ = 38;

constexpr const utf8_char_t* NOT_FOUND_HTML[] = {
u8R"(<!DOCTYPE HTML>
<html><head><title>404 Not Found</title>
<meta name="viewport" content="width=device-width,initial-scale=1"></head>
<meta charset="UTF-8">
<body>
<h1>404 Not Found</h1>
</body>
</html>
)"
};

void disp_on();
void disp_off();
void reboot();
void setup_wifi();
void setup();
void loop();
void on_handle_not_found();
void on_handle_ir_control();
void response_bad_request();

WebServer g_server(SERVER_PORT);
IRsend g_irsend(IR_SEND_PIN);

void setup()
{
  // Initial
  // WiFi.begin("${INITIAL SSID}", "${INITIAL PASSWORD}");

  M5.begin();
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_ON);
  disp_off();

  g_irsend.begin();
  Serial.begin(115200);
  while (!Serial) { // Wait for the serial connection to be establised.
    delay(50);
  }

  setup_wifi();
  if (MDNS.begin(HOSTNAME)) {
    dbg_println("mDNS responder started");
  }
  g_server.on("/ir-control", on_handle_ir_control);
  g_server.onNotFound(on_handle_not_found);
  g_server.begin();

  digitalWrite(LED_PIN, LED_OFF);

  Serial.println(WiFi.SSID());
  Serial.println(WiFi.localIP().toString());
}

void loop()
{
  g_server.handleClient();

  M5.update();
  if (M5.BtnB.pressedFor(3000)) {
    reboot();
  }
  delay(50);
}


void setup_wifi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin();

  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, LED_OFF);
    delay(250);
    digitalWrite(LED_PIN, LED_ON);
    delay(250);
  }
}

void on_handle_not_found()
{
  WiFiClient client = g_server.client();

  client.println("HTTP/1.1 404 Not Found");
  client.println("Content-Type: text/html");
  client.println("Accept-Ranges: none");
  client.println("Access-Control-Allow-Origin: *");
  client.println("Connection: close");
  client.println("");

  for (auto html_c_str = std::begin(NOT_FOUND_HTML);
       html_c_str < std::end(NOT_FOUND_HTML);
       ++html_c_str)
  {
    client.println(*html_c_str);
  }
}

void on_handle_ir_control()
{
  if (g_server.method() != HTTP_POST) {
    on_handle_not_found();
    // return;
  }

  String request_body;
  for (int i = 0; i < g_server.args(); i++) {
    request_body += g_server.argName(i) + ":" + g_server.arg(i);
  }
  Serial.println("ir-control");
  Serial.println(request_body);

  JSONVar json_response;
  if (g_server.args() > 0) {
    JSONVar json_root = JSON.parse(g_server.arg(0));
    if (JSON.typeof(json_root) == "undefined") {
      dbg_println("Bad JSON.");
      response_bad_request();
      return;
    }

    if (!json_root.hasOwnProperty("length")) {
      dbg_println("Not found 'length'.");
      response_bad_request();
      return;
    }
    if (!json_root.hasOwnProperty("raw")) {
      dbg_println("Not found 'raw'.");
      response_bad_request();
      return;
    }

    size_t length;
    if (JSON.typeof(json_root["length"]) == "string") {
      const char *str_length = static_cast<const char*>(json_root["length"]);
      length = strtol(str_length, NULL, 10);
    }
    else {
      length = static_cast<long>(json_root["length"]);
    }

    std::vector<uint16_t> raw_buffer(length);
    if (JSON.typeof(json_root["raw"]) == "array") {
      auto raw_ary = json_root["raw"];

      raw_buffer.resize(raw_ary.length());
      for (int i = 0; i < raw_ary.length(); i++) {
        raw_buffer[i] = static_cast<long>(raw_ary[i]);
      }
    }

    Serial.print("length: "); Serial.println(length);
    Serial.print("buffer.size: "); Serial.println(raw_buffer.size());
    Serial.print("raw type: "); Serial.println(JSON.typeof(json_root["raw"]));

    g_irsend.sendRaw(&raw_buffer[0], raw_buffer.size(), IR_FREQ_KHZ);
  }
  WiFiClient client = g_server.client();

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Accept-Ranges: none");
  client.println("Access-Control-Allow-Origin: *");
  client.println("Connection: close");
  client.println("");

  client.println(JSON.stringify(json_response));
}

void response_bad_request()
{
  WiFiClient client = g_server.client();

  client.println("HTTP/1.1 400 BadRequest");
  client.println("Content-Type: text/html");
  client.println("Accept-Ranges: none");
  client.println("Access-Control-Allow-Origin: *");
  client.println("Connection: close");
  client.println("");
}

void disp_off()
{
#if defined(ARDUINO_M5Stick_C)
  M5.Lcd.writecommand(ST7735_DISPOFF);
  M5.Axp.ScreenBreath(0);
#elif defined(ARDUINO_M5Stack_Core_ESP32) || defined(ARDUINO_M5STACK_FIRE)
  M5.Lcd.writecommand(ILI9341_DISPOFF);
  M5.Lcd.setBrightness(0);
#else
  // nothing
#endif
}

void disp_on()
{
  // display off
#if defined(ARDUINO_M5Stick_C)
  M5.Lcd.writecommand(ST7735_DISPON);
  M5.Axp.ScreenBreath(9);
#elif defined(ARDUINO_M5Stack_Core_ESP32) || defined(ARDUINO_M5STACK_FIRE)
  M5.Lcd.writecommand(ILI9341_DISPON);
  M5.Lcd.setBrightness(9);
#else
  // nothing
#endif
}

[[noreturn]] void reboot()
{
  esp_restart();  // software reset
}
