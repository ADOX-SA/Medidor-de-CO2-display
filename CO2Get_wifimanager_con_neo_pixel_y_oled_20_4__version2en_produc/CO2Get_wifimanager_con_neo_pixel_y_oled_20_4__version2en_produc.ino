#include <ESPEssentials.h>
#include <OTA.h>
#include <WebServer.h>
#include <Wifi.h>

String firmVer = "5.0";

String webpage = "";

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include "MHZ19.h"
#include <SoftwareSerial.h>

//AGREGADO OLED Y NEOPIXEL
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>

#include <Ticker.h>
#include <RTClib.h>

Ticker timer_1ms;
RTC_DS3231 rtc;

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library.
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET LED_BUILTIN  //4 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C     ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Which pin on the ESP8266 is connected to the NeoPixels?
#define PIN 0

// How many NeoPixels are attached to the ESP8266?
#define NUMPIXELS 10

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

int delayval = 500;  // delay for half a second
int cont_color_verde = 200;
int flag = 1;

int cont_amarillo = 0;
int cont_display = 0;
int pantalla = 0;
int estado_CO2 = 0;

//FIN AGREGADO OLED Y NEOPIXEL

#define RX_PIN 13      // D7 - Rx pin which the MHZ19 Tx pin is attached to
#define TX_PIN 15      // D8 - Tx pin which the MHZ19 Rx pin is attached to
#define BAUDRATE 9600  // Device to MH-Z19 Serial baudrate (should not be changed)

#define LED_D6 12

const String SensorID = String(ESP.getChipId(), HEX);

HTTPClient http_post;
WiFiClient client_post;
HTTPClient http_get;
WiFiClient client_get;


const int RSSI_MAX = -50;   // define maximum strength of signal in dBm
const int RSSI_MIN = -100;  // define minimum strength of signal in dBm

uint32_t PAUSA = 6;  // 5 min o 6 seg por ahora;


MHZ19 myMHZ19;                            // Constructor for library
SoftwareSerial mySerial(RX_PIN, TX_PIN);  // (Uno example) create device to MH-Z19 serial


int CO2;


ADC_MODE(ADC_VCC);  //este modo sirve para habilitar el divisor interno y
//y poder medir correctamente el BUS de 3.3v

void connect();

long connect_tic = 0;  // interval at which to blink (milliseconds)
long OLED_tic = 0;
long LEDS_tic = 0;
long CO2_tic = 0;
long guardar_datos_tic = 0;

#define MAX_BYTES 1500000


void Timer_1ms() {
  if (connect_tic > 0) connect_tic--;
  if (OLED_tic > 0) OLED_tic--;
  if (LEDS_tic > 0) LEDS_tic--;
  if (CO2_tic > 0) CO2_tic--;
  if (guardar_datos_tic > 0) guardar_datos_tic--;
}

