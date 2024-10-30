//Incluimos las librerias
#include "DHTesp.h"
#include <Wire.h>			
#include <Adafruit_GFX.h>		
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WebServer.h> // Librería para el servidor web	
//Iniciamos la pantalla OLED
#define ANCHO 128			
#define ALTO 64	
#define OLED_RESET 4			
Adafruit_SSD1306 display(ANCHO, ALTO);	
//Iniciamos sensor DHT22
int pinDHT = 15;
DHTesp dht;
//Variables del temporizador de la grafica
int anteriorMillis = 0;
int tiempo = 0;
float temperatura = 0;
float graficaTemperatura = 0;

int x[128]; // Buffer de la gráfica 
int y[128]; // Buffer secundario de la gráfica 
float bufferTemperatura[5]; // Buffer para el promedio móvil (5 lecturas)
int indexBuffer = 0; // Índice del buffer para el promedio móvil
int totalLecturas = 0; // Total de lecturas acumuladas
unsigned long intervalo = 1000; // Intervalo en milisegundos para actualizar la gráfica (ajústalo según necesites)
unsigned long ultimaActualizacion = 0;
int temp_obj = 0; //Variable de temperatura objetivo. Seteable
int HS_obj = 0; //Horas objetivo. Seteable
int MIN_obj = 0; //Minuntos sobjetivo. Seteable
int HS_total = 0; //Mostrar en pantalla
int MIN_total = 0; //Mostrar pantalla
int tiempo_total; //Contempla el HS_obj+MIN_obj en segundos
int tiempo_obj = 0; //Variable de tiempo objetivo
int menu = 0;//Flag menu principal
int menu2 = 0;//Flag para alternar entre temp_obj y tiempo_obj
int menu3 = 0;//Flag para alternar entre control manual de resistencia y ventiladores
int prog = 0; //Flag para programa
String valor; //Estado de resistencia
String valor2; //Estado de fan
const int menu_ok = 4; //Botón para entrar a menu o volver
const int menu_up = 5; // Botón para aumentar valores
const int menu_down = 2; //Botón para decrecer valores
const int menu_config = 17; //Botón para alternar entre temp y tiempo
const int fan = 19; //Salida para encender ventiladores
const int resistencia = 18; //Salida para encender resistencia
//Variables del temporizador
unsigned long tiempo_restante;
unsigned long tiempo_transcurrido;
unsigned long tiempoInicio;
bool temporizadorActivo = false;
bool confirmTemporizador = false;

//Configuración wifi
const char* ssid = "TeleCentro-0923";//Red del taller
const char* password = "Motorola123";

// Web server en el puerto 80
WebServer server(80);

// Pagina web
String getWebPage() {
  dht.setup(pinDHT, DHTesp::DHT22);
  TempAndHumidity data = dht.getTempAndHumidity();
  String html = "<html><head><meta http-equiv='refresh' content='5'/>"
                "<style>table {font-family: Arial, sans-serif; border-collapse: collapse; width: 100%;}"
                "td, th {border: 1px solid #dddddd; text-align: left; padding: 8px;}"
                "tr:nth-child(even) {background-color: #dddddd;}</style></head><body>"
                "<h2>Estado del Sistema</h2><table><tr><th>Mediciones</th><th>Seteos</th></tr>";
  html += "<tr><td>Temperatura actual: " + String(data.temperature, 1) + " C</td>";
  html += "<td>Temperatura objetivo: " + String(temp_obj) + " C</td></tr>";
  html += "<tr><td>Humedad actual: " + String(data.humidity, 1) + "%</td>";
  html += "<td>Temporizador: " + String(HS_total) + " hs " + String(MIN_total) + " min</td></tr>";
  html += "</table><br><form action='/detener' method='POST'><button type='submit'>Detener Secado</button></form>";
  html += "</body></html>";
  return html;
}
// Función para manejar las peticiones de la página principal
void handleRoot() {
  server.send(200, "text/html", getWebPage());
}
// Función para manejar la acción de detener el secado
void handleDetener() {
  //secadoActivo = false;  // Detiene el secado
  digitalWrite(18, LOW); // Apaga la resistencia
  digitalWrite(19, LOW); // Apaga el ventilador
  server.send(200, "text/html", "<html><body><h2>Secado detenido</h2><a href='/'>Volver</a></body></html>");
}

