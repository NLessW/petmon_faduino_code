#include <Arduino.h>
#include <avr/wdt.h> // 워치독 타이머 라이브러리 추가

// === 기존 코드 변수 ===
int seesaw_Sensor1 = 27;
int seesaw_Sensor2 = 28;
int openDoor_Sensor = 37;
int closeDoor_Sensor = 36;
int handSensor = 22;

// 상태 변수
int seesaw_State1 = 0;
int seesaw_State2 = 0;
int door_open_state = 0;
int door_close_state = 0;
int hand_State = 0;
int inverter_State = 0;
int fwd_State = 0;

// 24V DC MOTOR PIN
int ena_Pin = 6;
int in1_Pin = 7;
int in2_Pin = 8;

// 12V DC MOTOR PIN
int enb_Pin = 11;
int in4_Pin = 10;
int in3_Pin = 9;

// Inverter PIN
int inverterPin = 50;
int fwdPin = 40;

// LED PIN
int led_Red = 46;
int led_Blue = 47;

// === 띠 분리기 관련 추가 ===
int labelSwitch = 24;  // 스위치 입력 핀
int labelSensor = 25;  // 센서 입력 핀
int labelMotor = 48;   // 모터 출력 핀

bool motorRunning = false;
bool switchPressed = false;
bool motorStarted = false;
int lastSensorState = HIGH;
bool labelCutterState = false;

bool login = false; // 로그인 상태 변수

void setup() {
  Serial.begin(9600);

  // === 워치독 타이머 설정 ===
  // 약 8초로 워치독 타이머를 설정합니다.
  // 시리얼 입력이 8초 이상 없으면 재부팅됩니다.
  wdt_enable(WDTO_8S);

  // 기존 센서 핀 설정
  pinMode(seesaw_Sensor1, INPUT);
  pinMode(seesaw_Sensor2, INPUT);
  pinMode(openDoor_Sensor, INPUT);
  pinMode(closeDoor_Sensor, INPUT);
  pinMode(handSensor, INPUT);

  pinMode(ena_Pin, OUTPUT);
  pinMode(in1_Pin, OUTPUT);
  pinMode(in2_Pin, OUTPUT);

  pinMode(enb_Pin, OUTPUT);
  pinMode(in3_Pin, OUTPUT);
  pinMode(in4_Pin, OUTPUT);

  pinMode(inverterPin, OUTPUT);
  pinMode(fwdPin, OUTPUT);

  pinMode(led_Red, OUTPUT);
  pinMode(led_Blue, OUTPUT);

  // 띠 분리기 핀 설정
  pinMode(labelSwitch, INPUT);
  pinMode(labelSensor, INPUT);
  pinMode(labelMotor, OUTPUT);

  digitalWrite(led_Red, HIGH);
  digitalWrite(led_Blue, HIGH);

  Serial.println("=== System Initialized ===");
  Serial.println("*** LOGIN REQUIRED ***");
  Serial.println("Enter '99' to login and unlock all functions");
  Serial.println("Enter 'h' for help");
}