void setup() {

  //AGREGADO OLED Y NEOPIXEL
  Serial.begin(115200);

  pinMode(LED_D6, OUTPUT);


  // put your setup code here, to run once:
  pixels.begin();  // This initializes the NeoPixel library.
  pixels.show();
  pixels.setBrightness(150);

  pixels.fill(pixels.Color(0, 0, 0), 0, NUMPIXELS);
  pixels.show();

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();

  display.clearDisplay();

  // Clear the buffer
  display.clearDisplay();

  // Draw a single pixel in white
  //display.drawPixel(10, 10, SSD1306_WHITE);

  // Show the display buffer on the screen. You MUST call display() after
  // drawing commands to make them visible on screen!
  display.display();
  //delay(2000);
  // display.display() is NOT necessary after every single drawing command,
  // unless that's what you want...rather, you can batch up a bunch of
  // drawing operations and then update the screen all at once by calling
  // display.display(). These examples demonstrate both approaches...

  testdrawstyles();  // Draw 'stylized' characters


  //FIN AGREGADO OLED Y NEOPIXEL

  //initESPEssentials("Sensor_" + SensorID);
  WiFi.softAP(Wifi.getDefaultAPName());
  WebServer.init();
  OTA.init();
  

  WebServer.on("/reset_wifi", HTTP_GET, [&]() {
    WebServer.send(200, "text/plain", "Wifi settings reset.");
    Wifi.resetSettings();
  });

  WebServer.on("/borrar_memoria", HTTP_GET, [&]() {
    if (SPIFFS.format()) WebServer.send(200, "text/plain", "Memoria borrada OK");
    else WebServer.send(200, "text/plain", "Error al borrar Memoria");
  });

  WebServer.on("/download", File_Download);
  WebServer.on("/config", Config_Page);


  delay(2000);


  display.clearDisplay();
  display.setTextSize(2);  // Draw 2X-scale text

  display.setCursor(0, 0);  // Start at top-left corner
  display.println(F("SensorID:"));
 
  display.setCursor(0, 16);  // Start at top-left corner
  display.println(SensorID);

  display.display();
  delay(2000);

  display.setTextSize(1);  // Draw 2X-scale text

  display.setCursor(0, 32);  // Start at top-left corner
  display.print(F("IP: "));
  display.println(WiFi.localIP().toString());

  display.setCursor(0, 40);  // Start at top-left corner
  display.println(F("SSID: "));

  display.setCursor(0, 50);  // Start at top-left corner
  display.println(WiFi.SSID());

  display.display();
  delay(3000);

  mySerial.begin(BAUDRATE);  // (Uno example) device to MH-Z19 serial start
  myMHZ19.begin(mySerial);   // *Serial(Stream) refence must be passed to library begin().

  myMHZ19.autoCalibration(true);  // Turn auto calibration ON (OFF autoCalibration(false))

  ////////////int a = connect();
  // connect();

  timer_1ms.attach_ms(1, Timer_1ms);

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
  }

  if (!SPIFFS.begin())
  {
    Serial.println("[Storage] Couldn't mount file system.");
  }


}

void loop() {
  WebServer.handleClient();
  OTA.handle();

  pantalla = 1;

  if (!CO2_tic) {
    CO2_tic = 2000;
    CO2 = myMHZ19.getCO2();  // Request CO2 (as ppm)
  }

  if (!OLED_tic) {
    OLED_tic = 2000;
    testdrawstyles();
  }

  if (!LEDS_tic) {
    LEDS_tic = 500;

    if (CO2 == 0) {
      cont_amarillo = 0;
      estado_CO2 = 0;

      pixels.fill(pixels.Color(0, 0, 255), 0, NUMPIXELS);
      pixels.show();
    } else if (CO2 < 700) {
      cont_amarillo = 0;
      estado_CO2 = 1;

      pixels.fill(pixels.Color(0, 255, 0), 0, NUMPIXELS);
      pixels.show();
    } else if (CO2 < 1400) {
      estado_CO2 = 2;

      pixels.fill(pixels.Color(255, 100, 0), 0, NUMPIXELS);
      pixels.show();

      cont_amarillo++;
      if (cont_amarillo > 1000) {
        cont_amarillo = 0;
        digitalWrite(LED_D6, HIGH);  // sonalert encendido
        delay(100);
        digitalWrite(LED_D6, LOW);  // sonalert apagado
      }
    } else {
      estado_CO2 = 3;

      pixels.fill(pixels.Color(0, 0, 0), 0, NUMPIXELS);
      pixels.show();

      digitalWrite(LED_D6, HIGH);  // sonalert encendido
      delay(100);
      digitalWrite(LED_D6, LOW);  // sonalert apagado

      pixels.fill(pixels.Color(255, 0, 0), 0, NUMPIXELS);
      pixels.show();

      cont_amarillo = 0;
    }
  }
  //FIN AGREGADO OLED Y NEOPIXEL

  if (!guardar_datos_tic) {
    guardar_datos_tic = 60000;

    FSInfo fsInfo;
    SPIFFS.info(fsInfo);
    Serial.print("FS Total Bytes: ");
    Serial.println(fsInfo.totalBytes);
    Serial.print("FS Used Bytes: ");
    Serial.println(fsInfo.usedBytes);
          
    DateTime now = rtc.now();
    Serial.println(now.timestamp(DateTime::TIMESTAMP_FULL));

    if (fsInfo.usedBytes < MAX_BYTES) {
      File file = SPIFFS.open("/Log_"+ now.timestamp(DateTime::TIMESTAMP_DATE) + "_" + SensorID + ".csv", "a");
  
      if (file) {
        file.print(now.timestamp(DateTime::TIMESTAMP_FULL));
        file.print(',');
  
        if (CO2 == 0)
          file.println("---");
        else if (CO2 < 400)
          file.println("408");
        else
          file.println(CO2);
        
        Serial.println(file.size());
        file.close();      
      }
      else {
        Serial.println("Fallo al abrir archivo");
      }
    }
    else Serial.println("Memoria llena");      
  }

  if ((WiFi.status() == WL_CONNECTED) && !connect_tic) {
    connect_tic = 30000;
    connect();
  }
}


