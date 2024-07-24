#include <WiFi.h>
#include <WiFiClient.h>
#include <RoboClaw.h>
#include <ESP32Servo.h>

RoboClaw roboclaw(&Serial2, 10000);
String Mode;

String command = "";  // Declare command as an empty String

const char* ssid = "WIFI_0102";
const char* password = "admin0102";
WiFiServer server(80);

// Pin for solenoid valves
const int A1_Pin = 2;
const int A2_Pin = 15;
const int B1_Pin = 14;
const int B2_Pin = 12;
const int recycle_Pin = 13;

// Pin for water suction pump
const int re_motor_Pin = 27;

// Parameters for solenoid valve PWM pausing
const int ChannelA1 = 0;
const int ChannelRe = 4;
const int PWM_Resolution = 10;
const int maxDutyCycle = (int)(pow(2, PWM_Resolution) - 1);
const int onOffFreq = 1;

const int delay_suction = 850; // delay shut down water suction pump

// Advanced parameter variables
float adv_amplitude, adv_offset, adv_frequency;

// enum InteractionState { IDLE, START, PLAYING, WAITING };
// InteractionState actionState = IDLE;
// unsigned long actionTimer;
// int value = -1;


void setup() {
  Serial.begin(9600);
  roboclaw.begin(38400);

  connectToWiFi();
  server.begin();

  pinMode(A1_Pin, OUTPUT);   
  pinMode(recycle_Pin, OUTPUT);
  pinMode(re_motor_Pin, OUTPUT);
}

void connectToWiFi() {
  WiFi.begin(ssid, password); // Start the Wi-Fi connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void suction_pump_state(int status) {
  digitalWrite(re_motor_Pin, status ? HIGH : LOW);
}

void source_pump_state(int pwm_pct) {
  pwm_pct = constrain(pwm_pct, 0, 100);
  int source_pump_pwm = map(pwm_pct, 0, 100, 0, 127);
  roboclaw.ForwardM1(0x80, source_pump_pwm);
}


void solenoid_valve_state(int GPIO_dir, int freq, int A1_level, int Re_level){
  ledcSetup(ChannelA1, freq, PWM_Resolution);   // A1
  ledcAttachPin(A1_Pin, ChannelA1);
  ledcSetup(ChannelRe, freq, PWM_Resolution);   // Recycle
  ledcAttachPin(recycle_Pin, ChannelRe);
  
  GPIO.func_out_sel_cfg[recycle_Pin].inv_sel = GPIO_dir; 

  // set the status of the solenoid valve
  ledcWrite(ChannelA1, A1_level);
  ledcWrite(ChannelRe, Re_level);
}

void setup_advanced_parameters(int minRange, int maxRange, float decFreq){
  adv_amplitude = (maxRange - minRange) / 2;
  adv_offset = (maxRange + minRange) / 2;
  adv_frequency = decFreq;
}



void SystemSwitch(int switchNum) {
  switch (switchNum) {
    case 0:
      Serial.println("System OFF: pump is off, all valves are closed.");
      source_pump_state(0); // Turn off the source pump
      solenoid_valve_state(0, onOffFreq, 0, 0); // // Input: GPIO_dir, freq, A1_level, Re_level
      suction_pump_state(0); // Turn off the suction pump
      break;
    case 1:
      Serial.println("System ON: pump is on, only recycle valve is open.");
      solenoid_valve_state(0, onOffFreq, 0, maxDutyCycle);
      source_pump_state(100);             // Input: range of PWM 0-100: 0 being stopped and 100 full speed
      suction_pump_state(0);
      break;
  }
}

