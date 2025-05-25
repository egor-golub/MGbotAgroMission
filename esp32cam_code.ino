#include "esp_camera.h"
#include "WiFi.h"
#include "FS.h"
#include "SD_MMC.h"
#include "time.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
const char* ssid = "Xiaomi_E217";
const char* password = "internet217";
const char* triggerTime = "20:10"; // время фиксации каждый день
const char* ntpServer = "pool.ntp.org";
const long gmtOffset = 3 * 3600; 
const int daylightOffset = 0;     
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22
bool photoTakenToday = false;

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  Serial.begin(115200);
  initCamera();
  connectToWiFi();
  configTime(gmtOffset, daylightOffset, ntpServer);
}

void loop() {
  
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Не удалось получить время!");
    delay(1000);
    return;
  }
  char currentTime[6];
  strftime(currentTime, sizeof(currentTime), "%H:%M", &timeinfo);
  if (strcmp(currentTime, triggerTime) == 0 && !photoTakenToday) {
    takePhoto(&timeinfo);
    photoTakenToday = true;
  } 

  else if (strcmp(currentTime, "00:00") == 0) {
    photoTakenToday = false;
  }

  delay(1000); 
  Serial.println(currentTime);
}


void initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Ошибка инициализации камеры: 0x%x", err);
    while (true) delay(1000);
  }
}

void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Подключение к Wi-Fi...");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Не удалось подключиться!");
    while (true) delay(1000);
  }
  
  Serial.println("\nПодключено!");
}

void takePhoto(struct tm *timeinfo) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Ошибка захвата фото");
    return;
  }
  
  if (!SD_MMC.begin()) {
    Serial.println("Ошибка монтирования SD-карты");
    esp_camera_fb_return(fb);
    return;
  }

  char fileName[64];
  strftime(fileName, sizeof(fileName), "/дата_%d_%m_%Y_время_%H%M.jpg", timeinfo);
  
  File file = SD_MMC.open(fileName, FILE_WRITE);
  if (!file) {
    Serial.println("Ошибка открытия файла");
  } else {
    file.write(fb->buf, fb->len);
    Serial.printf("Фото сохранено: %s\n", fileName);
  }
  
  file.close();
  SD_MMC.end();
  esp_camera_fb_return(fb);
}