void setup() { 
  WiFi.begin(ssid, password);
  Serial.begin(115200);
  Serial.println(WiFi.localIP());
  // Inicia el servidor
  server.on("/", handleRoot);
  server.on("/detener", HTTP_POST, handleDetener);
  server.begin();
  pinMode(5, INPUT_PULLDOWN);
  pinMode(4, INPUT_PULLDOWN);
  pinMode(2, INPUT_PULLDOWN);
  pinMode(17, INPUT_PULLDOWN);
  pinMode(18, OUTPUT);
  pinMode(19, OUTPUT);
  temp_obj=0;
  Wire.begin();	
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  //Inicializamos el dht
  dht.setup(pinDHT, DHTesp::DHT22);
 // Inicialización del buffer x[] con valores fuera del rango de la pantalla
  for(int i=0; i<128; i++){
    x[i] = 62;  // Inicializa con un valor correspondiente a la línea base (0 °C)
  }
  // Inicializa el buffer del promedio móvil
  for (int i = 0; i < 5; i++) {
    bufferTemperatura[i] = 0;
  }
} 

void loop() {
  if(digitalRead(resistencia)==HIGH){ //Lectura del estado de resistencia
    valor="Enc.";
  }else{
    valor="Apag.";
  }
  if(digitalRead(fan)==HIGH){
    valor2="Enc.";
  }else{
    valor2="Apag.";
  }
  //Menu
  if(digitalRead(menu_ok) == HIGH){// Cambia el estado del menú
    menu++;
    menu2=0;
    menu3=0;
    delay(200); 
    if(menu==4){
      menu=0;
    }
  }
  if(menu==0){ //PANTALLA PRINCIPAL CON GRAFICO
    display.clearDisplay();
    display.setTextSize(1);           
    display.setTextColor(SSD1306_WHITE);      
    TempAndHumidity data = dht.getTempAndHumidity();
    unsigned long actualMillis = millis(); // Tiempo actual en milisegundos
    // Solo actualiza la gráfica si ha pasado el intervalo deseado
    if (actualMillis - ultimaActualizacion >= intervalo) {
      ultimaActualizacion = actualMillis; // Actualiza el tiempo de la última actualización
      display.clearDisplay(); // Limpia el buffer del display
      // Dibuja la escala de temperatura
      display.setCursor(0, 0); 
      display.print(F("80c")); 
      display.setCursor(0, 11);   
      display.print(F("60c"));
      display.setCursor(0, 24); 
      display.print(F("40c"));
      display.setCursor(0, 37);  
      display.print(F("20c"));      
      //Dibuja lineas para los numeros de arriba
      display.drawLine(20, 0, 25, 0, WHITE);
      display.drawLine(20, 14, 25, 14, WHITE);
      display.drawLine(20, 27, 25, 27, WHITE);
      display.drawLine(20, 40, 25, 40, WHITE);
      // Dibuja eje X y Y 
      display.drawLine(0, 62, 90, 62, WHITE);
      display.drawLine(25, 62, 25, 0, WHITE);
      display.drawLine(90, 62, 90, 0, WHITE);
      // Lee la temperatura del sensor DHT22
      temperatura = data.temperature; 
      // Actualiza el buffer de promedio móvil
      bufferTemperatura[indexBuffer] = temperatura;
      indexBuffer = (indexBuffer + 1) % 5;
      if (totalLecturas < 5) {
        totalLecturas++;
      }
      // Calcula el promedio móvil
      float suma = 0;
      for (int i = 0; i < totalLecturas; i++) {
        suma += bufferTemperatura[i];
      }
      float temperaturaPromedio = suma / totalLecturas;
      // Escala la temperatura a la posición en la pantalla (0 a 80 °C)
      graficaTemperatura = map(temperaturaPromedio, 0, 80, 62, 0); 
      // Desplaza el buffer x[] para la nueva lectura
      for (int i = 0; i < 90; i++) {
        x[i] = x[i+1];
      }
      x[90] = graficaTemperatura; // Asigna el valor escalado al último dato de la matriz
      // Dibuja líneas para una gráfica más fluida
      for(int i = 89; i >= 25; i--){ 
        display.drawLine(i+1, x[i+1], i, x[i], WHITE); // Conecta puntos adyacentes con una línea
      }
      // Imprime la temperatura en texto
      display.setTextSize(1.8);  
      display.setCursor(98, 0); 
      display.print("T:"+String(data.temperature,0)+"c");
      display.setCursor(98,57);
      display.print("H:"+String(data.humidity,0)+"%");
      display.setCursor(98,8);
      display.print(String(temp_obj)+"c");
      display.setCursor(98,20);
      display.print(String(HS_total)+":"+String(MIN_total));
      display.display();
    }
    //Programa secado
    if(digitalRead(menu_config)==HIGH){
      prog++;
      delay(200);
      if(prog==2){
        prog=0;
        temporizadorActivo = false;  // Detener temporizador 
        confirmTemporizador = false;  // Resetear confirmación de temporizador
        digitalWrite(resistencia, LOW);
        digitalWrite(fan, LOW);
        }
    }
    if(prog==0){
        tiempoInicio = 0;  // Reiniciar el temporizador 
        tiempo_transcurrido = 0;
        tiempo_restante = 0;
        temporizadorActivo = false;
        confirmTemporizador = false;  // Resetear la confirmación del temporizador

    }
    if(prog==1){
      if(temp_obj==0){
        display.clearDisplay();
        display.setCursor(8,20);
        display.setTextSize(1);
        display.print("TEMPERATURA OBJETIVO       NO SETEADA");
        display.display();
        prog=0;
        delay(2000);
        }else{
          digitalWrite(fan, HIGH);
          if(data.temperature <= temp_obj){
            digitalWrite(resistencia, HIGH);
          }else{
            digitalWrite(resistencia, LOW);
          }
          if (!temporizadorActivo && !confirmTemporizador && data.temperature >= temp_obj) {
            tiempoInicio = millis();  // Inicia el temporizador
            confirmTemporizador = true;  // Confirma que el temporizador ya inició
            temporizadorActivo = true;  // Activa el temporizador
          }
          if (temporizadorActivo) {
            tiempo_total = (HS_obj * 3600 + MIN_obj * 60) * 1000;  // Tiempo total en milisegundos
            tiempo_transcurrido = millis() - tiempoInicio;
            tiempo_restante = tiempo_total - tiempo_transcurrido;

            HS_total = tiempo_restante / (3600 * 1000);  // Horas
            MIN_total = (tiempo_restante % (3600 * 1000)) / (60 * 1000);  // Minutos
            if (HS_total==0 && MIN_total==0){
              digitalWrite(resistencia, LOW);
              digitalWrite(fan, LOW);
              prog=0;
              temporizadorActivo = false;  // Detener temporizador 
              confirmTemporizador = false;  // Resetear confirmación de temporizador
              display.clearDisplay();
              display.setCursor(15,25);
              display.setTextSize(1);
              display.print("SECADO COMPLETADO");
              display.display(); //Se despliega el aviso de que termino
              delay(2000);
              if(digitalRead(menu_config)==HIGH){ //Presionar para volver a pantalla principal
                menu=0;
              }
            }
          }
        }
      } 
  }
  if(menu==1){ //MENU SETEO TEMPERATURA Y TIEMPO
    if (digitalRead(menu_config)==HIGH){
      menu2++;
      delay(200);
      display.clearDisplay();
      if(menu2==2){
        menu2=0;
      }
    }
    if(menu2==0){
      display.setCursor(5,14);
      display.setTextSize(2);
      display.print(">");
      if(digitalRead(menu_up)==HIGH && temp_obj!=70){
        temp_obj += 5;
        delay(200);
      }
      if(digitalRead(menu_down)==HIGH && temp_obj!=0){
        temp_obj -= 5;
        delay(200);
      }
    }
    if(menu2==1){
      display.setCursor(5,44);
      display.setTextSize(2);
      display.print(">");
      if(digitalRead(menu_up)==HIGH){
        MIN_obj += 30;
        delay(200);
      }
      if(digitalRead(menu_down)==HIGH){
        MIN_obj -= 30;
        delay(200);
      }
      if(MIN_obj >= 60) {
      HS_obj += 1;
      MIN_obj = 0;
      }
      if(MIN_obj < 0) {
      HS_obj -= 1;
      MIN_obj = 30;
      }
    }
    display.setTextSize(1);
    display.setCursor(5,2);
    display.drawRect(0, 0, 127, 63, WHITE);
    display.fillRect(0, 0, 127, 11, WHITE);
    display.fillRect(0, 30, 127, 11, WHITE);
    display.setTextColor(BLACK, WHITE);
    display.print("TEMPERATURA OBJETIVO");
    display.setCursor(30,32);
    display.setTextColor(BLACK, WHITE);
    display.print("TIEMPO SECADO");
    display.setTextColor(WHITE, BLACK);
    display.setCursor(50,14);
    display.setTextSize(2);
    display.print(String(temp_obj));
    display.setCursor(80,14);
    display.print("c");
    display.setCursor(30,44);
    display.print(String(HS_obj)+":"+String(MIN_obj));
    display.setCursor(80,44);
    display.print("hs");
    display.display();
  }
  if(menu==2){//MENU ENC. Y APAG. VENTILADORES Y RESISTENCIA
    display.clearDisplay();
    if (digitalRead(menu_config)==HIGH){
      menu3++;
      delay(200);
      display.clearDisplay();
      if(menu3==2){
        menu3=0;
      }
    }
    if(menu3==0){
      display.setCursor(5,14);
      display.setTextSize(2);
      display.print(">");
      if(digitalRead(menu_up)==HIGH){
        valor="Enc.";
        digitalWrite(resistencia, HIGH);
        delay(200);
      }
      if(digitalRead(menu_down)==HIGH){
        valor="Apag.";
        digitalWrite(resistencia,LOW);
        delay(200);
      }
    }
    if(menu3==1){
      display.setCursor(5,44);
      display.setTextSize(2);
      display.print(">");
      if(digitalRead(menu_up)==HIGH){
        valor2="Enc.";
        digitalWrite(fan, HIGH);
        delay(200);
      }
      if(digitalRead(menu_down)==HIGH){
        valor2="Apag.";
        digitalWrite(fan, LOW);
        delay(200);
      }
    }
    display.setTextSize(1);
    display.setCursor(32,2);
    display.drawRect(0, 0, 127, 63, WHITE);
    display.fillRect(0, 0, 127, 11, WHITE);
    display.fillRect(0, 30, 127, 11, WHITE);
    display.setTextColor(BLACK, WHITE);
    display.print("RESISTENCIA");
    display.setCursor(30,32);
    display.setTextColor(BLACK, WHITE);
    display.print("VENTILADORES");
    display.setTextColor(WHITE, BLACK);
    display.setCursor(50,14);
    display.setTextSize(2);
    display.print(valor);
    display.setCursor(50,44);
    display.print(valor2);
    display.display();
  }
  if(menu==3){
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(32,2);
    display.drawRect(0, 0, 127, 63, WHITE);
    display.fillRect(0, 0, 127, 11, WHITE);
    display.fillRect(0, 30, 127, 11, WHITE);
    display.setTextColor(BLACK, WHITE);
    display.print("DIRECCION IP");
    display.setCursor(47,32);
    display.setTextColor(BLACK, WHITE);
    display.print("ESTADO");
    display.setTextColor(WHITE, BLACK);
    display.setCursor(25,17);
    display.setTextSize(1);
    display.print(WiFi.localIP());
    display.setCursor(40,48);
    if(WiFi.status() != WL_CONNECTED){
      display.print("DESCONECTADO");
    }else{
      display.print("CONECTADO");
    }
    display.display();
  }
  // Atender peticiones web
  server.handleClient();
}
