// 3rd includes
#include <Arduino.h>
#define ESP32
#include <M5StickC.h>
#undef min
// IR
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <IRtext.h>
// Network
#include <HTTPClient.h>
#include <ESPmDNS.h>

// system include
#include <cstdint>
#include <memory>
#include <tuple>

#define DEBUG

#if defined(DEBUG)
# define dbg_print    Serial.print
# define dbg_println  Serial.println
#else
# define dbg_print(...)
# define dbg_println(...)
#endif

// LED
constexpr uint8_t LED_PIN = GPIO_NUM_10;
constexpr uint8_t LED_ON = LOW;
constexpr uint8_t LED_OFF = HIGH;

// IR
constexpr uint16_t IR_RECV_PIN = GPIO_NUM_33;
constexpr uint16_t IR_CAPTURE_BUFFER_SIZE = 1024;
constexpr uint16_t IR_MIN_UNKNOWN_LENGTH = 12;
// 受信タイムアウト時間
// 長くすると、エアコンなどの複雑なメッセージパケットを受信することができます(一般に20〜40ms+数ミリ)
// 長いと、メッセージパケットが短いテレビリモコンなどで、長押しすると複数メッセージを1回でキャプチャされてしまうことがあります(一般に20ms)
// 130ms超えないようにすること
constexpr uint8_t IR_TIMEOUT_MS = 50;

// Network
constexpr char *TARGET_HOSTNAME = "ir-server";
constexpr char *HOSTNAME = "ir-client1";
constexpr int64_t TARGET_HOSTNAME_UPDATE_THRESHOLD_US = 5 * 60e6; // 5min

void setup_ir();
void setup_wifi();
void reboot();
const IPAddress &update_target_ip();
void print_ir_result_message(decode_results *results);
void send_ir_message(decode_results *results);

// IR
IRrecv g_irrecv{IR_RECV_PIN, IR_CAPTURE_BUFFER_SIZE, IR_TIMEOUT_MS, true};
decode_results g_results;
IPAddress g_target_ip;
int64_t g_last_updated_target_ip_us{0};

void setup()
{
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_ON);

  setup_ir();
  Serial.begin(115200);
  while (!Serial) { // Wait for the serial connection to be establised.
    delay(50);
  }

  setup_wifi();
  if (MDNS.begin(HOSTNAME)) {
    dbg_println("mDNS responder started");
  }
  delay(500);
  digitalWrite(LED_PIN, LED_OFF);

  update_target_ip();
  Serial.println(WiFi.SSID());
  Serial.println(WiFi.localIP().toString());
}

void loop()
{
  if (g_irrecv.decode(&g_results)) {
    print_ir_result_message(&g_results);
    if (g_results.decode_type != UNKNOWN) {
      send_ir_message(&g_results);
    }
    g_irrecv.resume();  // Receive the next value
  }
  else {
    M5.update();
    if (M5.BtnB.pressedFor(3000)) {
      reboot();
    }

    delay(50);
  }
}

const IPAddress& update_target_ip()
{
  const int64_t elapsed_us = esp_timer_get_time() - g_last_updated_target_ip_us;

  if ((g_target_ip == INADDR_NONE) || (elapsed_us >= TARGET_HOSTNAME_UPDATE_THRESHOLD_US)) {
    g_target_ip = MDNS.queryHost(TARGET_HOSTNAME, 2000); // 2sec
    g_last_updated_target_ip_us = esp_timer_get_time();
  }
  return g_target_ip;
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

void setup_ir()
{
  g_irrecv.setUnknownThreshold(IR_MIN_UNKNOWN_LENGTH);
  g_irrecv.enableIRIn();  // Start the receiver
}

std::tuple<size_t, String> make_raw_string(decode_results *results)
{
  size_t len = 0;
  String output;

  // reference: IRutils.cpp / resultToRawArray
  output = "[";
  for (uint16_t i = 1; i < results->rawlen; i++) {
    uint32_t usecs;
    for (usecs = results->rawbuf[i] * kRawTick;
         usecs > UINT16_MAX;
         usecs -= UINT16_MAX) {
      output += uint64ToString(UINT16_MAX);
      output += F(",0,");
      len += 2;
    }
    output += uint64ToString(usecs, 10); // convert to decimal
    len += 1;
    if (i < results->rawlen - 1)
      output += kCommaSpaceStr;            // ',' not needed on the last one
  }
  output += "]";
  return std::make_tuple(len, output);
}

void send_http_message(const String &message)
{
  HTTPClient http;

  const IPAddress &ip = update_target_ip();
  const String url = String("http://") + ip.toString() + "/ir-control";

  // @FIXME upate to socket connection(?)
  http.begin(url);
  const int status_code = http.POST(message);
  Serial.print("URL: ");
  Serial.println(url);
  Serial.print("StatusCode: ");
  Serial.println(status_code);
  http.end();
}

void send_ir_message(decode_results *results)
{
  String json_str;
  const std::tuple<size_t, String> send_info = make_raw_string(results);
  json_str += "{";
  json_str += "\"length\":" + String(std::get<0>(send_info)) + ",";
  json_str += "\"raw\":" + std::get<1>(send_info);
  json_str += "}";

  send_http_message(json_str);
  dbg_println(json_str.c_str());
}

void print_ir_result_message(decode_results *results)
{
  if (results->overflow) {
    Serial.println("IR code too long. Edit IRremoteInt.h and increase RAWBUF");
    return;
  }

  Serial.print("maker type: ");
  Serial.println(typeToString(results->decode_type));

  if (results->address > 0 || results->command > 0) {
    Serial.print("address = 0x");
    Serial.println(uint64ToString(results->address, 16));
    Serial.print("command = 0x");
    Serial.println(uint64ToString(results->command, 16));
  }
  // Most protocols have data
  Serial.println("data = 0x");
  Serial.println(uint64ToString(results->value, 16));
}

[[noreturn]] void reboot()
{
  esp_restart();  // software reset
}
