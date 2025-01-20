#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <Bounce2.h>
#include <ESPmDNS.h>

// Wi-Fi 配置
const char* ssid = "ESP32-AP";
const char* password = "12345678";

// 按键相关
const int button1Pin = 2;
const int button2Pin = 0;
const int button3Pin = 4;

Bounce button1 = Bounce();
Bounce button2 = Bounce();
Bounce button3 = Bounce();

// LED 参数
#define LED_PIN1   25
#define LED_PIN2   26
#define LED_PIN3   27
#define NUM_LEDS1  79
#define NUM_LEDS2  28
#define NUM_LEDS3  8

int SEGMENT = 8;  // 动态参数
int speed1 = 20;  // 模拟通路速度
int speed2 = 20;  // 模拟断路速度
int speed3 = 40;  // 短路速度

Adafruit_NeoPixel strip1(NUM_LEDS1, LED_PIN1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2(NUM_LEDS2, LED_PIN2, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip3(NUM_LEDS3, LED_PIN3, NEO_GRB + NEO_KHZ800);

// 显示模式
volatile int displayMode = 0; // 使用volatile修饰，避免任务切换时数据不一致

// Web服务器
WebServer server(80);
String parameter = "默认参数";

// HTML 页面
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 参数设置</title>
    <meta charset="UTF-8">
</head>
<body>
    <h1>设置参数</h1>
    <form action="/set">
        <label for="segment">SEGMENT (灯效段数):</label>
        <input type="number" id="segment" name="segment" value="%d">
        <br>
        <label for="speed1">通路速度 (ms):</label>
        <input type="number" id="speed1" name="speed1" value="%d">
        <br>
        <label for="speed2">断路速度 (ms):</label>
        <input type="number" id="speed2" name="speed2" value="%d">
        <br>
        <label for="speed3">短路速度 (ms):</label>
        <input type="number" id="speed3" name="speed3" value="%d">
        <br>
        <input type="submit" value="更新">
    </form>
    <p>当前 SEGMENT: %d</p>
    <p>当前速度: 通路 %d ms, 断路 %d ms, 短路 %d ms</p>
</body>
</html>
)rawliteral";

// Web处理函数
void handleRoot() {
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), htmlPage, SEGMENT, speed1, speed2, speed3, SEGMENT, speed1, speed2, speed3);
    server.send(200, "text/html", buffer);
}

void handleSet() {
    if (server.hasArg("segment")) {
        SEGMENT = server.arg("segment").toInt();
    }
    if (server.hasArg("speed1")) {
        speed1 = server.arg("speed1").toInt();
    }
    if (server.hasArg("speed2")) {
        speed2 = server.arg("speed2").toInt();
    }
    if (server.hasArg("speed3")) {
        speed3 = server.arg("speed3").toInt();
    }
    server.sendHeader("Location", "/");
    server.send(303);
}

// 初始化灯带
void initStrips() {
    strip1.clear();
    strip2.clear();
    strip3.clear();
    strip1.show();
    strip2.show();
    strip3.show();
}

// 按键检测任务
void buttonTask(void* param) {
    while (true) {
        button1.update();
        button2.update();
        button3.update();

        if (button1.fell()) {
            displayMode = 0;
        }
        if (button2.fell()) {
            displayMode = 1;
        }
        if (button3.fell()) {
            displayMode = 2;
        }
        vTaskDelay(pdMS_TO_TICKS(20)); // 每20ms检测一次
    }
}