void loop() {
  // 워치독 타이머 리셋 함수를 호출하여 타이머를 초기화합니다.
  // 이 함수가 주기적으로 호출되지 않으면 보드는 재부팅됩니다.
  wdt_reset();

  if(login){
    labelCutter();
  }
  if(labelCutterState==true){
    Serial.println("Door will opened..");
    executeOpenDoor();
    labelCutterState=false;
  }

  // 시리얼 명령 처리
  if (Serial.available() > 0) {
    String input = Serial.readString();
    input.trim();  // 공백 제거

    Serial.print("Input received: ");
    Serial.println(input);

    // 로그인 체크
    if (input == "99") {
      login = true;
      Serial.println("*** LOGIN SUCCESSFUL ***");
      Serial.println("All functions are now unlocked!");
      Serial.println("Enter 'h' for available commands");
      
      return;
    }

    // 로그인되지 않은 상태에서는 도움말과 센서 상태만 허용
    if (!login) {
      if (input == "h" || input == "H") {
        showLoginHelp();
      } else if (input == "0") {
        showSensorStatus();
      } else {
        Serial.println("*** ACCESS DENIED ***");
        Serial.println("Please login first by entering '99'");
        Serial.println("Available commands without login: 'h' (help), '0' (sensor status)");
      }
      return;
    }

    // 로그인된 상태에서만 실행되는 명령들
    if (input.length() == 1) {
      char command = input.charAt(0);
          
      switch(command) {
        case '1': executeOpenDoor(); break;
        case '2': executeCloseDoor(); break;
        case '3': executeSensor1Motor(); break;
        case '4': executeSensor2Motor(); break;
        case '5': runAutoSequence(); break;
        case '0': showSensorStatus(); break;
        case 'X': 
        case 'x': stopMotor(); break;
        case 'h':
        case 'H': showHelp(); break;
        case 'L':
        case 'l': logout(); break;
        default: Serial.println("Invalid command! Enter 'h' for help."); break;
      }
    } else {
      Serial.println("Invalid command! Enter 'h' for help.");
    }
  }
}

// 로그아웃 기능 추가
void logout() {
  login = false;
  Serial.println("*** LOGGED OUT ***");
  Serial.println("All functions are now locked.");
  Serial.println("Enter '99' to login again");
  stopMotor();

}

// 로그인 전 도움말
void showLoginHelp() {
  Serial.println("=== LOGIN REQUIRED ===");
  Serial.println("99 - Login to unlock all functions");
  Serial.println("h  - Show this help");
  Serial.println("0  - Show sensor status (allowed without login)");
  Serial.println("======================");
}

// 로그인 후 도움말
void showHelp() {
  Serial.println("=== Available Commands (Logged In) ===");
  Serial.println("1  - Open Door");
  Serial.println("2  - Close Door");
  Serial.println("3  - Run Sensor1 Motor (24V)");
  Serial.println("4  - Run Sensor2 Motor (24V)");
  Serial.println("5  - Run Full Auto Sequence");
  Serial.println("0  - Show Sensor Status");
  Serial.println("X  - Stop Motor (Emergency)");
  Serial.println("L  - Logout");
  Serial.println("h  - Show Help");
  Serial.println("=====================================");
}

// =========================
// 띠 분리기 제어 
// =========================
void labelCutter() {
  static int lastSwitchState = LOW;  // 이전 스위치 상태 저장
  int switchState = digitalRead(labelSwitch);

  // 스위치 상승엣지 감지: 이전 LOW, 현재 HIGH일 때만 1회 실행
  if (switchState == HIGH && lastSwitchState == LOW) {
    motorRunning = true;
    motorStarted = true;
    digitalWrite(labelMotor, HIGH);
    Serial.println("Label motor started (login required)");
  }

  int currentSensorState = digitalRead(labelSensor);

  // 센서 감지 시 모터 정지
  if (motorRunning && motorStarted && lastSensorState == HIGH && currentSensorState == LOW) {
    motorRunning = false;
    motorStarted = false;
    digitalWrite(labelMotor, LOW);
    Serial.println("Label cutting done!");
    labelCutterState = true;
  }

  lastSensorState = currentSensorState;
  lastSwitchState = switchState;  // 마지막에 현재 스위치 상태 저장
  
}

