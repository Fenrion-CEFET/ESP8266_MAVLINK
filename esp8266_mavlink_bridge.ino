/*
  ESP8266 (ESP8266MOD / ESP-12) : ponte MAVLink Pixhawk <-> Wi-Fi (roteador)

  Pixhawk TELEM2 (UART) <-> ESP8266 <-> roteador <-> Mission Planner / QGC / Python (UDP 14550)

  - Conecta no roteador em modo estacao (STA)
  - Le os pacotes MAVLink da Pixhawk e envia por UDP
      * antes de a estacao de solo falar: broadcast na rede, porta 14550
      * depois: direto para o IP/porta de quem enviou o ultimo pacote
  - Tudo que chega por UDP vai para a Pixhawk pela serial

  Ligacao (niveis 3,3 V):
    Pixhawk TELEM2 TX  -> ESP8266 RX (GPIO3)
    Pixhawk TELEM2 RX  -> ESP8266 TX (GPIO1)
    GND                -> GND
    Alimentacao: 3,3 V com regulador proprio (picos de ~300 mA no Wi-Fi).
    NAO ligar o pino de 5 V da TELEM direto no modulo ESP8266MOD.

  Parametros ArduPilot (Pixhawk):
    SERIAL2_PROTOCOL = 2      (MAVLink2)
    SERIAL2_BAUD     = 115    (115200)
    BRD_SER2_RTSCTS  = 0      (sem controle de fluxo)

  Arduino IDE:
    Placa: "Generic ESP8266 Module" (modulo solto) ou "NodeMCU 1.0 (ESP-12E Module)"
    Flash Size: 4MB (FS:none OTA:~1019KB)
    Debug (opcional): Serial1 = GPIO2 (somente TX), 115200
*/

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

// ===================== Configuracao =====================
const char *WIFI_SSID = "NOME_DA_REDE";
const char *WIFI_PASS = "SENHA";

#define MAV_BAUD   115200   // igual ao SERIAL2_BAUD da Pixhawk
#define UDP_PORT   14550    // porta padrao das estacoes de solo
#define DEBUG      1        // 1 = mensagens no Serial1 (GPIO2)

// IP fixo opcional (1 = IP fixo, 0 = DHCP)
#define USE_STATIC_IP 0
IPAddress local_IP(192, 168, 2, 60);
IPAddress gateway(192, 168, 2, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns1(8, 8, 8, 8);

#if DEBUG
#define DBG(...) Serial1.printf(__VA_ARGS__)
#else
#define DBG(...)
#endif

// ===================== Estado =====================
WiFiUDP udp;
IPAddress gcsIP;
uint16_t gcsPort = UDP_PORT;
bool gcsKnown = false;

uint8_t frame[300];       // maior quadro MAVLink2 assinado = 280 bytes
uint16_t frameIdx = 0;
uint16_t frameLen = 0;

uint8_t udpBuf[512];

uint32_t txFrames = 0, rxBytes = 0, lastStats = 0;

// ===================== Envio UDP =====================
void sendToGcs(const uint8_t *buf, size_t len) {
  if (WiFi.status() != WL_CONNECTED) return;
  IPAddress dst = gcsKnown ? gcsIP : WiFi.broadcastIP();
  uint16_t port = gcsKnown ? gcsPort : UDP_PORT;
  udp.beginPacket(dst, port);
  udp.write(buf, len);
  udp.endPacket();
  txFrames++;
}

// ===================== Montagem dos quadros MAVLink =====================
// Envia um pacote UDP por quadro MAVLink completo (v1 = 0xFE, v2 = 0xFD)
void parseMavByte(uint8_t c) {
  if (frameIdx == 0) {
    if (c == 0xFE || c == 0xFD) frame[frameIdx++] = c;  // espera inicio de quadro
    return;
  }

  frame[frameIdx++] = c;

  if (frameIdx == 2) {
    // byte 1 = tamanho do payload
    frameLen = (frame[0] == 0xFE) ? (c + 8) : (c + 12);
  } else if (frameIdx == 3 && frame[0] == 0xFD && (c & 0x01)) {
    frameLen += 13;  // MAVLink2 com assinatura
  }

  if (frameIdx > 2 && frameIdx == frameLen) {
    sendToGcs(frame, frameIdx);
    frameIdx = 0;
  } else if (frameIdx >= sizeof(frame)) {
    frameIdx = 0;  // perdeu sincronismo, recomeca
  }
}

// ===================== Wi-Fi =====================
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);  // menor latencia
  WiFi.setAutoReconnect(true);
#if USE_STATIC_IP
  WiFi.config(local_IP, gateway, subnet, dns1);
#endif
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  DBG("\nConectando ao Wi-Fi");
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DBG(".");
    if (millis() - t0 > 20000) {
      DBG("\nTimeout no Wi-Fi, reiniciando...\n");
      ESP.restart();
    }
  }
  DBG("\nConectado! IP: %s  RSSI: %d dBm\n",
      WiFi.localIP().toString().c_str(), WiFi.RSSI());
  DBG("Enviando MAVLink para %s:%u (broadcast ate a GCS responder)\n",
      WiFi.broadcastIP().toString().c_str(), UDP_PORT);
}

// ===================== Setup / Loop =====================
void setup() {
#if DEBUG
  Serial1.begin(115200);
#endif
  Serial.setRxBufferSize(2048);
  Serial.begin(MAV_BAUD);   // Serial = GPIO1/GPIO3, ligada na Pixhawk

  connectWiFi();
  udp.begin(UDP_PORT);
}

void loop() {
  // Pixhawk -> Wi-Fi
  while (Serial.available()) {
    parseMavByte((uint8_t)Serial.read());
  }

  // Wi-Fi -> Pixhawk
  int pkt = udp.parsePacket();
  if (pkt > 0) {
    if (!gcsKnown || udp.remoteIP() != gcsIP || udp.remotePort() != gcsPort) {
      gcsIP = udp.remoteIP();
      gcsPort = udp.remotePort();
      gcsKnown = true;
      DBG("Estacao de solo: %s:%u\n", gcsIP.toString().c_str(), gcsPort);
    }
    int n = udp.read(udpBuf, sizeof(udpBuf));
    if (n > 0) {
      Serial.write(udpBuf, n);
      rxBytes += n;
    }
  }

  // estatisticas a cada 5 s (somente no debug)
  if (millis() - lastStats > 5000) {
    lastStats = millis();
    DBG("Quadros enviados: %lu | bytes recebidos da GCS: %lu | RSSI: %d dBm\n",
        txFrames, rxBytes, WiFi.RSSI());
  }

  yield();
}
