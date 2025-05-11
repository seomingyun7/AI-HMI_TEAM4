
#include <SoftwareSerial.h> // 통신라이브러리
#include <SD.h> // SD 카드 라이브러리 추가
#include <Servo.h>



#define MAX_TICK 10 
volatile uint16_t TICK[MAX_TICK]; // 멀티스레드
SoftwareSerial RS232Serial(10, 11);  // 소프트웨어 시리얼 객체 생성 (RX = 10, TX = 11) //rs232 통신

// RX, TX (ESP-01과 연결된 핀) , 메가보드 -> esp 8266 와이파이 모듈 -> 서버 -> 데이터베이스
SoftwareSerial esp(2, 3);  


//서보 모터 관련 정의
Servo servo1;
Servo servo2;

int angle1=0;
int angle2=0;

bool direction1 = true;      //증가 감소 플래그(true면 각도 증가중)
bool isWaiting1 = false;   // 대기를 위한 플래그(true면 최대/최소각)

bool direction2 = true;      //증가 감소 플래그(true면 각도 증가중)
bool isWaiting2 = false;   // 대기를 위한 플래그(true면 최대/최소각)

bool prevBtn1State1 = LOW;   // 이전 버튼 상태 저장
bool prevBtn1State2 = LOW;   // 이전 버튼 상태 저장

const int btn1 = 12;      //왼쪽 버튼
const int btn2 = 13;      //왼쪽 버튼

bool onoff1 = false;   //버튼 상태를 저장하는 변수(0:꺼짐/1:켜짐)
bool onoff2 = false; 

const int MAX_PACKET_SIZE = 64;         // 최대 패킷 크기 정의
uint8_t receivedData[MAX_PACKET_SIZE];  // 수신 데이터를 저장할 배열
int index = 0;                          // 현재 수신된 데이터의 인덱스
int expectedPacketSize = -1;            // 예상되는 패킷 크기 (-1은 아직 크기를 모름을 의미)
unsigned long lastReceiveTime = 0;      // 마지막으로 데이터를 수신한 시간 (타임아웃 체크용)

const int RED = 3; // LED가 연결된 핀 번호
const int BLUE = 5;

int up = 0;
uint8_t LEDON[8] = {0x5a, 0xa5, 0x05,0x82,0x10,0x00,0x00,0x01}; // LED ON 통신 프로토콜, 정해져있기 때문에 명령수행에 이용
uint8_t LEDOFF[8] = {0x5a, 0xa5, 0x05,0x82,0x10,0x00,0x00,0x00}; // LED ON 통신 프로토콜
uint8_t SWITCH[7] = {0x5a, 0xa5, 0x04,0x83,0x10,0x02, 0x01}; // 스위치 주소 변수 읽기 통신 프로토콜
uint8_t led_repeat[14] = {0x5A, 0xA5, 0x0B, 0x82, 0x09, 0x9C, 0x5A, 0xA5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};// 시리얼 모니터에 수행 출력 위함


// 현재 시간 정의
int currentYear = 0;
int currentMonth = 0;
int currentDay = 0;
int currentHour = 0;
int currentMinute = 0;
int currentSecond = 0;
int currentDayOfWeek = 0;


//target시간 정의
int targetYear = -1;
int targetMonth = -1;
int targetDay = -1;
int targetHour = -1;
int targetMinute = -1;
int targetSecond = -1;


// 데이터로 받아온 시간들을 구조체 선언, hasReached는 썼는지 안썼는지 검사할 플래그, 16진수 0도 false임
struct TargetTime 
{
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
  bool hasReached;
};
const int MAX_TARGET_TIMES = 256; // 최대 256개의 목표 시간 저장가능
TargetTime targetTimes[MAX_TARGET_TIMES]; // TargetTim 구조체 이름, targetTimes 구조체 배열이름
int targetTimeCount = 0; // 현재 저장된 목표 시간의 개수


// 깜빡임과 관련한 변수 정의
bool shouldBlink = false;
unsigned long blinkStartTime = 0;
unsigned long blinkInterval = 1000; // 1초 간격으로 깜빡임
bool ledState = HIGH;
unsigned long lastCheckTime = 0;
const long checkInterval = 1000; // 1초마다 현재 시간과 목표 시간 비교