void connect() {
  int8_t Temp;

  Serial.print("SensorID:\n");
  Serial.print(SensorID + "\n");

  Serial.print("CO2 (ppm): ");
  Serial.println(CO2);
  
  Temp = myMHZ19.getTemperature();  // Request Temperature (as Celsius)
  Serial.print("Temperature (C): ");
  Serial.println(Temp);

  long rssi = WiFi.RSSI();
  Serial.print("RSSI:");
  Serial.println(rssi);

  int quality = dBmtoPercentage(rssi);
  Serial.println(quality);

  /*****************************************Envio al sistema nuevo por metodo POST****************************************/
  
  String url_post = "http://data.ambientecontrolado.com.ar/device/measure";
  String data_post = "{\"data\":{\"extraData\":{\"chipId\":\"" + SensorID + "\",\"co2\":"+ String(CO2) + ",\"ssid\":\""+ WiFi.SSID() + "\",\"ip\":\""+ WiFi.localIP().toString() +"\",\"signal\":" + String(quality) + ",\"firmwareVersion\":\"" +  firmVer + "\"}}}";

  Serial.println("URL: " + url_post);
  Serial.println("DATA: " + data_post);

  Serial.print("[HTTP] begin...\n");
  // configure traged server and url
  http_post.begin(client_post, url_post); //HTTP
  http_post.addHeader("Content-Type", "application/json");

  Serial.print("[HTTP] POST...\n");
  // start connection and send HTTP header and body
  int httpCode_post = http_post.POST(data_post);

  // httpCode will be negative on error
  if (httpCode_post > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] POST... code: %d\n", httpCode_post);

    // file found at server
    if (httpCode_post == HTTP_CODE_OK) {
      const String& payload = http_post.getString();
      Serial.println("received payload:\n<<");
      Serial.println(payload);
      Serial.println(">>");
    }
  }
  else {
      Serial.printf("[HTTP] POST... failed, error: %s\n", http_post.errorToString(httpCode_post).c_str());
  }

  http_post.end();
  
  /********************************Envio al servidor Calidaddelaireadox por metodo GET**************************************/

  Serial.print("[HTTP] begin...\n");

  String url = "http://159.203.150.67/calidaddelaireadox/services/Services.php?acc=AD&id=" + SensorID + "&co2=" + String(CO2) + "&temp=" + String(Temp) + "&bateria=" + String(ESP.getVcc()) + "&wifi=" + String(quality);

  Serial.println("URL: " + url);

  if (http_get.begin(client_get, url)) {
    // HTTP
    //http.addHeader("Content-Type", "text/plain");  //Specify content-type header

    Serial.print("[HTTP] GET...\n");
    // start connection and send HTTP header
    int httpCode_get = http_get.GET();

    // httpCode will be negative on error
    if (httpCode_get > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] GET... code: %d\n", httpCode_get);

      http_get.end();
    }
    else {
      Serial.printf("[HTTP] Unable to connect\n");
    }
  }

}

/*
   Written by Ahmad Shamshiri
    with lots of research, this sources was used:
   https://support.randomsolutions.nl/827069-Best-dBm-Values-for-Wifi
   This is approximate percentage calculation of RSSI
   Wifi Signal Strength Calculation
   Written Aug 08, 2019 at 21:45 in Ajax, Ontario, Canada
*/

int dBmtoPercentage(long dBm) {
  int quality;
  if (dBm <= RSSI_MIN) {
    quality = 0;
  } else if (dBm >= RSSI_MAX) {
    quality = 100;
  } else {
    quality = 2 * (dBm + 100);
  }

  return quality;
}  //dBmtoPercentage



