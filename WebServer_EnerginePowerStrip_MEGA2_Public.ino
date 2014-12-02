/*
  Arduino Power Socket Control Web Server
  Author: jfrmilner
  Website: http://jfrmilner.wordpress.com/
  
  Additional Sources and thanks:
  * Simple Web Server code provided by David A. Mellis & Tom Igoe.
  * RCSwitch lib - Details: https://code.google.com/p/rc-switch/
  * ICMP Ping lib - Details http://www.blake-foster.com/projects/ICMPPing.zip
 
 */
#include <SPI.h>
#include <Ethernet.h>
#include <RCSwitch.h>
#include <ICMPPing.h>

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192,168,0, 40); //Public IP http://1.1.1.1/
EthernetServer server(80);
RCSwitch mySwitch = RCSwitch();
int refreshRequired = 0; //var used by the refresh page code
byte pingAddr[] = {192,168,0,123}; // ip address to ping
SOCKET pingSocket = 3; //ICMPPing
char pbuffer [256]; //ICMPPing
ICMPPing ping(pingSocket); //ICMPPing
bool pingRes; // pingRes stores the ping() result (true/false)
IPAddress ippdu1(192,168,0,2); //digiboard-A IP-PDU1
IPAddress ippdu2(192,168,0,3); //digiboard-A IP-PDU2
String readString;
//ippdu
char ippduSources[8] = {'A','B','C','D','E','F','G','H'};
//ippdu1
char ippdu1Outlets[8]; //char array to hold the 8 IP-PDU1 Power States
char* ippdu1Devices[9] = {"Monitor 4 - Top Left","Monitor 3 - Top Right","","","","","MS02","Monitor 5 (CRT)"};
//ippdu2
char ippdu2Outlets[8]; //char array to hold the 8 IP-PDU1 Power States
char* ippdu2Devices[9] = {"Desktop","Monitor 1 - Lower Right","Monitor 2 - Lower Left","","","","",""};


void setup() {
  Serial.begin(9600);
  mySwitch.enableTransmit(10);
  Ethernet.begin(mac, ip);
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());
  pinMode(8, OUTPUT);
  //Collect Power States for each of the PDUs at boot
  PDUWebStatus(1);
  PDUWebStatus(2);
  //Ping State at boot
  pingRes = ping(1, pingAddr, pbuffer); //int nRetries, byte * addr, char * result
}

//Show freeRam - http://playground.arduino.cc/Code/AvailableMemory
int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

//function to collect current power state and rotate
void PDUWebCommandPush(IPAddress ippdu, String LEDCode) {
  int requestedSocketPosition = LEDCode.indexOf('1');
  String LEDCodeNew = "00000000";
  LEDCodeNew[requestedSocketPosition] = '1';
  if (ippdu == ippdu1) {
    if (ippdu1Outlets[requestedSocketPosition] == '1') {
    PDUWebCommand(ippdu, "off" ,LEDCodeNew);
    }
    else {
    PDUWebCommand(ippdu, "on" ,LEDCodeNew);
    }
  }
    if (ippdu == ippdu2) {
    if (ippdu2Outlets[requestedSocketPosition] == '1') {
    PDUWebCommand(ippdu, "off" ,LEDCodeNew);
    }
    else {
    PDUWebCommand(ippdu, "on" ,LEDCodeNew);
    }
  }
  
}

//function to issue power state changes on the PDUs
void PDUWebCommand(IPAddress hostname, String powerState, String LEDCode) { 
  EthernetClient client;       
  Serial.println("connecting...");
  // if you get a connection, report back via serial:
  if (client.connect(hostname, 80)) {
    Serial.println("connected");
    // Make a HTTP request:
    client.print("GET /");
    client.print(powerState);
    client.print("s.cgi?led=");
    client.print(LEDCode);
    client.println("0000000000000000");
    client.print("Host: ");
    client.println(hostname);
    //clear text username:password are not accepted
    //encode your username:password (make sure colon is between username and password) to base64 http://www.motobit.com/util/base64-decoder-encoder.asp
    client.println("Authorization: Basic c25tcDoxMjM0"); 
    client.println("User-Agent: Arduino Sketch/1.0 snmp@1234");
    client.println();
  } 
  else {
    // if you didn't get a connection to the server:
    Serial.println("connection failed");
  } 
client.stop();
refreshRequired = 1;
}