void setup() 
{
  cli();
  set_timer_1KHz();
  sei();

  Serial.begin(9600);          // 시리얼 모니터 통신 속도 설정
  RS232Serial.begin(9600);     // RS232 장치와 통신 속도 설정
  esp.begin(9600);             //SoftwareSerial 통신 속도를 9600 보오레이트로 시작 (아두이노 -> ESP-01)
  
  pinMode(btn1, INPUT);   //버튼 핀을 입력보드로 설정 12
  pinMode(btn2, INPUT);   //버튼 핀을 입력보드로 설정 13
  
  servo1.attach(8);      //8번핀 사용
  servo2.attach(9);      //9번핀 사용
  
  pinMode(RED, OUTPUT);
  pinMode(BLUE, OUTPUT);
  //readRTC_monitor(); // 시간 표시

  loadTargetTimesFromSD();
  
  /*
  Serial.println("ESP-01 초기화 중.."); // 시리얼 모니터에 초기화 메시지 출력
  sendCommand("AT", 1000); // 상태 체크
  sendCommand("AT+CWMODE=1", 1000); // STA 모드로 설정
  sendCommand("AT+CWJAP=\"ET24G\",\"etetet6322488\"", 5000); // 와이파이 연결(이름과 비밀번호), AP 접속
  Serial.println("Wi-Fi 연결 시도...");
  
  delay(5000);
  */


}

void sendCommand(String command, int delayTime) 
// 특정 AT 명령어를 ESP-01로 보내고, 응답을 시리얼 모니터에 출력하는 함수
{
  esp.println(command); // pc-> ESP-01로 명령어를 보냄 
  delay(delayTime);     // ESP이 명령어를 처리할 시간을 기다림
  while (esp.available()) // ESP로부터 데이터가 아두이노에 도착했다면
  {
    Serial.write(esp.read());
    // ESP-01->pc로 수신한 데이터를 컴퓨터 시리얼 모니터로 출력, esp.read()는 바이트형태
  }
  /*while(Serial.available()) pc에서 아두이노로 보낸 데이터가 있으면 (반대의 경우 예시)
  {
    esp.write(Serial.read); 아두이노에서 데이터를 읽어 ESP에 전송
  }*/

}


//alram_name, alram_time %s,%H
void sendDataToServer(string alram_name, unsigned int alram_time) // 서버에 있는 데이터에 업데이트
{
  // 1. HTTP 요청 데이터 생성 (이 데이터를 서버에 업데이트 하겠습니다)
  String postData = "alram_name=" + String(alram_name)+ "&alram_time=" + String(alram_time);  // HTTP POST 요청해서 업로드할 데이터를 저장할 String 생성,&는 데이터 구분자

  String request = "POST /data HTTP/1.1\r\n";                         
  // request 는 전체 http 요청메시지를 담을 변수,"POST /data HTTP/1.1\r\n": HTTP 요청의 **요청 라인(request line) = 구문처럼 취급
  request += "와이파이 host:\r\n";                               // Host 헤더 설정 (서버의 IP 주소)
  request += "Content-Type: application/x-www-form-urlencoded\r\n";   // Content-Type 헤더 설정 (POST 데이터의 형식,key1=value1&key2=value2구조)
  request += "Content-Length: " + String(postData.length()) + "\r\n"; // Content-Length 헤더 설정 (POST 데이터의 길이
  request += "Connection: close\r\n\r\n";                             // Connection 헤더 설정 (요청 후 연결을 닫음)
  request += postData; // 본문 추가                                   // HTTP 요청 본문에 POST 데이터 추가

  // 2. TCP 연결 시작, TCP 연결을 시작하는 AT 명령어 생성 (프로토콜, IP 주소, 포트 번호)
  String cmd = "AT+CIPSTART=\"TCP\",\"aihmi.tplinkdns.com\",5000";
  sendCommand(cmd, 2000); // ESP-01로 TCP 연결 시작 명령어 전송
  
  // 3. 데이터 전송 크기 설정 ,전송할 데이터의 크기를 ESP-01에 알리는 AT 명령어 생성
  String sendCmd = "AT+CIPSEND=" + String(request.length());
  sendCommand(sendCmd, 1000);

  // 4. HTTP 요청 데이터 전송
  esp.print(request); // 📌 println()이 아니라 print() 사용
  delay(1000); // 데이터 전송시간

  // 5. 서버 응답 읽기
  Serial.println("서버 응답 대기 중...");
  long timeout = millis() + 5000; // 5초이상 기다림
  while (millis() < timeout) 
  {
    while (esp.available()) //도착한 데이터가 있으면
    {
      char c = esp.read(); //도착한 데이터를 읽어서
      Serial.write(c); //시리얼 모니터에 출력해라
    }
  }
  // 6. 연결 종료
  sendCommand("AT+CIPCLOSE", 1000);
}



