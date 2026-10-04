#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <MQTT.h>
#include <DHT.h>
#include <time.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <atomic>

#include "configuracion.h"
#include "internet.h"
#include "certificado.h"

// ---------------- OBJETOS ----------------
DHT dht(PIN_DHT, DHTTYPE);
WiFiClientSecure conexion;
MQTTClient mqtt(512);
esp_timer_handle_t temporizadorRiego = nullptr;
std::atomic<bool> limiteAlcanzado{false};

// ---------------- DATOS DE SENSORES ----------------
int lecturaSuelo = 0;
int humedadSuelo = 0;
int lecturaLuz = 0;
float temperatura = NAN;
float humedadAire = NAN;

bool dhtValido = false;

bool hayLectura = false;

// ---------------- TIEMPOS ----------------
uint32_t ultimaLectura = 0;
uint32_t ultimoCiclo = 0;
uint32_t ultimoEnvio = 0;
uint32_t ultimoIntentoWiFi = 0;
uint32_t ultimoIntentoMQTT = 0;

// ---------------- MQTT Y FEEDS ----------------
String raiz;
String clienteId;
String idEnsayo;
const char *feeds[] = {
  "suelo",
  "temperatura",
  "humedad-aire",
  "luz",
  "orden-riego",
  "senal-wifi",
  "dht-valido"
};
String valores[7];

bool publicarValor[7];
int pasoEnvio = -1;
int ultimaOrdenPublicada = -1;

bool wifiAnterior = false;
uint32_t numeroPrueba = 0;
uint32_t inicioPrueba = 0;
uint32_t ultimoRTT = 0;
uint32_t pruebasEnviadas = 0;
uint32_t pruebasRecibidas = 0;
uint32_t pruebasSinEco = 0;
String mensajeEsperado;

bool esperandoEco = false;

bool rttDisponible = false;

bool rttPorPublicar = false;


void apagarPorTiempo(void *arg) {
  digitalWrite(PIN_RELE, RELE_OFF);
  bombaActiva.store(false);
  limiteAlcanzado.store(true);
}

void apagarBomba() {
  digitalWrite(PIN_RELE, RELE_OFF);
  bombaActiva.store(false);
  esp_timer_stop(temporizadorRiego);
}

void encenderBomba() {
  if (bombaActiva.load() || limiteAlcanzado.load()) {
    return;
  }
  esp_timer_stop(temporizadorRiego);
  bombaActiva.store(true);
  digitalWrite(PIN_RELE, RELE_ON);
  esp_err_t resultado = esp_timer_start_once(
    temporizadorRiego,
    (uint64_t)MAX_RIEGO_MS * 1000ULL
  );
  if (resultado != ESP_OK) {
    apagarBomba();
    limiteAlcanzado.store(true);
    Serial.println("ERROR de temporizador. Bomba apagada.");
  }
}

// ---------------- RIEGO AUTOMATICO ----------------

void controlarRiego() {
  if (!hayLectura) {
    apagarBomba();
    return;
  }
  if (humedadSuelo >= UMBRAL_RIEGO) {
    apagarBomba();
    limiteAlcanzado.store(false);
    return;
  }
  if (!limiteAlcanzado.load()) {
    encenderBomba();
  }
}

// ---------------- LECTURA DE SENSORES ----------------

void leerSensores() {
  lecturaSuelo = analogRead(PIN_SUELO);
  humedadSuelo = map(
    lecturaSuelo,
    SUELO_SECO,
    SUELO_HUMEDO,
    0,
    100
  );
  humedadSuelo = constrain(humedadSuelo, 0, 100);
  lecturaLuz = analogRead(PIN_LUZ);
  temperatura = dht.readTemperature();
  humedadAire = dht.readHumidity();
  dhtValido =
    !isnan(temperatura) &&
    !isnan(humedadAire);
  hayLectura = true;
}

// ---------------- MONITOR SERIE ----------------