//function to enumerate power states on PDU
void PDUWebStatus(int pdunum) {
  IPAddress hostname;
  if (pdunum == 1) {
   hostname = ippdu1; //digiboard-A IP-PDU1
  }
  if (pdunum == 2) {
   hostname = ippdu2; //digiboard-A IP-PDU2
  }
  
  EthernetClient client2;  
String readString;  
  Serial.println("connecting...");
  // if you get a connection, report back via serial:
  if (client2.connect(hostname, 80)) {
    Serial.println("connected");
    // Make a HTTP request:
    client2.println("GET /status.xml HTTP/1.1");
    client2.print("Host: ");
    client2.println(hostname);
    client2.println("Authorization: Basic c25tcDoxMjM0"); 
    client2.println("User-Agent: Arduino Sketch/1.0 snmp@1234");
    client2.println();
  } 
  else {
    // if you didn't get a connection to the server:
    Serial.println("connection failed");
  }
  delay(100);
  while (client2.available()) {
    //Serial.println("client available");
    char cc = client2.read();
          //read char by char HTTP request
            //store characters to string 
          readString += cc;
}
  //Debug
  //Serial.println("length");
  //Serial.println(readString.length());

  if (!client2.connected()) {
    Serial.println();
    Serial.println("disconnecting.");
    //Serial.println(readString); //print to serial monitor for debuging 
    //Serial.println("sub"); //print to serial monitor for debuging 
    String Sub = readString.substring(139,154);
    Sub.replace('3', '0'); //3=Offline Request Pending
    Sub.replace('2', '1'); //2=Online Request Pending
    Serial.println(Sub);
    // if the server's disconnected, stop the client:
    client2.stop();

    //Populate IP-PDU Power state array
      int s = 0;
      for (int i=0; i <= 7; i++){
      if (pdunum == 1) {
      ippdu1Outlets[i] = Sub.charAt(s); //digiboard-A IP-PDU1
      }
      if (pdunum == 2) {
      ippdu2Outlets[i] = Sub.charAt(s); //digiboard-A IP-PDU2
      }  
      s = s + 2;
    }
    
//    //DEBUG - Print IP-PDU Power state array
//    Serial.print("ippdu");
//    Serial.println(pdunum);
//      for (int i=0; i <= 7; i++){
//      Serial.print(i);
//      Serial.print("=");
//      if (pdunum == 1) {
//      Serial.println(ippdu1Outlets[i]);
//      }
//      if (pdunum == 2) {
//      Serial.println(ippdu2Outlets[i]);
//      }
//    }
  }
}