void loop() 
{
  if(TICK[1] == 0)
  {
    receivePacket();  // 패킷 수신 처리 함수 실행
  }
  TICK[1] = 10;
  if(TICK[2] == 0)
  {
    readRTC(); // 현재 시간읽기  
    processTargetTimes(); // 다음 타겟 시간을 계속 확인
  TICK[2] = 800;
  }
  if(TICK[3] == 0)
  {
    if(digitalRead(btn1) == HIGH && prevBtn1State1 == LOW ) // 눌렀던 상태에서 버튼에서 손땔때
    {
      onoff1 = !onoff1; // 상태 토글
      servo1.write(angle1);
      if(onoff1)
      {
        
        angle1 = 180;  // 첫 누름: 180도로 이동
        Serial.println("모터가 180 도 회전");
      }
      else
      {
        angle1 = 0;  // 두번째 누르기: 180도로 이동
        Serial.println("모터가 원상복귀");
      }
    }
    prevBtn1State1 = digitalRead(btn1); // 상태 갱신, 버튼을 때면 low가됨 
    if(digitalRead(btn2) == HIGH && prevBtn1State2 == LOW )
    {
      onoff2 = !onoff2; // 상태 토글
      servo2.write(angle2);
      if(onoff2)
      {
        angle2 = 180;  // 첫 누름: 180도로 이동
        Serial.println("모터가 180 도 회전");
        
      }
      else
      {
        angle2 = 0;    // 두 번째 누름: 원점 복귀
        Serial.println("모터가 원상복귀");
      }
  	 prevBtn1State2 = digitalRead(btn2); // 상태 갱신
  TICK[3] = 50;
  }
  if(TICK[4] == 0) //데이터 업데이트 임
  {
    //Serial.println(String(temp) +"," + String(humi));
    //sendDataToServer(temp, humi);  // 테스트 데이터 전송
  }     
  TICK[4] = 10;
  //TICK 3 END
  if(TICK[5] == 0) 
  {
        
    up = 0;
  } 
  TICK[5]=50;
  }
  if(TICK[6]==0)
  {
    if (shouldBlink) // 깜빡여야할 때 
    {
      if (millis() - blinkStartTime < 5 * 60 * 1000) // blinkStartTime는 깜빡이기 시작한 시간
      {
        ledState = !ledState;
        digitalWrite(BLUE, ledState);
        TICK[5]=blinkInterval;
      }
       else 
      {
      shouldBlink = false; // 5분 후 깜빡임 종료
      digitalWrite(BLUE, HIGH); // LED 끄기 (원하는 상태로 변경)
      Serial.println("LED BLUE 깜빡임 종료");
      TICK[6] = -1;
      }
    }
  }
  //shouldBlink 가 언제 true 가 될까? 타겟시간과 현재시간이 일치할때
  if (targetHour != -1 && millis() - lastCheckTime >= checkInterval) // 타겟시간이 변화했고,마지막으로 시간을 변화한 이후로 경과한 시간이 checkInterval보다 크다면
  {
    lastCheckTime = millis();
    
    if (currentYear == targetYear && currentMonth == targetMonth && currentDay == targetDay && currentHour == targetHour && currentMinute == targetMinute && currentSecond == targetSecond) 
    {
      Serial.println("목표 시간에 도달! LED BLUE 5분간 깜빡임 시작");
      shouldBlink = true;
      blinkStartTime = millis();
      TICK[6] = blinkInterval;
      
    }
  }
}
//아두이노 SD 카드에서 목표 시간 데이터를 읽어오는 함수