void showSensorStatus() {
  Serial.println("=== Current Sensor Status ===");
  
  int seesaw1 = digitalRead(seesaw_Sensor1);
  int seesaw2 = digitalRead(seesaw_Sensor2);
  int doorOpen = digitalRead(openDoor_Sensor);
  int doorClose = digitalRead(closeDoor_Sensor);
  int hand = digitalRead(handSensor);
  
  Serial.print("Seesaw Sensor1 (Pin 27): ");
  Serial.println(seesaw1 == HIGH ? "HIGH (Not Detected)" : "LOW (Detected)");
  
  Serial.print("Seesaw Sensor2 (Pin 28): ");
  Serial.println(seesaw2 == HIGH ? "HIGH (Not Detected)" : "LOW (Detected)");
  
  Serial.print("Door Open Sensor (Pin 36): ");
  Serial.println(doorOpen == HIGH ? "HIGH (Door Open)" : "LOW (Door Closed)");
  
  Serial.print("Door Close Sensor (Pin 37): ");
  Serial.println(doorClose == HIGH ? "HIGH (Door Closed)" : "LOW (Door Open)");
  
  Serial.print("Hand Sensor (Pin 22): ");
  Serial.println(hand == HIGH ? "HIGH (Hand Detected)" : "LOW (No Hand)");
  
  Serial.print("Login Status: ");
  Serial.println(login ? "LOGGED IN" : "NOT LOGGED IN");
  
  Serial.println("=============================");
}

void runAutoSequence() {
  Serial.println("Starting full automatic sequence...");
  
  // 단계 1: 문 열기 (knife 작동)
  Serial.println("=== Auto Step 1: Opening door ===");
  executeOpenDoor();
  
  // 3초 대기
  Serial.println("Waiting 3 seconds...");
  delay(3000);
  
  // 단계 2: 문 닫기
  Serial.println("=== Auto Step 2: Closing door ===");
  executeCloseDoor();
  
  // 5초 대기
  Serial.println("Waiting 5 seconds...");
  delay(2000);
  
  // 단계 3: 센서1 기반 모터
  Serial.println("=== Auto Step 3: Running motor based on Sensor1 ===");
  executeSensor1Motor();
  
  Serial.println("Waiting 3 seconds...");
  delay(3000);
  
  //단계 4: 센서2 기반 모터
  Serial.println("=== Auto Step 4: Running motor based on Sensor2 ===");
  executeSensor2Motor();
  
  // 모든 단계 완료 후 10초 대기 후 종료
  Serial.println("=== All steps completed ===");
  Serial.println("Waiting 10 seconds before shutdown...");
  delay(10000);
  
  // 시스템 종료
  Serial.println("=== System Shutdown ===");
  Serial.println("All processes completed. System is now idle.");
  Serial.println("To restart, reset the Arduino or enter new commands.");
  
  Serial.println("=== Full automatic sequence completed ===");
}

void executeOpenDoor() {
  // inverterPin 작동 (문 열기 시작 시)
  digitalWrite(inverterPin, HIGH);
  digitalWrite(fwdPin, HIGH);
  Serial.println("Knife activated!");
  
  door_open_state = digitalRead(openDoor_Sensor);
  Serial.print("Current door open sensor state: ");
  Serial.println(door_open_state);
  
  if (door_open_state == LOW){
    Serial.println("Door is closed. Opening door...");
    
    while(door_open_state == LOW) {
      digitalWrite(in3_Pin, LOW);
      digitalWrite(in4_Pin, HIGH);
      analogWrite(enb_Pin, 255);
      door_open_state = digitalRead(openDoor_Sensor);
      delay(100);
    }
    
    Serial.println("Door opened successfully!");
    
  } else {
    Serial.println("Door is already open. No action needed.");
  }

  // 모터 정지
  digitalWrite(in3_Pin, LOW);
  digitalWrite(in4_Pin, LOW);
  analogWrite(enb_Pin, 0);
  Serial.println("12V Motor stopped.");
  delay(3000);
}

