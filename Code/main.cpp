
//////////// Libraries ////////////
#include <ESP8266WiFi.h> // Include the ESP8266 WiFi library
#include <ESP8266mDNS.h> // Include the Multicast DNS library
#include <ESP8266WebServer.h>   // Include the WebServer library
#include <FS.h>   // Include the SPIFFS library
#include <NTPClient.h> // Include the NTP library
#include <WiFiUdp.h> // Include the UDP library

//////////// Function Declarations ////////////
String getContentType(String filename); // Determine MIME type
bool handleFileRead(String path);       // Read file and send to client
void handleMain();          
void handleNotFound();
void handleLogin();
void handleLED();
void getLogs();
void Logger(String Event);
String getTime();

//////////// Constants ////////////
const char* ssid = "SSID"; // Wifi SSID
const char* password = "password"; // Wifi SSID // Comment out if applicable

const char* email = "IOT@sjtg.dev"; // Username
const char* login_password = "password"; // Password

const long utcOffsetInSeconds = -28800; // Offset for PST timezone

//////////// Variables ////////////
long token; // Auth Token
int ledPin = 2; // On board ESP12E LED
bool ledState = HIGH; // Start LED OFF (Onboard led is inverse)

ESP8266WebServer server(80); // Create our webserver object 


//////////// Setup ////////////
void setup(void){ 
  Serial.begin(9600);         // Start Serial 
  delay(2000); // Time to catch the serial monitor output

  randomSeed(analogRead(0)); // Seed token generator
  pinMode(ledPin, OUTPUT); // Set our LED pin to output
  digitalWrite(ledPin, ledState); // Turn off our LED pin

  WiFi.begin(ssid, password); // Start our WiFi // Remove Password if need be

  while (WiFi.status() != WL_CONNECTED) // Check if WiFi is connected
  {
     delay(500);
     Serial.print("connecting...\n");
  }

  Serial.println("WiFi connection Successful\n");

  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());              // Show network SSID
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());           // Show network ip of our IOT device

  if (MDNS.begin("tiny",WiFi.localIP())) {              // Start the mDNS responder for tiny.local // Make unique for same network
    Serial.println("mDNS responder started");
  } else {
    Serial.println("Error setting up MDNS responder!");
  }

  SPIFFS.begin();                           // Start the SPI Flash Files System
  
  // Only applies for URIs that are not implicitly declared 
  server.onNotFound([]() {                              // If the client requests any URI
    if (!handleFileRead(server.uri()))                  // send it if it exists
      server.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
  });
  
  const char * headerkeys[] = {"User-Agent", "Cookie"} ;
  size_t headerkeyssize = sizeof(headerkeys) / sizeof(char*);
  server.collectHeaders(headerkeys, headerkeyssize); // Ask server to track these headers

  server.on("/login", HTTP_POST, handleLogin); // Set up handle call for URI for request type and function
  server.on("/main.html", HTTP_GET, handleMain);
  server.on("/LED", HTTP_GET, handleLED);
  server.on("/getLogs", HTTP_GET, getLogs);
  
  server.begin();                           // Start out webserver
  Serial.println("HTTP server started");
  MDNS.addService("http", "tcp", 80); // Add our webserver to mDNS
}

//////////// Loop ////////////
void loop(void){
  MDNS.update();
  server.handleClient();
}

//////////// Auth Function ////////////
bool authenticated() {                       // Bool check for authentication
  if (server.hasHeader("Cookie")) {          // Check if cookies are present
    String cookie = server.header("Cookie"); // Grab contents of cookie header
    String str_token = String(token);        // Stringify our token
    Serial.println(str_token);
    if (cookie == "Auth-T="+str_token) {    // Compare cookie to our token
      Serial.println("Authentication Successful");
      return true;
    }
    return false;
  }
  else{
    return false;
  }
}

//////////// Get Content Type ////////////
String getContentType(String filename){
  if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

//////////// Read File ////////////
bool handleFileRead(String path){  // Send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);
  if(path.endsWith("/")) path += "index.html";           // If a folder is requested, send the index file
  String contentType = getContentType(path);             // Get the MIME type
  if (SPIFFS.exists(path)){  
    File file = SPIFFS.open(path, "r");  // Open the file
    server.streamFile(file, contentType);  // Send it to the client                 
    file.close();                                          // Close the file again
    Serial.println(String("\tSent file: ") + path);
    return true;
  }
  Serial.println(String("\tFile Not Found: ") + path);
  return false;                                          // If the file doesn't exist, return false
}

//////////// Authorize and Load Main Dashboard ////////////
void handleMain() {
  if (authenticated()) {
    handleFileRead("/main.html"); }    // Server main.html if authorized
  else {                                                                            
    server.send(401, "text/plain", "Unauthorized");
  }
}

//////////// Handle Login and Auth Token ////////////
void handleLogin() {                         // If a POST request is made to URI /login
  if( ! server.hasArg("email") || ! server.hasArg("password") 
      || server.arg("email") == NULL || server.arg("password") == NULL) { // If the POST request doesn't have username and password data
    server.send(400, "text/plain", "400: Invalid Request");         // The request is invalid, so send HTTP status 400
    return;
  }
  if(server.arg("email") == email && server.arg("password") == login_password) { // If both the username and the password are correct
    token = random(100000); // Create a random Auth token
    String str_token = String(token);
    Logger("Login Token Issued");
    server.sendHeader("Set-Cookie", "Auth-T="+str_token); // Set our auth token in the browser
    server.sendHeader("Location","/main.html");        // Add a main paige as our redirect location
    server.send(303);      // Send redirect
    Logger("User Logged In");
  } else {                                                                     
    server.send(401, "text/plain", "401: Unauthorized");
  }
}

//////////// Logger Function ////////////
void Logger(String Event){
  File spiffsLogFile = SPIFFS.open("/log.txt", "a"); // Open our log file in append mode
  String event2log = Event+","+getTime();  // Get our event and time
  if (event2log.length() > 10) { // Remove iregulaties
    spiffsLogFile.println(event2log);
  }
  spiffsLogFile.close(); // Close file
}

//////////// Request Logs ////////////
void getLogs() {
  if (authenticated()) { // Check for auto token
    String LogData;
    File spiffsLogFile = SPIFFS.open("/log.txt", "r"); // Open our log
    while (spiffsLogFile.available()){
            LogData += char(spiffsLogFile.read()); // Read out log data
          }
    server.send(200, "text/plain", LogData); // Send the log data

    Serial.println("Logs Retrived"); }
  else {                                                                             
    server.send(401, "text/plain", "Unauthorized");
  }
}

//////////// Control LED ////////////
void handleLED() {
  if (authenticated()) { // Check Auth 
    if (ledState == LOW){ // If Low set High or visa versa 
        ledState = HIGH;
    }
    else {
        ledState = LOW;
    }
    Logger("Led Action");
    digitalWrite(ledPin, ledState); // Change pin
    Serial.println("LED Successful"); }
  else {                                                                             
    server.send(401, "text/plain", "Unauthorized");
  }
}

//////////// Get Time ////////////
String getTime() {
  WiFiUDP ntpUDP;
  NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds); // Create NTP request with time offset 
  return timeClient.getFormattedTime();
}