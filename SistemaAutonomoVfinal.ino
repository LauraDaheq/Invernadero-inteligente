//--------------------------------------------------------------------------------------------------------
//                                               LIBRERIAS
//--------------------------------------------------------------------------------------------------------
#include <U8g2lib.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include <WiFi.h>
#include <time.h>
#include <driver/ledc.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
//--------------------------------------------------------------------------------------------------------
//                                               PINES
//--------------------------------------------------------------------------------------------------------
// OLED SH1106 SPI
#define OLED_MOSI   23 //D1
#define OLED_CLK    18 //D0
#define OLED_DC     17 //DC
#define OLED_CS     5
#define OLED_RESET  16 //RES

//U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI display(U8G2_R0, 5, 17, 16);
//U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 5, /* dc=*/ 17, /* reset=*/ 16);
U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI display(U8G2_R0, OLED_CS , OLED_DC, OLED_RESET);

// DHT22 sensor
#define DHTPIN 15
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Sensor de humedad de suelo
#define SOIL_PIN 34

// Sensor ultrasónico
#define TRIG_PIN 26
#define ECHO_PIN 27

//Salidas digitales a Reles
#define FAN_PIN    13   // Ventilador
#define HEAT_PIN   21   // Calefacción
#define HUMID_PIN  25   // Humidificador
#define WATER_PIN  33   // Bomba de riego

//Salidas PWM
// Pines para salidas PWM
#define redPin 2
#define bluePin 4
#define greenPin 32
//--------------------------------------------------------------------------------------------------------
//                                               VARIABLES Y PARAMETROS
//--------------------------------------------------------------------------------------------------------
// Configuración común PWM
const int freq = 5000;   // Frecuencia: 10kHz
const int resolution = 8; // Resolución de 8 bits (0-255)

// Conexion Red WiFi
//const char* ssid = "ESDARA";
//const char* password = "HANUMAN03211923";

//const char* ssid = "mrVidPul";
//const char* password = "123456780";

const char* ssid = "lab_control";
const char* password = "lab_control";

// Parametros de dibujo del titulo en OLED
int count_main_title = 0;
bool change_title = true;
int transition_time_title = 10;
String print_cool = "OFF";
String print_heat = "OFF";
String print_humid = "OFF";
String print_water = "OFF";
String print_light = "OFF";
bool firsTime= true; //Pantalla de carga

// Parametros del tanque
float tank_depth=30; //cm
float min_percent_tank = 50;
float block_moto = false;

//Parametros humedad suelo
float min_soil = 60;
bool active_moto =false;

//Parametros humedad ambiental
float min_humid = 40;
bool active_humid =false;
//Parametros temperatura
float min_temp = 18;
float max_temp = 24;
bool active_fan =false;
bool active_heat =false;
//Parametros foto periodo

String duracionLuces = "00:01:00"; //Horas
String horaInicio = "00:01:00";
long count = 0;

//Parametros LED 0->0  255->100
int red_intensity = 0;
int blue_intensity = 0;

//parametros reloj
bool clock_conection = false;
//--------------------------------------------------------------------------------------------------------
//                                             CONFIGURACION PLACA
//--------------------------------------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  delay(1000);

  Serial.println("Conectando a WiFi...");
  WiFi.begin(ssid, password);
  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 20) {
    delay(500);
    Serial.print(".");
    intentos++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi conectado!");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nNo se pudo conectar.");
  }

   // Zona horaria Colombia (UTC-5)
  configTime(-5 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  // Esperar sincronización
  struct tm timeinfo;
  intentos = 0;
  while (!getLocalTime(&timeinfo) && intentos < 10) {
    Serial.println("Esperando hora NTP...");
    delay(1000);
    intentos++;
  }
  if (getLocalTime(&timeinfo)) {
    Serial.println("Hora obtenida:");
    Serial.printf("%02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    clock_conection = true;
  } else {
    Serial.println("No se pudo obtener la hora.");
    clock_conection = false;
  }

  display.begin();
  Serial.println("OLED SSD1306 0.96 active...");

    // Configurar los tres canales PWM
  ledcAttachChannel(redPin, freq, resolution,0);
  ledcAttachChannel(bluePin, freq, resolution,1);
  ledcAttachChannel(greenPin, freq, resolution,2);
  //pinMode(redPin, OUTPUT);
  //pinMode(bluePine,OUTPUT);
  //pinMode(greenPine,OUTPUT);
  Serial.println("PWM chanels active...");

  dht.begin();
  Serial.println("DHT22 sensor active...");

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  Serial.println("Ultrasonic sensor active...");

  pinMode(FAN_PIN, OUTPUT);   // Ventilador
  pinMode(HEAT_PIN, OUTPUT);  // Calefacción
  pinMode(HUMID_PIN, OUTPUT);   // Humidificador
  pinMode( WATER_PIN, OUTPUT);   // Bomba de riego
  Serial.println("Digital outputs active...");

  
  //while (horaInicio == "") {
  //  horaInicio = leerHoraDesdeSerial();  // Espera entrada válida
  //}

  //Serial.print("Hora recibida correctamente: ");
  //Serial.println(horaInicio);
}

//--------------------------------------------------------------------------------------------------------
//                                               FUNCIONES
//--------------------------------------------------------------------------------------------------------
float getTemperature() {
  return dht.readTemperature();
}

float getHumidity() {
  return dht.readHumidity();
}

float getSoilMoisture() {
  float percentSoilMoisture = 100*(1-analogRead(SOIL_PIN)/4095.0);
  //Serial.println(percentSoilMoisture);
  return percentSoilMoisture;
}

float getTankLevel() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);
  float distance = duration * 0.034 / 2;
  float percent = 100*(1-1/tank_depth*distance);
  if (percent<0){
    return 0.0;
  }else{
    //Serial.println(distance);
    return percent;
  }
}