// 模式显示任务
void displayTask(void* param) {
    while (true) {
        switch (displayMode) {
            case 0:
                simulatePathway();
                break;
            case 1:
                simulateBreak();
                break;
            case 2:
                simulateShortCircuit();
                break;
            default:
                break;
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // 控制每个循环的执行频率
    }
}

// 模拟通路效果
void simulatePathway() {
    initStrips();
    for (int i = 0; i < NUM_LEDS1 + SEGMENT; i++) {
        if (displayMode != 0) return;

        strip1.clear();
        strip3.fill(strip3.Color(255, 255, 255)); // 灯带3常亮白色

        for (int j = 0; j < SEGMENT; j++) {
            int ledIndex = i - j;
            if (ledIndex >= 0 && ledIndex < NUM_LEDS1) {
                uint8_t brightness = 255 - (j * 255 / SEGMENT);
                strip1.setPixelColor(ledIndex, strip1.Color(0, brightness, 0));
            }
        }

        strip1.show();
        strip3.show();
        vTaskDelay(pdMS_TO_TICKS(speed1));
    }
}

// 模拟断路效果
void simulateBreak() {
    initStrips();
    for (int i = 0; i < NUM_LEDS1 + SEGMENT; i++) {
        if (displayMode != 1) return;

        strip1.clear();
        strip3.clear(); // 灯带3全部关闭

        for (int j = 0; j < SEGMENT; j++) {
            int ledIndex = i - j;
            if (ledIndex >= 0 && ledIndex < 22) {
                uint8_t brightness = 255 - (j * 255 / SEGMENT);
                strip1.setPixelColor(ledIndex, strip1.Color(0, brightness, 0));
            }
        }

        for (int k = 22; k < NUM_LEDS1; k++) {
            if (k < 35 || k > 46) { // 跳过35到46号灯珠
                strip1.setPixelColor(k, strip1.Color(255, 0, 0));
            }
        }

        strip1.show();
        strip3.show();
        vTaskDelay(pdMS_TO_TICKS(speed2));
    }
}

void simulateShortCircuit() {
    // 更新灯带1
    for (int i = 0; i < strip1.numPixels(); i++) {
        if (i >= 35 && i <= 46) {
            strip1.setPixelColor(i, strip1.Color(0, 0, 0)); // 关闭第35到46号灯珠
        } else {
            strip1.setPixelColor(i, strip1.Color(255, 0, 0)); // 其余设置为红色
        }
    }

    // 更新灯带2
    for (int i = 0; i < strip2.numPixels(); i++) {
        strip2.setPixelColor(i, strip2.Color(255, 0, 0)); // 全部设置为红色
    }

    // 更新灯带3
    for (int i = 0; i < strip3.numPixels(); i++) {
        strip3.setPixelColor(i, strip3.Color(0, 0, 0)); // 关闭所有灯珠
    }

    // 更新显示
    strip1.show();
    strip2.show();
    strip3.show();

    // 延迟一段时间，模拟短路效果
    vTaskDelay(pdMS_TO_TICKS(speed3)); // speed3 是延迟时间，可调
}


void setup() {
    Serial.begin(115200);
    WiFi.softAP(ssid, password);
    if (!MDNS.begin("dl")) {
        Serial.println("Error setting up MDNS responder!");
    }
    Serial.println("mDNS responder started");

    server.on("/", handleRoot);
    server.on("/set", handleSet);
    server.begin();

    strip1.begin();
    strip2.begin();
    strip3.begin();
    initStrips();

    button1.attach(button1Pin, INPUT_PULLUP);
    button1.interval(50);
    button2.attach(button2Pin, INPUT_PULLUP);
    button2.interval(50);
    button3.attach(button3Pin, INPUT_PULLUP);
    button3.interval(50);

    xTaskCreate(buttonTask, "ButtonTask", 2048, NULL, 1, NULL);
    xTaskCreate(displayTask, "DisplayTask", 4096, NULL, 1, NULL);

    Serial.println("System ready. Use Serial commands to control modes.");
}

void loop() {
    server.handleClient();

    // 处理串口输入
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n'); // 读取串口输入直到换行符
        command.trim(); // 去除多余的空格和换行符

        if (command.equalsIgnoreCase("mode 0")) {
            displayMode = 0;
            Serial.println("Display mode set to 0: Pathway");
        } else if (command.equalsIgnoreCase("mode 1")) {
            displayMode = 1;
            Serial.println("Display mode set to 1: Break");
        } else if (command.equalsIgnoreCase("mode 2")) {
            displayMode = 2;
            Serial.println("Display mode set to 2: Short Circuit");
        } else if (command.startsWith("speed ")) {
            int newSpeed = command.substring(6).toInt(); // 提取数字部分
            if (newSpeed > 0) {
                SEGMENT = newSpeed; // 更新速度
                Serial.print("Segment parameter updated to: ");
                Serial.println(SEGMENT);
            } else {
                Serial.println("Invalid speed value. Please provide a positive number.");
            }
        } else {
            Serial.println("Invalid command. Use 'mode 0', 'mode 1', 'mode 2', or 'speed <value>'");
        }
    }
}
