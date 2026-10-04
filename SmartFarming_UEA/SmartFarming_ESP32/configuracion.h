#pragma once
#include <Arduino.h>
#include <DHT.h>

// ---------------- PINES ----------------
#define PIN_SUELO 35
#define PIN_LUZ   34
#define PIN_RELE  33
#define PIN_DHT   4
#define DHTTYPE   DHT11

// ---------------- CONFIGURACION ----------------
const int UMBRAL_RIEGO = 30;
const int SUELO_SECO = 4095;
const int SUELO_HUMEDO = 0;
const int RELE_ON = LOW;
const int RELE_OFF = HIGH;
const uint32_t MAX_RIEGO_MS = 10000;
const uint32_t INTERVALO_LECTURA = 3000;
const uint32_t INTERVALO_NUBE = 30000;
const uint32_t INTERVALO_ENVIO = 2500;
const int MQTT_QOS = 0;
const bool MOSTRAR_CSV = false;
static_assert(
  SUELO_SECO != SUELO_HUMEDO,
  "Las referencias de calibracion deben ser diferentes"
);