void testdrawstyles(void) {
  display.clearDisplay();

  if (pantalla == 0) {
    display.setCursor(0, 55);  // Start at top-left corner

    display.setTextSize(1);  // Draw 2X-scale text
    display.setTextColor(SSD1306_WHITE);
    display.println(firmVer);

    display.setCursor(15, 0);  // Start at top-left corner

    display.setTextSize(4);  // Draw 2X-scale text
    display.setTextColor(SSD1306_WHITE);
    display.println(F("ADOX"));

    display.setCursor(0, 30);  // Start at top-left corner

    display.setTextSize(2);  // Draw 2X-scale text
    display.setTextColor(SSD1306_WHITE);
    display.println(F("MonitorCO2"));

  } else {
    if (CO2 > 999) {
      display.setTextSize(4);  // Normal 1:1 pixel scale
    } else {
      display.setTextSize(6);  // Normal 1:1 pixel scale
    }

    display.setTextColor(SSD1306_WHITE);  // Draw white text
    display.setCursor(10, 0);             // Start at top-left corner

    if (CO2 == 0) {
      display.println(F("----"));
    } else if (CO2 < 400) {
      display.println(F("408"));
    } else {
      display.println(CO2);
    }

    display.setTextSize(2);  // Draw 2X-scale text
    display.setTextColor(SSD1306_WHITE);

    if (cont_display == 0) {
      display.println(F("CO2 (ppm)"));
      cont_display = 1;
    } else if (cont_display == 1) {
      if (estado_CO2 == 0) {
        display.println(F("SIN MED"));
      } else if (estado_CO2 == 1) {
        display.println(F("Normal"));
      } else if (estado_CO2 == 2) {
        display.println(F("Alta"));
      } else {
        display.println(F("Muy Alta"));
      }
      cont_display = 2;
    } else if (cont_display == 2) {
      if (WiFi.status() == 0) {
        display.println(F("WI-FI desc"));
      } else if (WiFi.status() == 3) {
        display.println(F("WI-FI con"));
      }
      cont_display = 0;
    }
  }

  display.display();
  //  delay(2000);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void File_Download() { // This gets called twice, the first pass selects the input, the second pass then processes the command line arguments
  if (WebServer.args() > 0 ) { // Arguments were received
    if (WebServer.hasArg("download")) SD_file_download(WebServer.arg(0));
    if (WebServer.hasArg("borrar")) SD_file_borrar(WebServer.arg(0));
  }
  else SelectInput("Descarga de Archivos", "Presione \"descargar\" para bajar el archivo o \"borrar\" para borrarlo", "download", "download");
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Config_Page() { // This gets called twice, the first pass selects the input, the second pass then processes the command line arguments
  if (WebServer.args() > 0 ) { // Arguments were received
    if (WebServer.hasArg("currentDateTime")) Set_Date_Time(WebServer.arg(0));
  }
  else {    
    SendHTML_Header();
    webpage += F("<h2>Ajustar Fecha y Hora</h2>");

    DateTime now = rtc.now();
    webpage += F("<p>Fecha y Hora actual: ") + now.timestamp(DateTime::TIMESTAMP_DATE) + F("&nbsp;&nbsp;&nbsp;&nbsp;") + now.timestamp(DateTime::TIMESTAMP_TIME) + F("</p>");
    webpage += F("<p>Seleccione nueva Fecha y Hora: </p>");
    webpage += F("<FORM action='/config'>"); // Must match the calling argument e.g. '/chart' calls '/chart' after selection but with arguments!
    webpage += F("<input type='datetime-local' id='currentDateTime' name='currentDateTime'>");
    webpage += F("<input type='submit'>");
    webpage += F("</FORM></body></html>");
    SendHTML_Content();
    SendHTML_Stop();
    
  }
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SD_file_download(String filename) {
  File download = SPIFFS.open("/" + filename, "r");
  if (download) {
    WebServer.sendHeader("Content-Type", "text/text");
    WebServer.sendHeader("Content-Disposition", "attachment; filename=" + filename);
    WebServer.sendHeader("Connection", "close");
    WebServer.streamFile(download, "application/octet-stream");
    download.close();
  } else ReportFileNotPresent("download");
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SD_file_borrar(String filename) {
    SendHTML_Header();
    if (SPIFFS.remove("/" + filename))
      webpage += F("<h3>El archivo se borro con exito</h3>");
    else
      webpage += F("<h3>Error al borrar archivo</h3>");
    webpage += F("<a href='/download'>[Back]</a><br><br>");
    webpage += F("</body></html>");
    SendHTML_Content();
    SendHTML_Stop();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Set_Date_Time(const String currentDateTime) {
    rtc.adjust(DateTime(currentDateTime.c_str()));
//    rtc.adjust(DateTime("2021-11-29T13:45"));
    SendHTML_Header();
    webpage += F("<h3>Fecha y hora actulizada</h3>");
    webpage += F("<a href='/config'>[Back]</a><br><br>");
    webpage += F("</body></html>");
    SendHTML_Content();
    SendHTML_Stop();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SendHTML_Header() {
  WebServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  WebServer.sendHeader("Pragma", "no-cache");
  WebServer.sendHeader("Expires", "-1");
  WebServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  WebServer.send(200, "text/html", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
  append_page_header();
  WebServer.sendContent(webpage);
  webpage = "";
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SendHTML_Content() {
  WebServer.sendContent(webpage);
  webpage = "";
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SendHTML_Stop() {
  WebServer.sendContent("");
  WebServer.client().stop(); // Stop is needed because no content length was sent
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SelectInput(String heading1, String heading2, String command, String arg_calling_name) {
  SendHTML_Header();
  webpage += F("<h3 class='rcorners_m'>"); webpage += heading1 + "</h3>";

  webpage += F("<h3>   Nombre  &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;  Tamaño   </h3>");
  webpage += F("<FORM action='/"); webpage += command + "' method='post'>"; // Must match the calling argument e.g. '/chart' calls '/chart' after selection but with arguments!

  String filename = "";
  Dir dir = SPIFFS.openDir("/");
  long usedbytes=0;
  while (dir.next()) {
    usedbytes += dir.fileSize();
    filename = dir.fileName();
    filename.remove(0,1);
    webpage += F("<p>"); webpage += dir.fileName() + F("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;") + dir.fileSize() + F(" bytes &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;");
    webpage += F("<button type='submit' name='download' value='") + filename + F("'>descargar</button> ");
    webpage += F("<button type='submit' name='borrar' value='") + filename + F("'>borrar</button> </p>");    
  }
  
  webpage += F("<h3>"); webpage += heading2 + "</h3>";
  
  FSInfo fsInfo;
  SPIFFS.info(fsInfo);
  Serial.println(fsInfo.totalBytes);
  webpage += F("<p>Bytes usados: "); webpage += String(usedbytes) + F(" / ")+ String(fsInfo.totalBytes) + F("</p>");

  if (fsInfo.usedBytes >= MAX_BYTES)
      webpage += F("<h3 style='color:Tomato;'>Memoria llena, por favor borre archivos para seguir grabando información</h3>");

//  webpage += F("<FORM action='/"); webpage += command + "' method='post'>"; // Must match the calling argument e.g. '/chart' calls '/chart' after selection but with arguments!
//  webpage += F("<input type='text' name='"); webpage += arg_calling_name; webpage += F("' value=''><br>");
//  webpage += F("<type='submit' name='"); webpage += arg_calling_name; webpage += F("' value=''><br><br>");
  webpage += F("</FORM></body></html>");
  SendHTML_Content();
  SendHTML_Stop();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void ReportFileNotPresent(String target) {
  SendHTML_Header();
  webpage += F("<h3>El archivo no existe</h3>");
  webpage += F("<a href='/"); webpage += target + "'>[Back]</a><br><br>";
  webpage += F("</body></html>");
  SendHTML_Content();
  SendHTML_Stop();
}

void append_page_header() {
  webpage  = F("<!DOCTYPE html><html>");
  webpage += F("<head>");
  webpage += F("<title>File Server</title>"); // NOTE: 1em = 16px
  webpage += F("<meta name='viewport' content='user-scalable=yes,initial-scale=1.0,width=device-width'>");
  webpage += F("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"/>");
  webpage += F("<style>");
  webpage += F("</style></head><body>");
}
