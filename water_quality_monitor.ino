#include <Wire.h>
#include <ADS1X15.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define LCD_ADDR 0x27
#define LCD_COLS 16
#define LCD_ROWS 2

#define ONE_WIRE_BUS D8

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
ADS1115 ads(0x48);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

float pH, turbidity, tds, temperature;

void notifyClients() {
    String json = "{\"pH\":" + String(pH, 2) + 
                  ",\"tds\":" + String(tds, 1) + 
                  ",\"turbidity\":" + String(turbidity, 1) + 
                  ",\"temperature\":" + String(temperature, 1) + "}";
    ws.textAll(json);
}

void setup() {
    Serial.begin(115200);
    Wire.begin(D2, D1);
    lcd.begin(LCD_COLS, LCD_ROWS);
    lcd.init();
    lcd.backlight();

    lcd.setCursor(0, 0);
    lcd.print("Connecting WiFi");
    WiFi.begin(ssid, password);
    int count = 0;
    while (WiFi.status() != WL_CONNECTED && count < 30) {
        delay(1000);
        count++;
    }
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi: ");
    lcd.print(WiFi.status() == WL_CONNECTED ? "Connected" : "Failed");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
    delay(3000);
    lcd.clear();

    Wire.begin(D3, D4);
    if (!ads.begin()) {
        Serial.println("ADS1115 Init Failed!");
        lcd.setCursor(0, 0);
        lcd.print("ADS Init Failed");
        while (1);
    }

    sensors.begin();

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", R"rawliteral(
        <!DOCTYPE html>
        <html>
        <head>
            <meta name='viewport' content='width=device-width, initial-scale=1'>
            <script src='https://cdn.jsdelivr.net/npm/chart.js'></script>
            <script>
            var ws = new WebSocket("ws://" + window.location.host + "/ws");
            ws.onmessage = function(event) {
                var data = JSON.parse(event.data);
                document.getElementById("pHValue").innerText = data.pH;
                document.getElementById("tdsValue").innerText = data.tds + " ppm";
                document.getElementById("turbValue").innerText = data.turbidity + " NTU";
                document.getElementById("tempValue").innerText = data.temperature + " °C";
            };
            </script>
            <style>
                body { font-family: Arial, sans-serif; text-align: center; background: #121212; color: white; }
                .card { padding: 10px; margin: 10px; background: #222; border-radius: 10px; display: inline-block; width: 150px; }
                .value { font-size: 1.5em; font-weight: bold; }
            </style>
        </head>
        <body>
            <h2>Water Quality Monitoring</h2>
            <div class='card'><h3>pH Level</h3><p class='value' id='pHValue'>--</p></div>
            <div class='card'><h3>TDS</h3><p class='value' id='tdsValue'>--</p></div>
            <div class='card'><h3>Turbidity</h3><p class='value' id='turbValue'>--</p></div>
            <div class='card'><h3>Temperature</h3><p class='value' id='tempValue'>--</p></div>
        </body>
        </html>
        )rawliteral");
    });

    ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
        if (type == WS_EVT_CONNECT) {
            client->text("{\"message\":\"Connected\"}");
        }
    });

    server.addHandler(&ws);
    server.begin();
}

void loop() {
    Wire.begin(D3, D4);
    int16_t phRaw = ads.readADC(0);
    int16_t turbidityRaw = ads.readADC(1);
    int16_t tdsRaw = ads.readADC(2);

    float phVoltage = phRaw * 0.0001875;
    float turbidityVoltage = turbidityRaw * 0.0001875;
    float tdsVoltage = tdsRaw * 0.0001875;

    pH = constrain(4.167 * phVoltage, 0, 14);
    turbidity = constrain((1 - turbidityVoltage) * 100, 0, 100);
    tds = constrain(tdsVoltage * 100, 0, 100);

    sensors.requestTemperatures();
    temperature = constrain(sensors.getTempCByIndex(0), 0, 50);

    Serial.printf("pH: %.2f | TDS: %.1f ppm | Turbidity: %.1f NTU | Temp: %.1f°C\n", pH, tds, turbidity, temperature);

    Wire.begin(D2, D1);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("pH:"); lcd.print(pH, 2);
    lcd.print(" T:"); lcd.print(temperature, 1);
    lcd.setCursor(0, 1);
    lcd.print("TDS:"); lcd.print(tds, 1);
    lcd.print(" Turb:"); lcd.print(turbidity, 1);

    notifyClients();
    delay(1000);
}
