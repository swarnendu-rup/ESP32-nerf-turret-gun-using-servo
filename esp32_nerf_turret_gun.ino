/*
🧩 Components Required:
Component	Quantity	Notes
1.ESP32 Dev Board	Acts as Wi-Fi AP and controller
2.Servo Motor (e.g., SG90/MG90S)	1	Controls trigger pulling mechanism
3.DC Motors (Flywheels)	2	For spinning the dart launcher wheels
4.Motor Driver (L298N or custom H-Bridge)	1	Controls the DC motors
5.External Power Source	1	For motors (e.g., 7.4V LiPo or 9V)
6.Breadboard & Jumper Wires	Several	For connections

🔌 Connection Diagram:
🔧 Servo Motor (Trigger Servo):
Signal → GPIO 18 (PWM supported)

VCC → 3.3V or external 5V (with level shifting if needed)

GND → GND

🔧 Motor Driver Pins (L298N or Similar):
Assuming you're using two DC motors and 4 control pins:

Motor1_A → GPIO 12

Motor1_B → GPIO 13

Motor2_A → GPIO 14

Motor2_B → GPIO 27

Power Supply:

Motors VCC → External Power (e.g. 7.4V battery)

GND of Power → GND of ESP32 (shared ground is critical)

Tip: If using L298N, connect EN pins to 5V to enable the motors always or PWM if you want speed control.

🌐 WiFi & Web Interface:
ESP32 creates a local Wi-Fi hotspot with SSID "sim" and password "simple12".

Any phone/laptop can connect to it and open the web interface at 192.168.4.1.

Control options: Single Fire, Start/Stop Continuous Fire, Emergency Stop

🧠 Working Logic Summary:
Single Fire: Starts motors → Activates trigger servo → Stops motors after 2s.

Continuous Mode: Motors keep running → Fires every 500ms → Stops when toggled off.

Fail-Safe: Auto stops motors after 2s in single-fire if not in continuous mode.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// Servo setup
Servo triggerServo;
const int TRIGGER_SERVO_PIN = 18;

// Motor pins
const int MOTOR1_A = 12;
const int MOTOR1_B = 13;
const int MOTOR2_A = 14;
const int MOTOR2_B = 27;

// WiFi credentials for Access Point
const char* ssid = "sim";
const char* password = "simple12";

// Web server
WebServer server(80);

// Motor control
bool motorsRunning = false;
unsigned long motorStartTime = 0;
const unsigned long MOTOR_RUN_TIME = 2000;

// Firing control
bool continuousMode = false;
bool isFiring = false;
unsigned long lastFireTime = 0;
const unsigned long FIRE_INTERVAL = 500;
const unsigned long TRIGGER_PRESS_TIME = 200;
unsigned long triggerPressStartTime = 0;
bool triggerPressed = false;

void setup() {
  Serial.begin(115200);

  // Servo setup
  triggerServo.attach(TRIGGER_SERVO_PIN);
  triggerServo.write(0);

  // Motor setup
  pinMode(MOTOR1_A, OUTPUT);
  pinMode(MOTOR1_B, OUTPUT);
  pinMode(MOTOR2_A, OUTPUT);
  pinMode(MOTOR2_B, OUTPUT);
  stopMotors();

  // Start Access Point
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.println("Access Point started");
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Web routes
  server.on("/", handleRoot);
  server.on("/fire", handleFire);
  server.on("/continuous", handleContinuous);
  server.on("/stop", handleStop);
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();

  if (continuousMode && isFiring && millis() - lastFireTime > FIRE_INTERVAL) {
    fireSingleShot();
    lastFireTime = millis();
  }

  if (triggerPressed && millis() - triggerPressStartTime > TRIGGER_PRESS_TIME) {
    triggerServo.write(0);
    triggerPressed = false;
  }

  if (!continuousMode && motorsRunning && millis() - motorStartTime > MOTOR_RUN_TIME) {
    stopMotors();
    motorsRunning = false;
    Serial.println("Motors stopped automatically");
  }
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><title>Nerf Turret Control</title><style>";
  html += "body { font-family: Arial; text-align: center; margin: 50px; }";
  html += "button { padding: 20px; margin: 10px; font-size: 18px; }";
  html += ".fire-button { background-color: red; color: white; font-size: 24px; }";
  html += "</style></head><body><h1>Nerf Turret Control</h1>";
  html += "<button class='fire-button' onclick='singleFire()'>SINGLE FIRE</button><br><br>";
  html += "<button class='fire-button' onclick='toggleContinuous()' id='continuousBtn'>START CONTINUOUS</button><br><br>";
  html += "<button onclick='stopFiring()' style='background-color: orange; color: white;'>STOP FIRING</button>";
  html += "<p>Motors: <span id='motorStatus'>Stopped</span></p>";
  html += "<p>Fire Mode: <span id='fireMode'>Single</span></p>";
  html += "<script>";
  html += "let continuousMode = false;";
  html += "function singleFire() { fetch('/fire'); document.getElementById('motorStatus').textContent = 'Single Fire!'; document.getElementById('fireMode').textContent = 'Single'; setTimeout(() => { document.getElementById('motorStatus').textContent = 'Stopped'; }, 2000); }";
  html += "function toggleContinuous() { if (!continuousMode) { continuousMode = true; document.getElementById('continuousBtn').textContent = 'STOP CONTINUOUS'; document.getElementById('continuousBtn').style.backgroundColor = 'darkred'; document.getElementById('motorStatus').textContent = 'Continuous Fire!'; document.getElementById('fireMode').textContent = 'Continuous'; fetch('/continuous?mode=start'); } else { stopContinuous(); } }";
  html += "function stopContinuous() { continuousMode = false; document.getElementById('continuousBtn').textContent = 'START CONTINUOUS'; document.getElementById('continuousBtn').style.backgroundColor = 'red'; document.getElementById('motorStatus').textContent = 'Stopped'; document.getElementById('fireMode').textContent = 'Single'; fetch('/continuous?mode=stop'); }";
  html += "function stopFiring() { if (continuousMode) { stopContinuous(); } fetch('/stop').then(() => { document.getElementById('motorStatus').textContent = 'Stopped'; }); }";
  html += "</script></body></html>";
  server.send(200, "text/html", html);
}

void handleFire() {
  Serial.println("Single FIRE command received!");
  runMotors();
  motorsRunning = true;
  motorStartTime = millis();
  fireSingleShot();
  server.send(200, "text/plain", "SINGLE FIRE!");
}

void handleContinuous() {
  if (server.hasArg("mode")) {
    String mode = server.arg("mode");
    if (mode == "start") {
      Serial.println("Starting continuous fire mode");
      continuousMode = true;
      isFiring = true;
      runMotors();
      motorsRunning = true;
      motorStartTime = millis();
      lastFireTime = millis();
      server.send(200, "text/plain", "CONTINUOUS FIRE STARTED");
    } else if (mode == "stop") {
      Serial.println("Stopping continuous fire mode");
      continuousMode = false;
      isFiring = false;
      stopMotors();
      motorsRunning = false;
      server.send(200, "text/plain", "CONTINUOUS FIRE STOPPED");
    }
  } else {
    server.send(400, "text/plain", "Missing mode parameter");
  }
}

void handleStop() {
  Serial.println("Emergency stop command received!");
  continuousMode = false;
  isFiring = false;
  stopMotors();
  motorsRunning = false;
  triggerServo.write(0);
  triggerPressed = false;
  server.send(200, "text/plain", "STOPPED");
}

void fireSingleShot() {
  triggerServo.write(90);
  triggerPressed = true;
  triggerPressStartTime = millis();
  Serial.println("Trigger actuated - dart fired!");
}

void runMotors() {
  digitalWrite(MOTOR1_A, HIGH);
  digitalWrite(MOTOR1_B, LOW);
  digitalWrite(MOTOR2_A, HIGH);
  digitalWrite(MOTOR2_B, LOW);
  Serial.println("Motors started - flywheels spinning");
}

void stopMotors() {
  digitalWrite(MOTOR1_A, LOW);
  digitalWrite(MOTOR1_B, LOW);
  digitalWrite(MOTOR2_A, LOW);
  digitalWrite(MOTOR2_B, LOW);
  Serial.println("Motors stopped");
}