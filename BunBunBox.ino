
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WiFi.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <EasyNTPClient.h>
#include <WiFiUdp.h>
extern "C" {
#include <ctime>
}

String getTimeStamp(unsigned long unixTime, const char* format);
String getTime();
void handleFileUpload();
bool loadFromSpiffs(String path);
void handleWebRequests();
void handleRoot();
void initWebserver();
void initWifi();
void btnled(int StateOfButton, bool *StateOfLED, int LED);
void loop();
void setup();


int StateOfButton5 = 0;
int StateOfButton6 = 0;
int StateOfButton7 = 0;
bool StateOfLED1 = false;
bool StateOfLED2 = false;
bool StateOfLED3 = false;

const char* ssid = "someid";
const char* password = "somepw";
const char* update_path = "/firmware";
const char* update_username = "anotherid";
const char* update_password = "anotherpw";
const String dataFolder = "/some/";
static unsigned long Timestamp;
File fsUploadFile;

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  SPIFFS.begin();
  initWifi();
  initWebserver();

  pinMode(D1, OUTPUT);
  pinMode(D2, OUTPUT);
  pinMode(D3, OUTPUT);

  digitalWrite(D1, LOW);
  digitalWrite(D2, LOW);
  digitalWrite(D3, LOW);

  pinMode(D5, INPUT_PULLUP);
  pinMode(D6, INPUT_PULLUP);
  pinMode(D7, INPUT_PULLUP);

}


void loop() {
  server.handleClient();
  StateOfButton5 = digitalRead(D5);
  StateOfButton6 = digitalRead(D6);
  StateOfButton7 = digitalRead(D7);

  btnled(StateOfButton5, &StateOfLED1, D1);
  btnled(StateOfButton6, &StateOfLED2, D2);
  btnled(StateOfButton7, &StateOfLED3, D3);

}

//turn LED on when Button is pressed
//open txt-file and write string with timestamp and LED 
//when pushing button again LED turns off
void btnled(int StateOfButton, bool *StateOfLED, int LED) {

  if (StateOfButton == LOW && *StateOfLED == false) {
    *StateOfLED = true;
    digitalWrite(LED, HIGH);
    File f = SPIFFS.open(dataFolder + "some.txt", "a");
    if (f) {
      f.println(getTime()  + " " + String(LED));
      f.close();
    }else{
      Serial.println(F("Error writing data"));
    }
    delay(500);

  } else if (StateOfButton == LOW && *StateOfLED == true) {
    *StateOfLED = false;
    digitalWrite(LED, LOW);
    delay(500);
  }

}

void initWifi() {
  
  WiFi.begin(ssid, password);     //Connect to your WiFi router
  WiFi.mode(WIFI_STA);
  Serial.println("");
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }
  //If connection successful show IP address in serial monitor
  Serial.println("");
  Serial.print(F("Connected to "));
  Serial.println(ssid);
  Serial.print(F("IP address: "));
  Serial.println(WiFi.localIP());  //IP address assigned to your ESP
  WiFiUDP udp;
  EasyNTPClient ntpClient(udp, "de.pool.ntp.org", ((1 * 60 * 60))); // IST = GMT + 2 Summertime GMT + 1 Wintertime
  unsigned long tmptime = 0;
  while (tmptime == 0 || tmptime >= 4294960000) {
    tmptime = ntpClient.getUnixTime() - (millis() / 1000);
    Timestamp = tmptime;
    Serial.println("Unixtime: " + String(tmptime));
    delay(500);
  }
}

void initWebserver() {
  //Initialize Webserver
  server.on("/", handleRoot);
  server.onNotFound(handleWebRequests); //Set setver all paths are not found so we can handle as per URI
  server.on("/upload", HTTP_GET, []() {                 // if the client requests the upload page
    server.send(200, "text/html", "<form method=\"post\" enctype=\"multipart/form-data\"><input type=\"file\" name=\"name\"><input class=\"button\" type=\"submit\" value=\"Upload\"></form>");
  });

  server.on("/upload", HTTP_POST,                       // if the client posts to the upload page
    []() { server.send(200); },                          // Send status 200 (OK) to tell the client we are ready to receive
    handleFileUpload                                    // Receive and save the file
  );
  httpUpdater.setup(&server, update_path, update_username, update_password);
  server.begin();
}

void handleRoot() {
  server.sendHeader("Location", "/index.html", true);  //Redirect to our html web page
  server.send(302, "text/plane", "");
}
void handleWebRequests() {
  if (loadFromSpiffs(server.uri())) return;
  server.send(404, "text/plain", "404 Not Found");
}

bool loadFromSpiffs(String path) {
  if (!SPIFFS.exists(path.c_str())) {
    return false;
  }
  String dataType = "text/plain";
  if (path.endsWith("/")) path += "index.html";
  if (path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
  else if (path.endsWith(".html")) dataType = "text/html";
  else if (path.endsWith(".htm")) dataType = "text/html";
  else if (path.endsWith(".css")) dataType = "text/css";
  else if (path.endsWith(".js")) dataType = "application/javascript";
  else if (path.endsWith(".png")) dataType = "image/png";
  else if (path.endsWith(".gif")) dataType = "image/gif";
  else if (path.endsWith(".jpg")) dataType = "image/jpeg";
  else if (path.endsWith(".ico")) dataType = "image/x-icon";
  else if (path.endsWith(".xml")) dataType = "text/xml";
  else if (path.endsWith(".pdf")) dataType = "application/pdf";
  else if (path.endsWith(".zip")) dataType = "application/zip";
  else if (path.endsWith(".json")) dataType = "application/json";
  File dataFile = SPIFFS.open(path.c_str(), "r");
  if (server.hasArg("download")) dataType = "application/octet-stream";
  if (server.streamFile(dataFile, dataType) != dataFile.size()) {
  }
  dataFile.close();
  return true;
}

void handleFileUpload() {
  // upload a new file to the SPIFFS
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;
    Serial.print("handleFileUpload Name: "); Serial.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
    filename = String();
  }
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
  }
  else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {                                    // If the file was successfully created
      fsUploadFile.close();                               // Close the file again
      Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
      server.send(200, "text/html", "<html><body><h1>Upload Success</h1></body></html>");
    }
    else {
      server.send(500, "text/html", "<html><body><h1>Couldn't create file</h1></body></html>");
    }
  }
}

String getTime() {
  return getTimeStamp(Timestamp + (millis() / 1000), "%Y-%m-%d %H:%M:%S");
}

String getTimeStamp(unsigned long unixTime, const char* format)
{
  time_t epochTime = (time_t)(unixTime);
  char timestamp[64] = { 0 };
  strftime(timestamp, sizeof(timestamp), format, localtime(&epochTime));
  return timestamp;
}
