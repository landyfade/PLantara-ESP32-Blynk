/*
  ==============================================================
  SISTEM IRIGASI CERDAS - SEMI OTOMATIS
  Muhammad Fadlan - 2207431057
  ==============================================================
  note:
  - Pin Relay dipindah ke 25 untuk menghindari potensi kerusakan di pin 26.
  - VCC Relay WAJIB disambungkan ke 3.3V ESP32 untuk menyamakan level logika.
  - Alarm dibuat non-blocking agar tidak mengganggu komunikasi Blynk.
*/

// --- PENGATURAN BLYNK & WIFI ---
#define BLYNK_TEMPLATE_ID "TMPL6uk1tF4SB"
#define BLYNK_TEMPLATE_NAME "Sistem Irigasi Cerdas"
#define BLYNK_AUTH_TOKEN    "FPchqOpA97507CoM_VBhMmFm18yB_gWQ"
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

// setup wifi
char ssid[] = "Fanara";
char pass[] = "wanda023005";

// --- PENGATURAN PINOUT (FINAL) ---
const int PIN_SENSOR_TANAH = 34;
const int PIN_RELAY_POMPA  = 25; // PIN DIPINDAHKAN!
const int PIN_LED_HIJAU    = 19;
const int PIN_LED_MERAH    = 5;
const int PIN_BUZZER       = 18;

// --- PENGATURAN LOGIKA & KALIBRASI (SESUAIKAN HASIL TES-MU!) ---
const int NILAI_SENSOR_UDARA = 3100; // Ganti dengan nilai ADC saat sensor di udara!
const int NILAI_SENSOR_AIR   = 1500; // Ganti dengan nilai ADC saat sensor terendam air!

const int POMPA_ON  = LOW;
const int POMPA_OFF = HIGH;

const int THRESHOLD_KERING   = 35;
const int THRESHOLD_BASAH    = 75;
const long DURASI_MAKS_SIRAM = 15 * 1000;

// --- VARIABEL GLOBAL ---
bool statusAirHabis = false;
bool pompaAktif     = false;
unsigned long waktuMulaiSiram = 0;
BlynkTimer timer;
int buzzerTimerId = -1;

// --- FUNGSI-FUNGSI ---

void bunyikanAlarm() {
  digitalWrite(PIN_BUZZER, HIGH);
  timer.setTimeout(100L, []() {
    digitalWrite(PIN_BUZZER, LOW);
  });
}

BLYNK_WRITE(V3) {
  Serial.println("\n>>> Tombol Reset V3 DITERIMA oleh ESP32! <<<\n");
  if (param.asInt() == 1 && statusAirHabis) {
    statusAirHabis = false;
    if (buzzerTimerId != -1) {
      timer.disable(buzzerTimerId);
      buzzerTimerId = -1;
    }
    digitalWrite(PIN_LED_MERAH, LOW);
    digitalWrite(PIN_BUZZER, LOW);
    digitalWrite(PIN_LED_HIJAU, HIGH);
    Blynk.virtualWrite(V1, "Normal");
    Serial.println(">>> [RESET] Mode Darurat DISETEL ULANG. Sistem kembali normal. <<<\n");
  }
}

void cekKelembabanDanSiram() {
  if (statusAirHabis) {
    digitalWrite(PIN_RELAY_POMPA, POMPA_OFF);
    digitalWrite(PIN_LED_HIJAU, LOW);
    digitalWrite(PIN_LED_MERAH, HIGH);
    Serial.println("[DARURAT] Sistem terkunci. Pompa OFF. Tunggu reset...");
    return;
  }

  int nilaiSensor = analogRead(PIN_SENSOR_TANAH);
  int kelembaban = map(nilaiSensor, NILAI_SENSOR_UDARA, NILAI_SENSOR_AIR, 0, 100);
  kelembaban = constrain(kelembaban, 0, 100);
  
  Blynk.virtualWrite(V0, kelembaban);
  Serial.print("[INFO] Nilai ADC: "); Serial.print(nilaiSensor); Serial.print(" -> Kelembaban: "); Serial.print(kelembaban); Serial.println("%");

  if (kelembaban < THRESHOLD_KERING && !pompaAktif) {
    Serial.println("[AKSI] Tanah kering. -> PERINTAH: POMPA ON");
    pompaAktif = true;
    waktuMulaiSiram = millis();
    digitalWrite(PIN_RELAY_POMPA, POMPA_ON);
    Blynk.virtualWrite(V1, "Menyiram...");
    Blynk.virtualWrite(V2, 255);
  }
  else if (kelembaban >= THRESHOLD_BASAH && pompaAktif) {
    Serial.println("[AKSI] Tanah basah. -> PERINTAH: POMPA OFF");
    pompaAktif = false;
    digitalWrite(PIN_RELAY_POMPA, POMPA_OFF);
    Blynk.virtualWrite(V1, "Normal");
    Blynk.virtualWrite(V2, 0);
  }
  
  if (pompaAktif) {
    digitalWrite(PIN_LED_HIJAU, !digitalRead(PIN_LED_HIJAU));
    if (millis() - waktuMulaiSiram > DURASI_MAKS_SIRAM) {
      Serial.println("\n!!! [PERINGATAN] Timer Keamanan Tercapai. Mengaktifkan Mode Darurat !!!\n");
      statusAirHabis = true;
      pompaAktif = false;
      digitalWrite(PIN_RELAY_POMPA, POMPA_OFF);
      Blynk.virtualWrite(V1, "AIR HABIS!");
      Blynk.virtualWrite(V2, 0);
      Blynk.logEvent("air_habis", "Sistem mendeteksi air kemungkinan habis.");
      if (buzzerTimerId == -1) {
        buzzerTimerId = timer.setInterval(2000L, bunyikanAlarm);
      }
    }
  } else {
    digitalWrite(PIN_LED_HIJAU, HIGH);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n[SETUP] Memulai Sistem Irigasi Cerdas Final...");
  pinMode(PIN_RELAY_POMPA, OUTPUT);
  pinMode(PIN_LED_HIJAU, OUTPUT);
  pinMode(PIN_LED_MERAH, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_RELAY_POMPA, POMPA_OFF);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  timer.setInterval(2000L, cekKelembabanDanSiram);
}

void loop() {
  Blynk.run();
  timer.run();
}