unsigned long convertirAHoraEnMilisegundos(String hora) {
  int h = 0, m = 0, s = 0;

  // Verifica si el formato es correcto: longitud 8 y separadores en la posición 2 y 5
  if (hora.length() == 8 && hora.charAt(2) == ':' && hora.charAt(5) == ':') {
    h = hora.substring(0, 2).toInt();
    m = hora.substring(3, 5).toInt();
    s = hora.substring(6, 8).toInt();
  } else {
    Serial.println("Formato de hora inválido. Debe ser HH:MM:SS");
    return 0;
  }

  // Calcula los milisegundos
  unsigned long totalMilisegundos = ((unsigned long)h * 3600 + m * 60 + s) * 1000;
  return totalMilisegundos;
}

String leerHoraDesdeSerial() {
  String hora = "";

  while (Serial.available() == 0) {
    // Espera a que el usuario escriba algo
    delay(100);
  }

  hora = Serial.readStringUntil('\n');
  hora.trim();  // Elimina espacios o saltos de línea

  // Validación de formato: debe tener 8 caracteres y ':' en posiciones 2 y 5
  if (hora.length() != 8 || hora.charAt(2) != ':' || hora.charAt(5) != ':') {
    Serial.println("Formato inválido. Use HH:MM:SS");
    return "";
  }

  // Opcional: Validar valores numéricos
  int h = hora.substring(0, 2).toInt();
  int m = hora.substring(3, 5).toInt();
  int s = hora.substring(6, 8).toInt();

  if (h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 59) {
    Serial.println("Valores fuera de rango. Use HH de 00 a 23, MM y SS de 00 a 59.");
    return "";
  }

  return hora;
}


//--------------------------------------------------------------------------------------------------------
//                                FUNCION PARA ENVIAR LOS DATOS AL CANAL DE THINGSPEAK
//--------------------------------------------------------------------------------------------------------
void enviarDatosAThingSpeak(float temp, float hum, float suelo, float tanque) {
  if (
    isnan(temp) || temp < -40 || temp > 80 ||
    isnan(hum) || hum < 0 || hum > 100 ||
    isnan(suelo) || suelo < 0 || suelo > 100 ||
    isnan(tanque) || tanque < 0 || tanque > 100
  ) {
    Serial.println("❌ Lectura inválida. Datos no enviados a ThingSpeak.");
    return; // No se envía nada
  }

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.thingspeak.com/update?api_key=MQ7YSC1YB1ZEDEOW";
    url += "&field1=" + String(temp, 2);
    url += "&field2=" + String(hum, 2);
    url += "&field3=" + String(suelo, 2);
    url += "&field4=" + String(tanque, 2);
    url += "&field5=" + String(active_moto ? 1 : 0); 
    url += "&field6=" + String(print_light == "ON" ? 1 : 0);

    http.begin(url);
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      Serial.print("✅ Datos enviados a ThingSpeak. Código: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("❌ Error al enviar datos: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  }
}


//--------------------------------------------------------------------------------------------------------
//                  FUNCION PARA LEER COMANDOS DE CONTROL DESDE THINGSPEAK (LUZ Y RIEGO)
//--------------------------------------------------------------------------------------------------------
void leerComandosDesdeThingSpeak() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.thingspeak.com/channels/2993017/feeds/last.json?api_key=IFDLL1D2OQV7OZCD";
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);

      int riego = doc["field5"].as<int>();
      int luces = doc["field6"].as<int>();

      // Control del riego
      if (riego == 1) {
        digitalWrite(WATER_PIN, HIGH);
        print_water = "ON ";
      } else {
        digitalWrite(WATER_PIN, LOW);
        print_water = "OFF ";
      }

      // Control de luces
      if (luces == 1) {
        ledcWrite(0, 254);   // canal 0: redPin
        ledcWrite(1, 1);     // canal 1: bluePin
        ledcWrite(2, 128);   // canal 2: greenPin
        print_light = "ON";
      } else {
        ledcWrite(0, 0);
        ledcWrite(1, 0);
        ledcWrite(2, 0);
        print_light = "OFF";
      }

    } else {
      Serial.println("❌ Error al obtener comandos desde ThingSpeak");
    }
    http.end();
  }
}