void mostrarDatos() {
  Serial.println();
  Serial.println("------------------------------------");
  Serial.print("Suelo - Lectura cruda: ");
  Serial.println(lecturaSuelo);
  Serial.print("Suelo - Humedad estimada: ");
  Serial.print(humedadSuelo);
  Serial.println("%");
  Serial.print("Luz - Lectura: ");
  Serial.print(lecturaLuz);
  Serial.println(" ADC");
  if (dhtValido) {
    Serial.print("Temperatura: ");
    Serial.print(temperatura, 1);
    Serial.println(" C");
    Serial.print("Humedad del aire: ");
    Serial.print(humedadAire, 1);
    Serial.println("%");
  } else {
    Serial.println("DHT11 - ERROR: No se pudo leer");
  }
  Serial.print("DHT11 - Lectura valida: ");
  Serial.println(dhtValido ? "SI" : "NO");
  Serial.println();
  if (bombaActiva.load()) {
    Serial.println("Humedad del suelo BAJA");
    Serial.println("-> RIEGO ACTIVADO");
  } else if (limiteAlcanzado.load()) {
    Serial.println("Limite de riego alcanzado");
    Serial.println("-> RIEGO DESACTIVADO");
    Serial.println("Esperando humedad suficiente");
  } else {
    Serial.println("Humedad del suelo SUFICIENTE");
    Serial.println("-> RIEGO DESACTIVADO");
  }
  Serial.println();
  Serial.print("Wi-Fi: ");
  Serial.println(
    WiFi.status() == WL_CONNECTED
      ? "CONECTADO"
      : "DESCONECTADO"
  );
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Senal Wi-Fi: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  }
  Serial.print("MQTT: ");
  Serial.println(
    mqtt.connected() ? "CONECTADO" : "DESCONECTADO"
  );
  Serial.print("QoS: ");
  Serial.println(MQTT_QOS);
  Serial.print("Ultimo tiempo de ida y vuelta: ");
  if (rttDisponible) {
    Serial.print(ultimoRTT);
    Serial.println(" ms");
  } else {
    Serial.println("Pendiente");
  }
  Serial.print("Pruebas MQTT enviadas: ");
  Serial.println(pruebasEnviadas);
  Serial.print("Pruebas con retorno: ");
  Serial.println(pruebasRecibidas);
  Serial.print("Pruebas sin retorno en 10 segundos: ");
  Serial.println(pruebasSinEco);
  if (MOSTRAR_CSV) {
    Serial.printf(
      "RESUMEN,%s,%d,%lu,%lu,%lu,%d\n",
      idEnsayo.c_str(),
      MQTT_QOS,
      (unsigned long)pruebasEnviadas,
      (unsigned long)pruebasRecibidas,
      (unsigned long)pruebasSinEco,
      esperandoEco
    );
  }
}

// ---------------- RECEPCION MQTT ----------------

void recibirMQTT(String &tema, String &mensaje) {
  if (tema != raiz + "prueba") {
    Serial.println("Aviso de Adafruit IO: " + mensaje);
    return;
  }
  if (!esperandoEco || mensaje != mensajeEsperado) {
    return;
  }
  uint32_t tiempo = millis() - inicioPrueba;
  esperandoEco = false;
  if (tiempo > 10000) {
    pruebasSinEco++;
    return;
  }
  ultimoRTT = tiempo;
  pruebasRecibidas++;
  rttDisponible = true;
  rttPorPublicar = true;
  if (MOSTRAR_CSV) {
    Serial.printf(
      "RTT,%s,%d,%lu,%lu,%d\n",
      idEnsayo.c_str(),
      MQTT_QOS,
      (unsigned long)numeroPrueba,
      (unsigned long)ultimoRTT,
      WiFi.RSSI()
    );
  }
}

// ---------------- INTERNET Y MQTT ----------------

void mantenerConexion() {
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiAnterior) {
      conexion.stop();
      pasoEnvio = -1;
    }
    wifiAnterior = false;
    if (millis() - ultimoIntentoWiFi >= 15000) {
      ultimoIntentoWiFi = millis();
      WiFi.reconnect();
    }
    return;
  }
  if (!wifiAnterior) {
    wifiAnterior = true;
    configTime(
      0,
      0,
      "pool.ntp.org",
      "time.google.com"
    );
  }
  if (time(nullptr) < 1700000000) {
    return;
  }
  if (mqtt.connected()) {
    mqtt.loop();
    return;
  }
  if (millis() - ultimoIntentoMQTT < 15000) {
    return;
  }
  ultimoIntentoMQTT = millis();
  if (mqtt.connect(
        clienteId.c_str(),
        AIO_USERNAME,
        AIO_KEY
      )) {
    bool suscrito = mqtt.subscribe(
      raiz + "prueba",
      MQTT_QOS
    );
    mqtt.subscribe(
      String(AIO_USERNAME) + "/errors",
      0
    );
    mqtt.subscribe(
      String(AIO_USERNAME) + "/throttle",
      0
    );
    if (!suscrito) {
      mqtt.disconnect();
      conexion.stop();
      return;
    }
    ultimaOrdenPublicada = -1;
    pasoEnvio = -1;
  } else {
    Serial.print("MQTT - Conexion pendiente. Error ");
    Serial.println((int)mqtt.lastError());
    conexion.stop();
  }
}

// ---------------- PREPARAR DATOS ----------------