void loadTargetTimesFromSD() 
{
  File dataFile = SD.open("madicine_table.txt"); // 파일 열기 [cite: 1]
  if (dataFile) { // 파일이 성공적으로 열렸는지 확인
    targetTimeCount = 0; // 목표 시간 개수 초기화
    while (dataFile.available() && targetTimeCount < MAX_TARGET_TIMES) 
    { // 파일에 데이터가 있고, 배열이 가득 차지 않았다면 반복 [cite: 1]
      String line = dataFile.readStringUntil('\n'); // 한 줄 읽기 [cite: 1]
      line.trim(); // 앞뒤 공백 제거
      if (line.length() > 0) // 빈 줄이 아닌 경우에만 처리, 한줄처리
      { 
        int values[6]; // 시간 값을 저장할 배열 (년, 월, 일, 시, 분, 초)
        int valueIndex = 0; // 배열 인덱스 초기화
        
        // 여기서부터 토큰이 끝나는데까지 잘 이해가 안됨
        char *str = line.c_str(); 
        /* 제 str이라는 "문자를 가리키는 포인터(화살표)" 변수를 선언합니다. 그리고 이 str 포인터에 line.c_str()이 반환한 메모리 주소를 저장합니다.
        즉, str 화살표는 line.c_str()이 만들어 놓은 C 스타일 문자열의 첫 번째 문자를 '콕' 하고 가리키게 됩니다. */
        char *token = strtok(str, ","); //str은 line 문자열의 시작 위치를 가리키는 화살표입니다.
        /*  char *token = strtok(str, ",");를 실행하면 strtok()는 str 문자열을 쉼표를 기준으로 나누고, 첫 번째 토큰인 "0x19"의 시작 주소를 token에 저장합니다. 
        str은 내부적으로 "0x19\0x05,0x05,0x09,0x00,0x00"으로 변경됩니다 ('\0'은 null 문자).  */ 

        while (token != NULL && valueIndex < 6) 
        { // 토큰이 있고, 배열에 모두 저장하지 않았다면 반복
          values[valueIndex] = strtol(token, NULL, 16); // 16진수 문자열을 정수로 변환 [cite: 1, 2] , 정수와 16진수는 직접비교가능
          valueIndex++; // 인덱스 증가
          token = strtok(NULL, ","); // 다음 토큰 얻기
        }

        if (valueIndex == 6) 
        { // 모든 시간 데이터를 읽은 경우
          targetTimes[targetTimeCount].year = values[0];   // 년
          targetTimes[targetTimeCount].month = values[1];  // 월
          targetTimes[targetTimeCount].day = values[2];    // 일
          targetTimes[targetTimeCount].hour = values[3];   // 시
          targetTimes[targetTimeCount].minute = values[4]; // 분
          targetTimes[targetTimeCount].second = values[5]; // 초
          
          targetTimes[targetTimeCount].hasReached = false; // 도달 여부 초기화
          targetTimeCount++; // 목표 시간 개수 증가
        } 
        else 
        {
          Serial.println("Invalid data format: " + line); // 잘못된 데이터 형식 출력
        }
      }
    }
    if (targetTimeCount > 0)  // 데이터가 하나 이상 읽혔을 경우에만
    { 
      targetTimes[0].hasReached = true;
    }
    dataFile.close(); // 파일 닫기 [cite: 1]
    Serial.println(targetTimeCount); // 읽어온 목표 시간 개수 출력
    Serial.println(" target times loaded from SD card."); // 메시지 출력
  } 
  else
  {
    Serial.println("Error opening madicine_table.txt"); // 파일 열기 실패 메시지 출력 [cite: 1]
  }
}


void processTargetTimes() // 읽어온 시간데이터 중 다음 타켓시간을 지정하는 함수
{
  for (int i = 0; i < targetTimeCount; i++) 
  {
    if (targetTimes[i].hasReached) // ture 일때 타겟시간으로 저장
    {
      targetYear = targetTimes[i].year;
      targetMonth = targetTimes[i].month;
      targetDay = targetTimes[i].day;
      targetHour = targetTimes[i].hour;
      targetMinute = targetTimes[i].minute;
      targetSecond = targetTimes[i].second;
      if (i + 1 < targetTimeCount) 
      {
       targetTimes[i + 1].hasReached = true;
      }
      break; // 찾았으면 루프 종료 (하나만 처리)
    }
  }
}


//RS232Serial.write() 요청명령어를 전송하는 함수는 기본적으로 바이트(byte) 형태의 데이터
//RS232Serial.available() 이 함수는 RS-232 시리얼 포트의 수신 버퍼에 현재 읽을 수 있는 바이트 수를 반환합니다.
//                        int 형의 값으로, 수신 버퍼에 있는 읽을 수 있는 바이트 수를 나타냅니다. 0이면 수신된 데이터가 없다는 의미입니다.
//RS232Serial.read();이 함수는 시리얼 버퍼에서 하나의 바이트를 읽어와서 정수형(int)으로 반환 2^8 