void loop() {
  // Check WiFi Connection
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }
  unsigned long startTime, startTime_sawtooth, period, period_sawtooth, stepDuration_sawtooth;
  int delay_advanceMode, advmin_input, advmax_input, sawtooth_pwmValue, pwmstep_input, minPWMValue, maxPWMValue, gradual_duration, pausing_freq_input, dutyCycle, reverse_flag, gradual_pwmValue;


  // Listen for incoming clients
  WiFiClient client = server.available();
  

  if (client) {
    String currentLine = "";

    // Process client requests
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        currentLine += c;

        if (c == '\n') {
          // Handle the request
          if (currentLine.startsWith("GET /?value=")) {
            int value = currentLine.substring(12).toInt();
            if (value == 1) {
              Serial.println("Receiving touch request");
              command = "touch";
            }
            else if (value == 2) {
              command = "press";
            }
            else if (value == 3) {
              command = "injection";
            }
            else if (value == 4) {
              command = "shield1";
            }
            else if (value == 5) {
              command = "shield2";
            }
            else if (value == 6) {
              command = "shield3";
            }
            else if (value == 7) {
              command = "plantA2Hz";
            }
            else if (value == 8) {
              command = "plantA3Hz";
            }
            else if (value == 9) {
              command = "plantA7Hz";
            }
            else if (value == 0) {
              command = "plantB";
            }
            else {
              command = "";
            }
          }

          if (currentLine.startsWith("GET /?volume=")) {
            int volume = currentLine.substring(13).toInt();
            SystemSwitch(volume);
          }

          // Clear currentLine after processing the request
          currentLine = "";
        }

        if (currentLine.length() == 0) {
          // Send HTTP response
          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:text/plain");
          client.println("Connection: close");
          client.println();
          client.stop();
          break; // End client communication
        }
      }
      
    }
  }


  if (command == ""){
    // Serial.println("Do Nothing.");
  }
  else if (command == "touch") {
    Serial.println("touch");

    source_pump_state(40);        // source pump PWM = 40%
    suction_pump_state(1);        // suction pump is on
    solenoid_valve_state(0, onOffFreq, maxDutyCycle, maxDutyCycle);    // A1 is on, Re is on
    delay(1800);
    solenoid_valve_state(0, onOffFreq, 0, maxDutyCycle);
    delay(delay_suction);
    source_pump_state(100); 
    suction_pump_state(0);

    command = "";
  }
  else if (command == "press") {
    source_pump_state(80);        // source pump PWM = 70%
    suction_pump_state(1);        // suction pump is on
    solenoid_valve_state(0, onOffFreq, maxDutyCycle, 0);    // A1 is on, Re is off
    delay(1800);
    solenoid_valve_state(0, onOffFreq, 0, maxDutyCycle);
    delay(delay_suction);
    source_pump_state(100); 
    suction_pump_state(0);

    command = "";
  }
  else if (command == "injection") {
    // source_pump_state(100);        // source pump PWM = 100%
    // suction_pump_state(1);        // suction pump is on
    // solenoid_valve_state(0, onOffFreq, maxDutyCycle, 0);    // A1 is on, Re is off
    // delay(5000);
    // solenoid_valve_state(0, onOffFreq, 0, maxDutyCycle);
    // delay(delay_suction);
    // source_pump_state(100); 
    // suction_pump_state(0);

    delay_advanceMode = 4500;
    advmin_input = 20;
    advmax_input = 100;
    setup_advanced_parameters(advmin_input, advmax_input, 0.1);
    suction_pump_state(1);
    solenoid_valve_state(0, onOffFreq, maxDutyCycle, 0);

    const unsigned long period_triangle = (unsigned long)(1000 / adv_frequency);  // Total period in milliseconds
    const int steps = (advmax_input - advmin_input); // Number of steps for ramp up or down
    const unsigned long stepDuration = period_triangle / (2 * steps); // Duration of each step
    unsigned long startTime_triangle = millis();
    int triangle_pwmValue = advmin_input;
    bool rampUp = true; // Start with ramping up

    while (millis() - startTime_triangle < delay_advanceMode) {
      source_pump_state(triangle_pwmValue);
      Serial.println(triangle_pwmValue);
      delay(stepDuration);
      if (rampUp) {
        triangle_pwmValue++;
        if (triangle_pwmValue >= advmax_input) {
          rampUp = false; // Start ramping down
        }
      } else {
        triangle_pwmValue--;
        if (triangle_pwmValue <= advmin_input) {
          rampUp = true; // Start ramping up
        }
      }
    }
    solenoid_valve_state(0, onOffFreq, 0, maxDutyCycle);
    delay(delay_suction);
    suction_pump_state(0);
    source_pump_state(100);

    command = "";
  }
  else if (command == "shield1") {
    delay_advanceMode = 6000;
    advmin_input = 30;
    advmax_input = 100;
    setup_advanced_parameters(advmin_input, advmax_input, 0.5); // minRange, maxRange, freq
    suction_pump_state(1);
    solenoid_valve_state(0, onOffFreq, maxDutyCycle, 0);
    period = (unsigned long)(1000 / adv_frequency);
    startTime = millis();
    while (millis() - startTime < delay_advanceMode) {
      unsigned long currentTime = millis() - startTime;
      float t = (float)currentTime / period; // Normalized time variable
      
      // Calculate sine value
      float sineValue = adv_amplitude * sin(2 * PI * t) + adv_offset;
      
      // Ensure PWM value is within the range min to max
      int sine_pwmValue = (int)constrain(sineValue, advmin_input, advmax_input);
      
      source_pump_state(sine_pwmValue);
      
      delay(50); // Small delay to prevent too frequent updates
    }
    solenoid_valve_state(0, onOffFreq, 0, maxDutyCycle);
    delay(delay_suction);
    suction_pump_state(0);
    source_pump_state(100);

    command = "";
  }
  else if (command == "shield2") {
    delay_advanceMode = 6000;
    advmin_input = 30;
    advmax_input = 100;
    setup_advanced_parameters(advmin_input, advmax_input, 0.3);
    suction_pump_state(1);
    solenoid_valve_state(0, onOffFreq, maxDutyCycle, 0);

    period_sawtooth = (unsigned long)(1000 / adv_frequency);
    stepDuration_sawtooth = period_sawtooth / (advmax_input - advmin_input);
    startTime_sawtooth = millis();
    sawtooth_pwmValue = advmin_input;

    while (millis() - startTime_sawtooth < delay_advanceMode) {
      source_pump_state(sawtooth_pwmValue);
      delay(stepDuration_sawtooth);
      sawtooth_pwmValue++;
      if (sawtooth_pwmValue > advmax_input) {
        sawtooth_pwmValue = advmin_input; // Reset to minimum
      }
    }
    solenoid_valve_state(0, onOffFreq, 0, maxDutyCycle);
    delay(delay_suction);
    suction_pump_state(0);
    source_pump_state(100);

    command = "";
  }
  else if (command == "shield3") {

    delay_advanceMode = 6000;
    advmin_input = 30;
    advmax_input = 100;
    setup_advanced_parameters(advmin_input, advmax_input, 0.5);
    suction_pump_state(1);
    solenoid_valve_state(0, onOffFreq, maxDutyCycle, 0);

    const unsigned long halfPeriod = (unsigned long)(500 / adv_frequency);  // Half period in milliseconds
    unsigned long startTime_square = millis();
    bool isHigh = true;

    while (millis() - startTime_square < delay_advanceMode) {
      int square_pwmValue = isHigh ? advmax_input : advmin_input;
      isHigh = !isHigh;
      source_pump_state(square_pwmValue);
      Serial.println(square_pwmValue);
      delay(halfPeriod);
    }
    solenoid_valve_state(0, onOffFreq, 0, maxDutyCycle);
    delay(delay_suction);
    suction_pump_state(0);
    source_pump_state(100);

    command = "";
  }
  else if (command == "plantA2Hz") {
    pausing_freq_input = 2;
    dutyCycle = 0.3 * maxDutyCycle;
    reverse_flag = 1;
    
    source_pump_state(100);
    suction_pump_state(1);
    solenoid_valve_state(reverse_flag, pausing_freq_input, dutyCycle, dutyCycle);
    delay(7000);
    solenoid_valve_state(0, pausing_freq_input, 0, maxDutyCycle);
    delay(delay_suction);
    suction_pump_state(0);
    source_pump_state(100);

    command = "";
  }
  else if (command == "plantA3Hz") {
    pausing_freq_input = 3;
    dutyCycle = 0.3 * maxDutyCycle;
    reverse_flag = 1;
    
    source_pump_state(100);
    suction_pump_state(1);
    solenoid_valve_state(reverse_flag, pausing_freq_input, dutyCycle, dutyCycle);
    delay(7000);
    solenoid_valve_state(0, pausing_freq_input, 0, maxDutyCycle);
    delay(delay_suction);
    suction_pump_state(0);
    source_pump_state(100);

    command = "";
  }
  else if (command == "plantA7Hz") {
    pausing_freq_input = 7;
    dutyCycle = 0.3 * maxDutyCycle;
    reverse_flag = 1;
    
    source_pump_state(100);
    suction_pump_state(1);
    solenoid_valve_state(reverse_flag, pausing_freq_input, dutyCycle, dutyCycle);
    delay(7000);
    solenoid_valve_state(0, pausing_freq_input, 0, maxDutyCycle);
    delay(delay_suction);
    suction_pump_state(0);
    source_pump_state(100);

    command = "";
  }
  else {
    command = "";
  }

}

