

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
bool labelOpenTriggered = false; // 문 열기 1회 트리거 가드
bool errorState = false; // 오류 상태 플래그

bool login = false;  // 로그인 상태 변수

void setup() {
    Serial.begin(9600);

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
    if(login){
        labelCutter();
    }
    if(labelCutterState==true && !labelOpenTriggered){
        Serial.println("Door will opened..");
        executeOpenDoor();
        // 문 열기는 사이클 당 1회만 수행
        labelOpenTriggered = true;
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

// 공통 오류 트리거: 메시지 출력 후 모든 프로세스 강제 정지
void triggerError(const char* msg) {
    if (errorState) return;
    Serial.print("ERROR: ");
    Serial.println(msg);
    // 모든 모터/출력 정지
    stopMotor();
    // 비상 상황: 문이 열린 상태면 안전하게 닫기 시도
    emergencyCloseDoorIfOpen();
    // 오류 상황에서는 인버터까지 완전 차단
    digitalWrite(inverterPin, LOW);
    digitalWrite(fwdPin, LOW);
    Serial.println("All outputs OFF (inverter disabled) due to error.");
    errorState = true;
}

// 비상 닫기: 손 감지 시 즉시 중단, 제한 시간 내에만 시도
void emergencyCloseDoorIfOpen() {
    // 닫힘 센서가 HIGH면 이미 닫힘 상태
    int closeState = digitalRead(closeDoor_Sensor);
    if (closeState == HIGH) {
        Serial.println("Emergency: Door already closed.");
        return;
    }

    Serial.println("Emergency: Attempting to close door...");
    unsigned long startMs = millis();
    const unsigned long timeoutMs = 8000; // 8초 내 시도

    while (digitalRead(closeDoor_Sensor) == LOW) {
        // 손 감지되면 즉시 중단 (문을 열어두어 안전 확보)
        if (digitalRead(handSensor) == HIGH) {
            Serial.println("ERROR: Hand detected during emergency close; aborting close.");
            break;
        }

        // 닫힘 방향으로 구동
        digitalWrite(in3_Pin, HIGH);
        digitalWrite(in4_Pin, LOW);
        analogWrite(enb_Pin, 120);

        if (millis() - startMs > timeoutMs) {
            Serial.println("ERROR: Emergency close timeout; door may remain open.");
            break;
        }
        delay(50);
    }

    // 모터 정지
    digitalWrite(in3_Pin, LOW);
    digitalWrite(in4_Pin, LOW);
    analogWrite(enb_Pin, 0);

    if (digitalRead(closeDoor_Sensor) == HIGH) {
        Serial.println("Emergency: Door closed.");
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
    static int lastSensorState = HIGH; // 초기값을 HIGH로 잡아 즉시 정지 방지
    static unsigned long motorStartTime = 0;
    const unsigned long sensorIgnoreMs = 700; // 시작 후 짧은 시간 센서 무시
    const unsigned long maxRunMs = 15000; // 15초 타임아웃

    int switchState = digitalRead(labelSwitch);
if (labelCutterState) {
        return;
    }
    // 스위치 상승엣지 감지: 이전 LOW, 현재 HIGH일 때만 1회 실행
    if (switchState == HIGH && lastSwitchState == LOW) {
        motorRunning = true;
        motorStarted = true;
        digitalWrite(labelMotor, HIGH);
        motorStartTime = millis();
        Serial.println("Label motor started (login required)");
    }

    int currentSensorState = digitalRead(labelSensor);

    // 모터가 시작된 상태에서, 시작 후 지정 시간 경과 후에만 센서 하강엣지로 정지
    if (motorRunning && motorStarted) {
        unsigned long elapsed = millis() - motorStartTime;
        // 일정 시간 경과 후 센서 변화 감지로 정지
        if (elapsed > sensorIgnoreMs) {
            if (lastSensorState == HIGH && currentSensorState == HIGH) {
                motorRunning = false;
                motorStarted = false;
                digitalWrite(labelMotor, LOW);
                Serial.println("Label cutting done!");
                labelCutterState = true;
            }
        }
        // 최대 15초 넘기면 에러 처리
        if (elapsed > maxRunMs) {
            motorRunning = false;
            motorStarted = false;
            digitalWrite(labelMotor, LOW);
            // 라벨 분리기는 시스템 에러에서 제외: 모터만 정지하고 알림만 출력
            Serial.println("Label cutter timeout (15s) - label motor stopped (no system error)");
        }
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
    if (errorState) { Serial.println("Auto aborted due to error."); return; }
    
    // 3초 대기
    Serial.println("Waiting 3 seconds...");
    delay(3000);
    
    // 단계 2: 문 닫기
    Serial.println("=== Auto Step 2: Closing door ===");
    executeCloseDoor();
    if (errorState) { Serial.println("Auto aborted due to error."); return; }
    
    // 5초 대기
    Serial.println("Waiting 5 seconds...");
    delay(2000);
    
    // 단계 3: 센서1 기반 모터
    Serial.println("=== Auto Step 3: Running motor based on Sensor1 ===");
    executeSensor1Motor();
    if (errorState) { Serial.println("Auto aborted due to error."); return; }
    
    Serial.println("Waiting 3 seconds...");
    delay(3000);
    
    //단계 4: 센서2 기반 모터
    Serial.println("=== Auto Step 4: Running motor based on Sensor2 ===");
    executeSensor2Motor();
    if (errorState) { Serial.println("Auto aborted due to error."); return; }
    
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
        unsigned long startMs = millis();
        while(door_open_state == LOW) {
            digitalWrite(in3_Pin, LOW);
            digitalWrite(in4_Pin, HIGH);
            analogWrite(enb_Pin, 255);
            door_open_state = digitalRead(openDoor_Sensor);
            delay(100);
            if (millis() - startMs > 15000) {
                triggerError("Open door timeout (15s)");
                break;
            }
        }
        if (!errorState) {
            Serial.println("Door opened successfully!");
        }
        
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
        unsigned long startMs = millis();
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
                // 손 감지 구간은 타임아웃에서 제외하기 위해 타이머 리셋
                startMs = millis();
            }
            
            digitalWrite(in3_Pin, HIGH);
            digitalWrite(in4_Pin, LOW);
            analogWrite(enb_Pin, 120);
            door_close_state = digitalRead(closeDoor_Sensor);
            delay(100);
            if (millis() - startMs > 15000) {
                triggerError("Close door timeout (15s)");
                break;
            }
        }
        if (!errorState) {
            Serial.println("Door closed successfully!");
        }
        
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
        unsigned long startMs = millis();
        while (seesaw_State2 == HIGH) {
            digitalWrite(in1_Pin, LOW);
            digitalWrite(in2_Pin, HIGH);
            analogWrite(ena_Pin, 70);
            
            seesaw_State2 = digitalRead(seesaw_Sensor2);
            delay(100);
            if (millis() - startMs > 15000) {
                triggerError("Motor1 timeout (15s)");
                break;
            }

            
        digitalWrite(led_Blue, HIGH);
        }
            
        if (!errorState) {
            Serial.println("Sensor2 became LOW. Motor task completed!");
            Serial.println("24V Motor stopped.");
        }
        
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

        unsigned long startMs = millis();
        while (seesaw_State1 == HIGH) {
            digitalWrite(in1_Pin, HIGH);
            digitalWrite(in2_Pin, LOW);
            analogWrite(ena_Pin, 65);

            seesaw_State1 = digitalRead(seesaw_Sensor1);
            delay(100);
            if (millis() - startMs > 15000) {
                triggerError("Motor2 timeout (15s)");
                break;
            }
        }
        if (!errorState) {
            Serial.println("Sensor1 became LOW. Motor task completed!");
        }
    } else {
        Serial.println("Sensor1 is already LOW. No action needed.");
    }

    // 모터 정지 + knife off
    digitalWrite(in1_Pin, LOW);
    digitalWrite(in2_Pin, LOW);
    analogWrite(ena_Pin, 0);
    delay(5000);
    // Allow next label cycle
    labelCutterState = false;
    labelOpenTriggered = false;
    if (!errorState) {
        Serial.println("24V Motor stopped.");
    }
}

void stopMotor(){
    // 모션 정지: 정상(X) 정지 시에는 인버터는 유지하고 FWD만 해제
    digitalWrite(fwdPin, LOW);
    // 12V 모터 정지
    digitalWrite(in3_Pin, LOW);
    digitalWrite(in4_Pin, LOW);
    analogWrite(enb_Pin, 0);
    // 24V 모터 정지
    digitalWrite(in1_Pin, LOW);
    digitalWrite(in2_Pin, LOW);
    analogWrite(ena_Pin, 0);
    Serial.println("Motor stopped.");
    login = false;  // 로그인 상태를 false로 변경
        // Allow next label cycle
    labelCutterState = false;
    labelOpenTriggered = false;
    Serial.println("Knife deactivated.");
    Serial.println("*** LOGGED OUT BY EMERGENCY STOP ***");
    Serial.println("Enter '99' to login again");
}