void readRTC_monitor() // 현재시간 바꾸기, 주입
{
  uint8_t requestCmd_m[14];
  requestCmd_m[0] = 0x5A; // 머리부분
  requestCmd_m[1] = 0xA5;
  requestCmd_m[2] = 0x0B; // 아래로 데이터 길이, 11개
  requestCmd_m[3] = 0x82; // 현재시간 전송 명령어
  requestCmd_m[4] = 0x00; //vp 주소 상위
  requestCmd_m[5] = 0x10; // vp 주소 하위 (나중에 변경가능)
  requestCmd_m[6] = 0x19; // 2025년도
  requestCmd_m[7] = 0x05; // 월
  requestCmd_m[8] = 0x08; // 일
  requestCmd_m[9] = 0x00; // 주
  requestCmd_m[10] = 0x0B; // 시
  requestCmd_m[11] = 0x13; // 분
  requestCmd_m[12] = 0x00; // 초
  requestCmd_m[13] = 0x00; // 0
  RS232Serial.write(requestCmd_m,sizeof(requestCmd_m)); // lcd로 요청 명령어 전송하는데 이용됨
  
  Serial.println("현재 시간  표시하기");
}

void readRTC_monitor2()
{
  uint8_t rtc_cmd[] = // 14
  {
    0x5A, 0xA5, 0x0C, 0x82,   // 헤더 + 명령어 (0x82) + 데이터 길이 (12)
    0x00, 0x9C,               // VP 주소 0x009C
    0x5A, 0xA5,               // 고정 패턴
    0x19, 0x05, 0x07,         // 년 월 일
    0x01, 0x1B, 0x00          // 시 분 초
  };
  RS232Serial.write(rtc_cmd, sizeof(rtc_cmd));
  Serial.println("RTC 설정 명령 전송 완료");
}

void readRTC() // 현재시간 요청함수 
{
  uint8_t requestCmd[7]={0x5A, 0xA5, 0x04, 0x83, 0x00, 0x10, 0x04};// 머리부분 // 아래로 데이터 길이// 읽기 전송요청 명령어//vp 주소 상위 // vp 주소 하위 // 읽을 워드 04개 
  RS232Serial.write(requestCmd,sizeof(requestCmd)); // lcd로 요청 명령어 전송하는데 이용됨
  //RS232Serial.write() 요청명령어를 전송하는 함수는 기본적으로 바이트(byte) 형태의 데이터
  //RS232Serial.available() 이 함수는 RS-232 시리얼 포트의 수신 버퍼에 현재 읽을 수 있는 바이트 수를 반환합니다.
  //                        int 형의 값으로, 수신 버퍼에 있는 읽을 수 있는 바이트 수를 나타냅니다. 0이면 수신된 데이터가 없다는 의미입니다.
  //RS232Serial.read();이 함수는 시리얼 버퍼에서 하나의 바이트를 읽어와서 정수형(int)으로 반환 2^8 
}

  
void receivePacket() // RS232Serial로부터 데이터가 들어오는 동안 반복 저장
{

  while (RS232Serial.available()) 
  {
   
    uint8_t receivedByte = RS232Serial.read();  // 한 바이트 읽기 여기서 예를 들어, 16진수 0x5A가 전송되었다면 receivedByte에는 십진수 값 90이 저장됩니다. int 형
    lastReceiveTime = millis();                 // 마지막 수신 시간 갱신

    // 첫 번째 바이트가 0x5A가 아니면 패킷이 아님, 무시
    if (index == 0 && receivedByte != 0x5A) 
    {
      
      continue;
    }

    // 읽은 바이트를 버퍼에 저장 ,receivedData 는 현재 바이트상태
    receivedData[index++] = receivedByte; 

    // 패킷 크기를 판별 (패킷의 세 번째 바이트가 데이터 길이 정보)
    if (index == 3) 
    {
      expectedPacketSize = 3 + receivedData[2];  // 헤더(3바이트) + 데이터 길이
      // 예상되는 패킷 크기가 최대 크기를 초과하면 리셋
      if (expectedPacketSize > MAX_PACKET_SIZE) 
      {
        index = 0;
        expectedPacketSize = -1;
      }
    }

    // 예상된 패킷 크기만큼 데이터를 모두 수신한 경우 패킷 처리
    if (expectedPacketSize > 0 && index >= expectedPacketSize) 
    {
      processPacket();  // 수신된 패킷을 처리하는 함수
      index = 0;
      expectedPacketSize = -1;
    }
  }

  // 데이터가 수신되었으나 타임아웃(200ms)된 경우 버퍼 초기화
  if (index > 0 && millis() - lastReceiveTime > 200) {
    Serial.println("Packet timeout, resetting buffer.");  // 타임아웃 메시지 출력
    index = 0;
    expectedPacketSize = -1;
  }
}