void prepararPublicacion() {
  if (
    !hayLectura ||
    !mqtt.connected() ||
    pasoEnvio >= 0
  ) {
    return;
  }
  valores[0] = String(humedadSuelo);
  valores[1] = String(temperatura, 1);
  valores[2] = String(humedadAire, 1);
  valores[3] = String(lecturaLuz);
  valores[5] = String(WiFi.RSSI());
  valores[6] = dhtValido ? "1" : "0";
  for (int i = 0; i < 7; i++) {
    publicarValor[i] = true;
  }
  publicarValor[1] = dhtValido;
  publicarValor[2] = dhtValido;
  pasoEnvio = 0;
}

bool enviarFeed(const char *feed, const String &valor) {
  ultimoEnvio = millis();
  return mqtt.publish(
    raiz + feed,
    valor,
    false,
    MQTT_QOS
  );
}

// ---------------- PUBLICAR SIN PAUSAS LARGAS ----------------

void publicarDatos() {
  if (
    !mqtt.connected() ||
    millis() - ultimoEnvio < INTERVALO_ENVIO
  ) {
    return;
  }
  int ordenActual = bombaActiva.load() ? 1 : 0;
  // Primero informa los cambios de la bomba.
  if (ordenActual != ultimaOrdenPublicada) {
    if (enviarFeed("orden-riego", String(ordenActual))) {
      ultimaOrdenPublicada = ordenActual;
    }
    return;
  }
  if (rttPorPublicar) {
    rttPorPublicar = false;
    enviarFeed("rtt", String(ultimoRTT));
    return;
  }
  if (pasoEnvio >= 0 && pasoEnvio < 7) {
    int posicion = pasoEnvio++;
    if (!publicarValor[posicion]) {
      return;
    }
    if (posicion == 4) {
      valores[posicion] = String(ordenActual);
    }
    bool enviado = enviarFeed(
      feeds[posicion],
      valores[posicion]
    );
    if (posicion == 4 && enviado) {
      ultimaOrdenPublicada = ordenActual;
    }
    return;
  }
  if (pasoEnvio == 7) {
    pasoEnvio = -1;
    if (esperandoEco) {
      return;
    }
    numeroPrueba++;
    pruebasEnviadas++;
    mensajeEsperado =
      idEnsayo + "-" + String(numeroPrueba);
    inicioPrueba = millis();
    esperandoEco = true;
    enviarFeed("prueba", mensajeEsperado);
  }
}

// ---------------- SETUP ----------------

void setup() {
  Serial.begin(115200);
  digitalWrite(PIN_RELE, RELE_OFF);
  pinMode(PIN_RELE, OUTPUT);
  digitalWrite(PIN_RELE, RELE_OFF);
  pinMode(PIN_SUELO, INPUT);
  pinMode(PIN_LUZ, INPUT);
  analogReadResolution(12);
  dht.begin();
  esp_timer_create_args_t configuracion = {};
  configuracion.callback = apagarPorTiempo;
  configuracion.name = "riego";
  if (
    esp_timer_create(
      &configuracion,
      &temporizadorRiego
    ) != ESP_OK
  ) {
    Serial.println("ERROR al crear temporizador.");
    Serial.println("Bomba apagada.");
    while (true) {
      delay(1000);
    }
  }
  raiz = String(AIO_USERNAME) + "/feeds/";
  idEnsayo = String(esp_random(), HEX);
  clienteId =
    "smart-farming-" +
    String((uint32_t)ESP.getEfuseMac(), HEX) +
    "-" +
    idEnsayo;
  conexion.setCACert(ROOT_CA);
  conexion.setHandshakeTimeout(5);
  mqtt.begin("io.adafruit.com", 8883, conexion);
  mqtt.setOptions(30, true, 1000);
  mqtt.onMessage(recibirMQTT);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println();
  Serial.println("====================================");
  Serial.println("       SMART FARMING - ESP32");
  Serial.println("====================================");
  Serial.println("Sensor suelo -> GPIO 35");
  Serial.println("Sensor luz   -> GPIO 34");
  Serial.println("DHT11        -> GPIO 4");
  Serial.println("Rele         -> GPIO 33");
  Serial.println("Rele activo en LOW");
  Serial.println("Umbral riego: 30%");
  Serial.println("Tiempo maximo de riego: 10 segundos");
  Serial.println();
  Serial.println("Sistema iniciado.");
}

// ---------------- LOOP ----------------

void loop() {
  if (millis() - ultimaLectura >= INTERVALO_LECTURA) {
    ultimaLectura = millis();
    leerSensores();
    controlarRiego();
    mostrarDatos();
  }
  mantenerConexion();
  if (
    esperandoEco &&
    millis() - inicioPrueba > 10000
  ) {
    esperandoEco = false;
    pruebasSinEco++;
  }
  if (millis() - ultimoCiclo >= INTERVALO_NUBE) {
    ultimoCiclo = millis();
    prepararPublicacion();
  }
  publicarDatos();
  delay(5);
}