//--------------------------------------------------------------------------------------------------------
//                                               FUNCION PRINCIPAL
//--------------------------------------------------------------------------------------------------------
long milisegundosHoraActual ;
char horaTexto[9];
bool flag = false;

void loop() {
  leerComandosDesdeThingSpeak();  
  float temp = getTemperature();
  float hum = getHumidity();
  float soil = getSoilMoisture();
  float tank = getTankLevel();
  enviarDatosAThingSpeak(temp, hum, soil, tank); 



  //Serial.println(ledcRead(redPin));
  //Serial.println(ledcRead(bluePin));
  //Serial.println(ledcRead(greenPin));

  //Control temperatura maxima
  if(temp > max_temp){
    active_fan = true;
    print_cool = "ON ";
    digitalWrite(FAN_PIN, HIGH);
  }else{
    active_fan = false;
    print_cool = "OFF";  
    digitalWrite(FAN_PIN, LOW); 
  }

    //Control temperatura minima
  if(temp < min_temp){
    active_heat = true;
    print_heat = "ON ";
    digitalWrite(HEAT_PIN, HIGH);
  }else{
    active_heat = false;
    print_heat = "OFF"; 
    digitalWrite(HEAT_PIN, LOW);  
  }

  //Control humedad ambiental minima
  if(hum < min_humid){
    active_fan = true;
    active_humid = true;
    print_humid = "ON ";
    digitalWrite(HUMID_PIN, HIGH);
  }else{
    active_fan = false;
    active_humid = false;
    print_humid= "OFF";  
    digitalWrite(HUMID_PIN, LOW); 
  }

  //Control bloqueo motobomba
  if(tank < min_percent_tank ){
    block_moto = true;
  }else{
    block_moto = false;
  }

  //Control humedad suelo minima
  if(soil < min_soil && block_moto != true){
    active_moto = true;
    print_water = "ON ";
    digitalWrite(WATER_PIN, HIGH);
    delay(500); //Definir tiempo de riego
    digitalWrite(WATER_PIN, LOW); 
  }else{
    active_moto = false;
    print_water = "OFF "; 
    digitalWrite(WATER_PIN, LOW); 
  }

  display.clearBuffer();

  if(count_main_title==transition_time_title){
    change_title=false;
  }
  if(count_main_title==0){
    change_title=true;
  }

  struct tm timeinfo;   
  if (getLocalTime(&timeinfo)) {
    sprintf(horaTexto, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  }else{
    Serial.println("Error mostrando la hora");
  }
  
  if (change_title){ 
    display.setFont(u8g2_font_6x10_tf);  // Fuente grande tipo reloj
    display.setCursor(41, 10);             // Posición de la hora
    display.printf(horaTexto);
    count_main_title++;
  }else{
    display.setFont(u8g2_font_6x10_tf);
    display.setCursor(24, 10);
    display.printf("SYSTEM ACTIVE");    
    count_main_title--;
  }

  display.setFont(u8g2_font_5x8_tf);
  display.setCursor(0, 25);
  display.printf("Soil: %.1f%%", soil);
  display.setCursor(0, 37);
  display.printf("Temp: %.1fC ", temp); 
  display.setCursor(0, 49);
  display.printf("Humi: %.1f%%", hum); 
  display.setCursor(0, 61);
  display.printf("Tank: %.1f%%", tank);

  milisegundosHoraActual = convertirAHoraEnMilisegundos(horaTexto);
  Serial.println(convertirAHoraEnMilisegundos(horaTexto));
  Serial.println(convertirAHoraEnMilisegundos(horaInicio));
  Serial.println(convertirAHoraEnMilisegundos(duracionLuces));
  Serial.println(count);

  if(milisegundosHoraActual >= convertirAHoraEnMilisegundos(horaInicio) && milisegundosHoraActual <= (convertirAHoraEnMilisegundos(horaInicio)+2000)){
    flag = true;
  }

  if( flag && count<convertirAHoraEnMilisegundos(duracionLuces)){
    print_light="ON ";
    count=count+1000;
    ledcWrite(redPin, red_intensity);
    ledcWrite(bluePin, blue_intensity);
    ledcWrite(greenPin, 0);
    
  }else{
    print_light="OFF ";
    ledcWrite(redPin, 255);
    ledcWrite(bluePin, 255);
    ledcWrite(greenPin, 0);
    count=0;
    flag = false;
  }

  display.setCursor(75, 23);
  display.printf(" HEAT: %s", print_heat);
  display.setCursor(75, 33);
  display.printf(" COOL: %s", print_cool);
  display.setCursor(75, 43);
  display.printf("HUMID: %s", print_humid);
  display.setCursor(75, 53);
  display.printf("WATER: %s", print_water);
  display.setCursor(75, 63);
  display.printf("LIGHT: %s", print_light);
  display.sendBuffer();



  delay(1000); // actualización cada 1 segundo
}