void executeCloseDoor() {
  door_close_state = digitalRead(closeDoor_Sensor);
  
  Serial.print("Current door close sensor state: ");
  Serial.println(door_close_state);
  
  if (door_close_state == LOW){
    Serial.println("Door is open. Closing door...");
    
    while(door_close_state == LOW) {
      // 손 감지 확인
      if(digitalRead(handSensor) == HIGH) {
        Serial.println("*** HAND DETECTED! Stopping door and reopening ***");
        
        // 모터 즉시 정지
        digitalWrite(in3_Pin, LOW);
        digitalWrite(in4_Pin, LOW);
        analogWrite(enb_Pin, 0);
        
        // 문 다시 열기 (knife도 다시 작동)
        executeOpenDoor();
        delay(3000);
        
        // 손이 제거될 때까지 대기
        Serial.println("Waiting for hand to be removed...");
        while(digitalRead(handSensor) == HIGH) {
          Serial.println("Hand still detected. Please remove hand.");
          delay(500);
        }
        
        Serial.println("Hand removed. Resuming door closing...");
        delay(1000);
      }
      
      digitalWrite(in3_Pin, HIGH);
      digitalWrite(in4_Pin, LOW);
      analogWrite(enb_Pin, 120);
      door_close_state = digitalRead(closeDoor_Sensor);
      delay(100);
    }
    
    Serial.println("Door closed successfully!");
    
  } else {
    Serial.println("Door is already closed. No action needed.");
  }

  // 모터 정지
  digitalWrite(in3_Pin, LOW);
  digitalWrite(in4_Pin, LOW);
  analogWrite(enb_Pin, 0);
  Serial.println("Door stopped.");
  
}

void executeSensor1Motor() {

  seesaw_State2 = digitalRead(seesaw_Sensor2);
  
  Serial.print("Current Sensor2 state (used in Motor1): ");
  Serial.println(seesaw_State2);
  
  for(int i = 0; i < 5; i++){
    digitalWrite(led_Blue, LOW);
    delay(500);
    digitalWrite(led_Blue, HIGH);
    delay(500);
  }
  // 센서2가 HIGH(미감지)일 때 모터 회전 시작
  if (seesaw_State2 == HIGH) {
    Serial.println("Sensor2 is HIGH. Starting 24V motor (direction 1)...");
    
    while (seesaw_State2 == HIGH) {
      digitalWrite(in1_Pin, LOW);
      digitalWrite(in2_Pin, HIGH);
      analogWrite(ena_Pin, 80);
      
      seesaw_State2 = digitalRead(seesaw_Sensor2);
      delay(100);

      
    digitalWrite(led_Blue, HIGH);
    }
    
    Serial.println("Sensor2 became LOW. Motor task completed!");
    
    Serial.println("24V Motor stopped.");
    
  } else {
    Serial.println("Sensor2 is already LOW. No action needed.");
  }

  // 모터 정지 (knife off 없음)
  digitalWrite(in1_Pin, LOW);
  digitalWrite(in2_Pin, LOW);
  analogWrite(ena_Pin, 0);
  
}

void executeSensor2Motor() {
  delay(2000);
  seesaw_State1 = digitalRead(seesaw_Sensor1);
  
  Serial.print("Current Sensor1 state (used in Motor2): ");
  Serial.println(seesaw_State1);

  // 센서1이 HIGH(미감지)일 때 모터 회전 시작
  if (seesaw_State1 == HIGH) {
    Serial.println("Sensor1 is HIGH. Starting 24V motor (direction 2)...");

    while (seesaw_State1 == HIGH) {
      digitalWrite(in1_Pin, HIGH);
      digitalWrite(in2_Pin, LOW);
      analogWrite(ena_Pin, 60);

      seesaw_State1 = digitalRead(seesaw_Sensor1);
      delay(100);
    }
    
    Serial.println("Sensor1 became LOW. Motor task completed!");
  } else {
    Serial.println("Sensor1 is already LOW. No action needed.");
  }

  // 모터 정지 + knife off
  digitalWrite(in1_Pin, LOW);
  digitalWrite(in2_Pin, LOW);
  analogWrite(ena_Pin, 0);
  delay(5000);
  Serial.println("24V Motor stopped.");
}

void stopMotor(){
  digitalWrite(inverterPin, LOW);
  digitalWrite(fwdPin, LOW);
  login = false;  // 로그인 상태를 false로 변경
  Serial.println("Knife deactivated.");
  Serial.println("*** LOGGED OUT BY EMERGENCY STOP ***");
  Serial.println("Enter '99' to login again");
}
