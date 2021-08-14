#include <ESPmDNS.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <time.h>
#include <sstream>
#include <iomanip>

#include <HTTPClient.h>
HTTPClient http;

time_t last_request;
time_t next_alarm;
time_t time_right_now;
time_t timer;

//time step of 6 seconds yiels 39.2 minutes sunrise duration
uint8_t time_constant = 6;

// Replace with your network credentials and device IP
const char* ssid = "SSID";
const char* password = "PASSWORD";
const char* bulb_ip = "192.168.15.35";

long next_wifi_reconnect = 900;
long since_wifi_reconnect = 0;
uint16_t curve_counter = 0;
char payload_buffer[500];

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Pick an ntp server of your choice
const char* ntpServer = "south-america.pool.ntp.org";

// Adjust for time zone offset of -10800 seconds corresponding to UTC-3
const long Offset_sec = -10800; 

String time_t2datetime(time_t trn_t)
{
  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&trn_t));
  return buffer;
}

String today(time_t trn_t)
{
  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d", localtime(&trn_t));
  return buffer;
}

char* payload(){
  strftime(payload_buffer, sizeof(payload_buffer), "{\"Timerstring\":\"You will wake up in %H hours and %M minutes. It is time to sleep!\"}", gmtime(&timer));
  return payload_buffer;
}

time_t set_next_alarm(String wakeuptime){
  time(&time_right_now);
  String ifitweretoday = today(time_right_now) + " " + wakeuptime + ":00";
  struct tm tm_if;
  strptime(ifitweretoday.c_str(), "%Y-%m-%d %H:%M:%S", &tm_if);
  time_t new_t = mktime(&tm_if);
  if(new_t > time_right_now){ 
    return new_t;
  }
  return new_t+86400;
}

void setup()
{
  Serial.begin(115200); 
  Serial.println("Sunrise Wake-Up Light");  

  if(!SPIFFS.begin()){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  if(!MDNS.begin("sunrise")) {
     Serial.println("Error starting mDNS");
     return;
  }

  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html");
  });

  // Updates the heads-up message
  server.on("/timerstring", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", payload());
  });

  // Sets the wake up time and saves it to a file in flash memory
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request){
    String inputMessage;
    // GET wkp value on <ESP_IP>/get?wkp=<inputMessage>
    if(request->hasParam("wkp")){
      inputMessage = request->getParam("wkp")->value();
      writeFile(SPIFFS, "/WakeUpTime.txt", inputMessage.c_str());
      next_alarm = set_next_alarm(inputMessage.c_str());
    }
    Serial.println(inputMessage);
  });

  // Writes the internal RTC of the ESP32 
  configTime(Offset_sec, 0, ntpServer);
  delay(1000);
  time(&last_request);
  delay(1000);

  // Reads the wake up time from flash memory, if set
  String wakeuptime = readFile(SPIFFS, "/WakeUpTime.txt");
  if(strlen(wakeuptime.c_str()) > 4){
    next_alarm = set_next_alarm(wakeuptime);
  }
  else{
    next_alarm = set_next_alarm("06:30");
  }
  Serial.println("Time of internal RTC set to \"" + time_t2datetime(last_request) + "\".");
  
  // Start server
  server.begin();
}

void loop()
{
  // Check if the network connection is active once in a while
  if(since_wifi_reconnect > next_wifi_reconnect){
    since_wifi_reconnect = 0;
    if(WiFi.status() != WL_CONNECTED){
      Serial.println("Reconnecting to WiFi...");
      WiFi.disconnect();
      WiFi.begin(ssid,password);
    }
  }
  since_wifi_reconnect++;
  
  time(&time_right_now);
  timer = difftime(next_alarm,time_right_now);

  //If the wake up time is reached, we gradually dim up the LEDs and issue an HTTP request to the bulb IP
  if(time_right_now+392*time_constant > next_alarm && time_right_now >= last_request + time_constant && curve_counter < 392)
  { 
    float dimmer = 1.;
    float temperature = 2500.;
    int cct = 0;
    if(curve_counter < 256){
      dimmer = curve_counter/255.;
      temperature = 1000. + curve_counter/255.*1500.;
    }
    if(curve_counter > 200 && curve_counter < 392){
      cct = 63 + (curve_counter-200);
    }
    Serial.println(time_t2datetime(time_right_now));
    std::stringstream ss;
    ss << std::uppercase << std::setw(2) << std::setfill('0') << std::hex << (int) (dimmer*255);
    ss << std::uppercase << std::setw(2) << std::setfill('0') << std::hex << (int) (dimmer*green_val(temperature));
    ss << std::uppercase << std::setw(2) << std::setfill('0') << std::hex << (int) (dimmer*blue_val(temperature));
    ss << std::uppercase << std::setw(2) << std::setfill('0') << std::hex << (int) cct;
    std::string parameters = ss.str();
    char* url;
    const char* prefix = "http://";
    const char* command = "/cm?cmnd=Color%20";
    url = (char*)malloc(strlen(prefix)+strlen(bulb_ip)+strlen(command)+sizeof(parameters));
    strcpy(url, prefix);
    strcat(url, bulb_ip);
    strcat(url, command);
    strcat(url, parameters.c_str());
    Serial.println(url);
    http.begin(url);
    http.GET();
    http.end();
    time(&last_request);
    curve_counter++;

    // When the maximum light intensity is reached, the wake up time is increased by 24h.
    if(curve_counter == 392){
      curve_counter = 0;
      next_alarm += 86400;
    }
  }
}

float green_val(float temperature){
  float temp = -35.3271+0.119667*temperature-1.94e-05*pow(temperature,2)+1.19547e-09*pow(temperature,3);
  if(temp < 0){
    return 0;
  }
  else if(temp > 255){
    return 255;
  }
  else{
    return temp;
  }
}

float blue_val(float temperature){
  float temp = -248.693+0.181044*temperature-2.50579e-05*pow(temperature,2)+1.40384e-09*pow(temperature,3);
  if(temp < 0){
    return 0;
  }
  else if(temp > 255){
    return 255;
  }
  else{
    return temp;
  }
}

String readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path, "r");
  if(!file || file.isDirectory()){
    Serial.println("- empty file or failed to open file");
    return String();
  }
  Serial.println("- read from file:");
  String fileContent;
  while(file.available()){
    fileContent+=String((char)file.read());
  }
  Serial.println(fileContent);
  return fileContent;
}

void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\r\n", path);
  File file = fs.open(path, "w");
  if(!file){
    Serial.println("- failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("- file written");
  } else {
    Serial.println("- write failed");
  }
}