// 수신된 패킷을 처리하는 함수
void processPacket() 
{
  String packetString = ""; // 수신된 데이터를 16진수로 저장되는 문자열
  String time_packetString = "";
  bool time_packet=false; // 시간 데이터 패킷과 따로 받으려 사용,  5a a5 0c 83 5a a5 0c 83 00 10 04 11 01 01 00
  if (receivedData[0] == 0x5A && receivedData[1] == 0xA5 &&receivedData[3]==0x83&&receivedData[4]==0x00&&receivedData[5]==0x10&&receivedData[6]==0x04) // 시간에 관한 패킷
  {
    time_packet=true;
  } 
  for (int i = 0; i < index; i++) 
  {
    String hexValue = String(receivedData[i], HEX);  // 바이트 형태를 16진수로 만들고 문자열에 저장  
    if (receivedData[i] < 0x10)   //== ((receivedData[i], HEX)<16)
    {
      hexValue = "0" + hexValue; //그 문자열 앞에 0추가
    }
    if (time_packet) 
    {
      time_packetString += hexValue + " ";
    } 
    else 
    {
      packetString += hexValue + " ";
    } 
  }
  // 대문자로 출력되는 16진수를 소문자로 변환
  for (int i = 0; i < packetString.length(); i++) 
  {
    packetString[i] = tolower(packetString[i]);
  }
  // 변환된 패킷을 시리얼 모니터에 출력
  //Serial.println(time_packetString);
  Serial.print("수신된 패킷: ");
  Serial.println(packetString);
  if (receivedData[0] == 0x5A && receivedData[1] == 0xA5) // 응답헤더 확인, 헤더 일치 시
  {
    byte dataLength = receivedData[2]; //예상값은 12,0C
    if (dataLength == 0x0C) // 응답 데이터 길이 일치
    {
      if (receivedData[3]==0x83&&receivedData[4]==0x00&&receivedData[5]==0x10&&receivedData[6]==0x04) 
      {         
        currentYear = receivedData[7];        // 년도 (0-99) 뒤 두자리만 받도록 설정되어있음
        currentMonth = receivedData[8];       // 월 (0-12)
        currentDay = receivedData[9];         // 일 (0-31)
        currentDayOfWeek = receivedData[10];   // 주 
        currentHour = receivedData[11];        // 시 (0-23)
        currentMinute = receivedData[12];      // 분 (0-59) - 인덱스 주의!
        currentSecond = receivedData[13];      // 초 (0-59) - 인덱스 주의!
        Serial.print(receivedData[7]);
        Serial.print("년");
        Serial.print(receivedData[8]);
        Serial.print("월");
        Serial.print(receivedData[9]);
        Serial.print("일");
        Serial.print(receivedData[11]);
        Serial.print("시");
        Serial.print(receivedData[12]);
        Serial.print("분");
        Serial.print(receivedData[13]);
        Serial.println("초");

      } 
      else 
      {
        Serial.println("시간응답 데이터 불일치");
      }
    }
  }
  if (packetString == "5a a5 06 83 10 02 01 00 01 ") 
  {
    digitalWrite(RED, LOW);
    Serial.println("LED 켜짐");
  } 
  else if(packetString == "5a a5 06 83 10 02 01 00 00 ") 
  {
    digitalWrite(RED, HIGH);
    Serial.println("LED 꺼짐");
  }
  
}

// 1ms 타이머 인터럽트
ISR(TIMER2_COMPA_vect) {
  for (int i = 0; i < MAX_TICK; i++) {
    if (TICK[i] > 0) TICK[i]--;
  }
}

// 타이머 1kHz 설정
void set_timer_1KHz() {
  TCCR2A = 0;
  TCCR2B = 0;
  TCNT2 = 0;
  OCR2A = 249;
  TCCR2A |= (1 << WGM21);   // CTC 모드
  TCCR2B |= (1 << CS22);    // 분주비 64
  TIMSK2 |= (1 << OCIE2A);  // 인터럽트 허용
}