void loop() {
  
  // listen for incoming web clients
  EthernetClient client = server.available();
  if (client) {
    Serial.println("new client");
    
    //Check Server (LED) Voltage
    int sensorValue = analogRead(A0); // read the input on analog pin 0:
    float voltage = sensorValue * (5.0 / 1023.0); // Convert the analog reading (which goes from 0 - 1023) to a voltage (0 - 5V):
    //Debug Code
    //  Serial.print("PC Voltage = ");
    //  Serial.println(voltage);  // print out the value you read:
      
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    String buffer = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.print(c);
        buffer+=c;
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          //HTML START        
client.println("<head>");
client.println("<title>jfrmilners Arduino Page</title>");
client.println("</head>");
client.println("<meta name=\"viewport\" content=\"width=device-width\" />");
client.println("<script>");
client.println("function gtu(path, params, method) {");
client.println("    method = method || \"GET\";");
client.println("    var form = document.createElement(\"form\");");
client.println("    form.setAttribute(\"method\", method);");
client.println("    form.setAttribute(\"action\", path);");
client.println("    for(var key in params) {");
client.println("        if(params.hasOwnProperty(key)) {");
client.println("            var hF = document.createElement(\"input\");");
client.println("            hF.setAttribute(\"type\", \"hidden\");");
client.println("            hF.setAttribute(\"name\", key);");
client.println("            hF.setAttribute(\"value\", params[key]);");
client.println("            form.appendChild(hF);");
client.println("         }");
client.println("    }");
client.println("    document.body.appendChild(form);");
client.println("    form.submit();");
client.println("}");
client.println("</script>");
client.println("<body>");
client.println("<table width=\"768\" height=\"287\" border=\"1\">");
client.println("  <tr>");
client.println("    <th width=\"150\" scope=\"col\">Source</th>");
client.println("    <th width=\"170\" scope=\"col\">Device</th>");
client.println("    <th width=\"175\" scope=\"col\"><div align=\"center\">Power</div></th>");
client.println("    <th width=\"80\" scope=\"col\">Ping</th>");
client.println("    <th width=\"159\" scope=\"col\">Other</th>");
client.println("  </tr>");
client.println("  ");
client.println("  <tr>");
//Section One - Arduino Direct
client.println("    <td>Arduino Direct</td>");
client.println("    <td>MS01</td>");
//Section One - Arduino Direct - Check for voltage on PC LED         
if ( voltage >= 1.5 && voltage <= 4.5 ) { 
  client.println("    <td><div align=\"center\"><input name=\"b1\" button style=\"background-color:lightgreen; width: 160px\" type=\"button\" onClick=\"gtu('http://1.1.1.1/\',{'AD':'PUSH'})\" value=\"Power\" /></div></td>");
  //Ping
  pingRes = ping(1, pingAddr, pbuffer); //int nRetries, byte * addr, char * result
}
else {
  client.println("    <td><div align=\"center\"><input name=\"b1\" button style=\"background-color:red; width: 160px\" type=\"button\" onClick=\"gtu('http://1.1.1.1/\',{'AD':'PUSH'})\" value=\"Power\" /></div></td>");  
  pingRes = false;
}
//Section One - Arduino Direct - Ping Status td
  if ( pingRes ) {
  client.println("    <td>Online</td>");
}
  else {
  client.println("    <td>Offline</td>");  
}
client.print("    <td>");
//Section One - Arduino Direct - Voltage Status td
client.print(voltage);
client.println("v  </td>");
client.println("  </tr>");

//Section Two - Put Sets here
client.println("  <tr>");
client.println("    <td>Set - 1</td>");
client.println("    <td>Desktop + Monitors 1-4</td>");
client.println("    <td><div align=\"center\">");
client.println("      <input name=\"b10\" type=\"button\" style=\"height: 25px; width: 80px\" onClick=\"gtu('http://1.1.1.1/\',{'S1':'ON'})\" value=\"ON\" />");
client.println("      <input name=\"b10\" type=\"button\" style=\"height: 25px; width: 80px\" onClick=\"gtu('http://1.1.1.1/\',{'S1':'OFF'})\" value=\"OFF\" />");
client.println("    </div></td>");
client.println("    <td></td>");
client.println("    <td></td>");
client.println("  </tr>");

//Section Three - Energine RF Power Devices
client.println("  <tr>");
client.println("    <td>energenie - 1</td>");
client.println("    <td>Foscam IP Camera - 1</td>");
client.println("    <td><div align=\"center\">");
client.println("      <input name=\"b10\" type=\"button\" style=\"height: 25px; width: 80px\" onClick=\"gtu('http://1.1.1.1/\',{'E1':'ON'})\" value=\"ON\" />");
client.println("      <input name=\"b10\" type=\"button\" style=\"height: 25px; width: 80px\" onClick=\"gtu('http://1.1.1.1/\',{'E1':'OFF'})\" value=\"OFF\" />");
client.println("    </div></td>");
client.println("    <td></td>");
client.println("    <td></td>");
client.println("  </tr>");
client.println("  <tr>");
client.println("    <td>energenie - 2</td>");
client.println("    <td></td>");
client.println("    <td><div align=\"center\">");
client.println("      <input name=\"b10\" type=\"button\" style=\"height: 25px; width: 80px\" onClick=\"gtu('http://1.1.1.1/\',{'E2':'ON'})\" value=\"ON\" />");
client.println("      <input name=\"b10\" type=\"button\" style=\"height: 25px; width: 80px\" onClick=\"gtu('http://1.1.1.1/\',{'E2':'OFF'})\" value=\"OFF\" />");
client.println("    </div></td>");
client.println("    <td></td>");
client.println("    <td></td>");
client.println("  </tr>");
client.println("  <tr>");
client.println("    <td>energenie - 3</td>");
client.println("    <td>IP PDU 1</td>");
client.println("    <td><div align=\"center\">");
client.println("      <input name=\"b10\" type=\"button\" style=\"height: 25px; width: 80px\" onClick=\"gtu('http://1.1.1.1/\',{'E3':'ON'})\" value=\"ON\" />");
client.println("      <input name=\"b10\" type=\"button\" style=\"height: 25px; width: 80px\" onClick=\"gtu('http://1.1.1.1/\',{'E3':'OFF'})\" value=\"OFF\" />");
client.println("    </div></td>");
client.println("    <td></td>");
client.println("    <td></td>");
client.println("  </tr>");
client.println("  <tr>");
client.println("    <td>energenie - 4</td>");
client.println("    <td>LED Lamp</td>");
client.println("    <td><div align=\"center\">");
client.println("      <input name=\"b10\" type=\"button\" style=\"height: 25px; width: 80px\" onClick=\"gtu('http://1.1.1.1/\',{'E4':'ON'})\" value=\"ON\" />");
client.println("      <input name=\"b10\" type=\"button\" style=\"height: 25px; width: 80px\" onClick=\"gtu('http://1.1.1.1/\',{'E4':'OFF'})\" value=\"OFF\" />");
client.println("    </div></td>");
client.println("    <td></td>");
client.println("    <td></td>");
client.println("  </tr>");
client.println("  ");

//Section Four - IP PDUs
for (int pdu=1; pdu <= 2; pdu++){
  for (int i=0; i <= 7; i++){
//Debug Code    
//    Serial.print(i);
//    Serial.print("=");
//    Serial.println(ippdu1Outlets[i]);
    client.println("  <tr>");
    //Source td
    client.print("    <td>IP PDU ");
    client.print(pdu);
    client.print(" - ");
    client.print(ippduSources[i]);
    client.println("</td>");
    //Device td
    client.print("    <td>");
    if (pdu == 1) { 
    client.print(ippdu1Devices[i]);
    }
    if (pdu == 2) { 
    client.print(ippdu2Devices[i]);
    }  
    client.println("</td>");
    //Power td
    if (pdu == 1) { 
      if ( ippdu1Outlets[i] == '1') { 
      client.print("    <td><div align=\"center\"><input name=\"b1\" button style=\"background-color:lightgreen; width: 160px\" type=\"button\" onClick=\"gtu('http://1.1.1.1/\',{'PDU1");
      client.print(ippduSources[i]);
      client.println("':'PUSH'})\" value=\"Power\" /></div></td>");
      }
      else {
      client.print("    <td><div align=\"center\"><input name=\"b1\" button style=\"background-color:red; width: 160px\" type=\"button\" onClick=\"gtu('http://1.1.1.1/\',{'PDU1");
      client.print(ippduSources[i]);
      client.println("':'PUSH'})\" value=\"Power\" /></div></td>"); 
      }
    }
    if (pdu == 2) { 
      if ( ippdu2Outlets[i] == '1') { 
      client.print("    <td><div align=\"center\"><input name=\"b1\" button style=\"background-color:lightgreen; width: 160px\" type=\"button\" onClick=\"gtu('http://1.1.1.1/\',{'PDU2");
      client.print(ippduSources[i]);
      client.println("':'PUSH'})\" value=\"Power\" /></div></td>");
      }
      else {
      client.print("    <td><div align=\"center\"><input name=\"b1\" button style=\"background-color:red; width: 160px\" type=\"button\" onClick=\"gtu('http://1.1.1.1/\',{'PDU2");
      client.print(ippduSources[i]);
      client.println("':'PUSH'})\" value=\"Power\" /></div></td>");     
      }
    }  
    //Ping td
    client.println("    <td></td>");
    //Other td
    client.println("    <td></td>");
    client.println("  </tr>");    
  }
}
client.println("</table>");
client.println("</body>");
          //HTML END
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
          buffer=""; 
        } 
        
        else if (c == '\r') {
          // you've gotten a character on the current line
          //Arduino Direct Section
          if(buffer.indexOf("GET /?AD=PUSH")>=0) {
          digitalWrite(8,HIGH);
          delay(500);               // wait for half a second
          digitalWrite(8,LOW);
          refreshRequired = 1;            
          Serial.println("AD=PUSH");
          }
          
          //Set 1
          if(buffer.indexOf("GET /?S1=ON")>=0) {
          mySwitch.send(4314011, 24);
          delay(1000);
          PDUWebCommand(ippdu1, "on", "11000000" );
          PDUWebCommand(ippdu2, "on", "11100000" );
          refreshRequired = 1;
          Serial.println("S1=ON");
          }
          if(buffer.indexOf("GET /?S1=OFF")>=0) {
          PDUWebCommand(ippdu1, "off", "11000000" );
          PDUWebCommand(ippdu2, "off", "11100000" );
          refreshRequired = 1;
          Serial.println("S1=ON");
          }
         
          //energine section (4 ports)   
          if(buffer.indexOf("GET /?E1=ON")>=0) {
          mySwitch.send(4314015, 24); 
          refreshRequired = 1;
          }
          if(buffer.indexOf("GET /?E1=OFF")>=0) {
          mySwitch.send(4314014, 24); 
          refreshRequired = 1;
          }
          if(buffer.indexOf("GET /?E2=ON")>=0) {
          mySwitch.send(4314007, 24);
          refreshRequired = 1;
          }
          if(buffer.indexOf("GET /?E2=OFF")>=0) {
          mySwitch.send(4314006, 24);
          refreshRequired = 1;
          }
          if(buffer.indexOf("GET /?E3=ON")>=0) {
          mySwitch.send(4314011, 24);
          refreshRequired = 1;
          }
          if(buffer.indexOf("GET /?E3=OFF")>=0) {
          mySwitch.send(4314010, 24);
          refreshRequired = 1;
          }
          if(buffer.indexOf("GET /?E4=ON")>=0) {
          mySwitch.send(4314003, 24);
          refreshRequired = 1;
          }
          if(buffer.indexOf("GET /?E4=OFF")>=0) {
          mySwitch.send(4314002, 24);
          refreshRequired = 1;
          }
     
          //IP-PDU1 Section 8 ports (A-H)
          if(buffer.indexOf("GET /?PDU1A=PUSH")>=0) {
          PDUWebCommandPush(ippdu1, "10000000" );  
          }
          if(buffer.indexOf("GET /?PDU1B=PUSH")>=0) {
          PDUWebCommandPush(ippdu1, "01000000" );
          }
          if(buffer.indexOf("GET /?PDU1C=PUSH")>=0) {
          PDUWebCommandPush(ippdu1, "00100000" );
          }
          if(buffer.indexOf("GET /?PDU1D=PUSH")>=0) {
          PDUWebCommandPush(ippdu1, "00010000" );
          }
          if(buffer.indexOf("GET /?PDU1E=PUSH")>=0) {
          PDUWebCommandPush(ippdu1, "00001000" ); 
          }
          if(buffer.indexOf("GET /?PDU1F=PUSH")>=0) {
          PDUWebCommandPush(ippdu1, "00000100" ); 
          }
          if(buffer.indexOf("GET /?PDU1G=PUSH")>=0) {
          PDUWebCommandPush(ippdu1, "00000010" );
          }
          if(buffer.indexOf("GET /?PDU1H=PUSH")>=0) {
          PDUWebCommandPush(ippdu1, "00000001" ); 
          }

          //IP-PDU2 Section 8 ports (A-H)
          if(buffer.indexOf("GET /?PDU2A=PUSH")>=0) {
          PDUWebCommandPush(ippdu2, "10000000" );  
          }
          if(buffer.indexOf("GET /?PDU2B=PUSH")>=0) {
          PDUWebCommandPush(ippdu2, "01000000" );
          }
          if(buffer.indexOf("GET /?PDU2C=PUSH")>=0) {
          PDUWebCommandPush(ippdu2, "00100000" );
          }
          if(buffer.indexOf("GET /?PDU2D=PUSH")>=0) {
          PDUWebCommandPush(ippdu2, "00010000" );
          }
          if(buffer.indexOf("GET /?PDU2E=PUSH")>=0) {
          PDUWebCommandPush(ippdu2, "00001000" ); 
          }
          if(buffer.indexOf("GET /?PDU2F=PUSH")>=0) {
          PDUWebCommandPush(ippdu2, "00000100" ); 
          }
          if(buffer.indexOf("GET /?PDU2G=PUSH")>=0) {
          PDUWebCommandPush(ippdu2, "00000010" );
          }
          if(buffer.indexOf("GET /?PDU2H=PUSH")>=0) {
          PDUWebCommandPush(ippdu2, "00000001" ); 
          }
          
          else {
          //Serial.println("No Match");
          }
        }
        else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
        
      }
    }
    // give the web browser time to receive the data
    delay(1);
    
    //
    if (refreshRequired == 1 ) {
      //Ping
      pingRes = ping(1, pingAddr, pbuffer); //int nRetries, byte * addr, char * result
      //Debug Code  
      //  Serial.print("Ping Bool = ");
      //  Serial.println(pingRes);
      //  Serial.print("Ping Buffer = ");
      //  Serial.println(pbuffer);
      //  Serial.print("Ping Requried Counter = ");
      //  Serial.println(pingRequired);
        
      delay(100);
      PDUWebStatus(1);
      PDUWebStatus(2);
      Serial.println("refreshRequired");
      refreshRequired = 0;
      //Added to prevent a manual refresh of the page changing a powerstate due to content in the address bar
      client.println("<meta http-equiv=\"refresh\" content=\"5;url=http://1.1.1.1/\" >"); 
    }
    
    // close the connection:    
    client.stop();
    Serial.println(freeRam());   
    Serial.println("client disonnected");
  }
}
