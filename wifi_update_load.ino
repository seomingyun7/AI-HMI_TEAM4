#include <SoftwareSerial.h>
// #include <Adafruit_Sensor.h>
// #include <DHT.h>
// #include <DHT_U.h>

//#define DHTPIN 4    // DHT11 센서를 연결할 아두이노 핀 번호
//#define DHTTYPE DHT11 // DHT 센서의 종류를 DHT11으로 설정

#define esp Serial2 // Rx2(17),Tx2(16)
//SoftwareSerial esp(2, 3);   // SoftwareSerial 객체 'esp' 생성, 아두이노 디지털 2번 핀을 RX, 3번 핀을 TX로 사용 (ESP-01과 연결)
// DHT dht(DHTPIN, DHTTYPE); // DHT 객체 생성 (DHT11 센서를 DHTPIN에 연결)


// ESP-01로 명령어를 보내고 지정된 시간만큼 대기하는 함수
void sendCommand(String command, int delayTime)
{
  esp.println(command);   // ESP-01로 명령어를 보낸 후 줄 바꿈 문자 추가
  delay(delayTime);       // 지정된 시간만큼 대기
}

// ESP-01로부터 응답을 읽어와 String 형태로 반환하는 함수 (타임아웃 설정)
String readESPResponse(unsigned long timeout = 5000) 
{
  String res = "";         // 응답 문자열 초기화
  unsigned long start = millis(); // 현재 시간을 저장하여 타임아웃 계산 시작
  while (millis() - start < timeout) 
  { // 타임아웃 시간 5초 내에
    while (esp.available()) 
    { // ESP-01로부터 데이터가 도착했다면
      char c = esp.read();   // 도착한 데이터를 읽어서 char 변수에 저장
      res += c;              // 읽은 문자를 응답 문자열에 추가
      Serial.write(c);       // 읽은 문자를 시리얼 모니터로 출력
    }
  }
  return res;                // 최종 응답 문자열 반환
}


void setup() 
{
  Serial.begin(9600);
  
  esp.begin(9600);
 
  // dht.begin();

  Serial.println("ESP-01 초기화 중..");

  sendCommand("AT", 1000); // 상태 체크
  readESPResponse();
  sendCommand("AT+CWMODE=1", 1000); // STA 모드로 설정
  readESPResponse();
  sendCommand("AT+CWJAP=\"hanzzi\",\"12345678\"", 5000); // 와이파이 연결(이름과 비밀번호)
  readESPResponse();
  //sendCommand("AT+CWJAP=\"LeeGyeongseo\",\"00000000\"", 5000); // 와이파이 연결(이름과 비밀번호)
  //sendCommand("AT+CWJAP=\"spotwon\",\"12345678\"", 5000); // 와이파이 연결(이름과 비밀번호)
  //sendCommand("AT+CWJAP=\"kyong\",\"00000000\"", 5000); // 와이파이 연결(이름과 비밀번호)
  //sendCommand("AT+CWJAP=\"iphone\",\"123456789\"", 5000); // 와이파이 연결(이름과 비밀번호)
  delay(10000); // 연결될 시간을 추가로 기다림
  String cmd = "AT+CIPSTART=\"TCP\",\"192.168.0.28\",5000";
  
  Serial.println("Wi-Fi 연결 시도...");
  sendCommand(cmd, 2000); //TCP 연결
  readESPResponse();

  
  delay(5000); // 연결될 시간을 추가로 기다림

  // 1회 서버에 데이터 보내기
  string alram_name  = "김두한"; 
  unsigned int alram_time = 12:00:00;    
  int is_active=1;
  Serial.println(String(alram_name) +"," + String(alram_time) +","+String(is_active));
  sendDataToServer(alram_name, alram_time,is_active);  // 테스트 데이터 전송
  delay(10000);  // 10초마다 데이터 전송

}

void loop() 
{
  GetDataFromServer();
  delay(10000)
}

void sendDataToServer(string alram_name, unsigned int alram_time,int is_active) 
{
  // 1. HTTP 요청 데이터 생성  requset 만듦
  String postData = "alram_name=" + String(alram_name)+ "&alram_time=" + String(alram_time)+ "&is_active=" + String(is_active);
  String request = "POST /data HTTP/1.1\r\n";
  request += "와이파이Host:\r\n";
  request += "Content-Type: application/x-www-form-urlencoded\r\n";
  request += "Content-Length: " + String(postData.length()) + "\r\n";
  request += "Connection: close\r\n\r\n"; // 헤더 종료 표시
  request += postData; // 본문 추가

  // 2. TCP 연결 시작 
  //String cmd = "AT+CIPSTART=\"TCP\",\"192.168.0.126\",5000";
  // sendCommand(cmd, 2000);
  
  // 3. 데이터 전송 크기 설정
  String sendCmd = "AT+CIPSEND=" + String(request.length());
  sendCommand(sendCmd, 1000);
  readESPResponse();

  // 4. HTTP 요청 데이터 전송
  esp.print(request); // 📌 println()이 아니라 print() 사용
  delay(1000);

  // 5. 서버 응답 읽기
  Serial.println("서버 응답 대기 중...");
  long timeout = millis() + 5000;
  while (millis() < timeout) // 5초간 받을 시간 확보
  {
    while (esp.available()) 
    {
      char b = esp.read();
      Serial.write(b); //서버가 무언가 읽었다고 시리얼에 표시
    }
  }
  // 6. 연결 종료
  sendCommand("AT+CIPCLOSE", 1000);
  readESPResponse();
}

void GetDataFromServer()
{
  String request = "GET /get_alarm HTTP/1.1\r\n"; // HTTP GET 요청 라인 생성, '/get_data'는 서버의 엔드포인트
  request += "Host: 192.168.0.28\r\n";          // Host 헤더 설정 (서버의 IP 주소)
  request += "Connection: close\r\n\r\n";      // Connection 헤더 설정 (요청 후 연결을 닫음)

  String cmd = "AT+CIPSTART=\"TCP\",\"192.168.0.28\",5000"; // TCP 연결을 시작하는 AT 명령어 생성 (프로토콜, IP 주소, 포트 번호)
  sendCommand(cmd, 2000);                                   //setup 에서 연결햇지만 loop 임,  ESP-01로 TCP 연결 시작 명령어 전송
  readESPResponse();                                       /
  
  String sendCmd = "AT+CIPSEND=" + String(request.length()); // 전송할 데이터의 크기를 ESP-01에 알리는 AT 명령어 생성
  sendCommand(sendCmd, 1000);                                  // ESP-01로 데이터 전송 크기 명령어 전송
  readESPResponse();                                       // ESP-01의 응답 읽기

  esp.print(request); // HTTP GET 요청 데이터 전송 (println()이 아닌 print() 사용)
  Serial.println("서버 응답 대기 중..."); // 시리얼 모니터에 서버 응답 대기 메시지 출력

  response = readESPResponse(7000); // 서버로부터 응답을 읽어와 'response' 변수에 저장 (타임아웃 7초)
  processResponse(response);         // 받은 응답을 처리하는 함수 호출

  sendCommand("AT+CIPCLOSE", 1000); // TCP 연결을 종료하는 AT 명령어 전송
  readESPResponse();               // ESP-01의 응답 읽기


// 서버로부터 받은 응답을 처리하는 함수
void processResponse(String res) 
{
  Serial.println("DB에서 값을 받았습니다.");
  Serial.print("전체 응답: ");
  Serial.println(res);

  String alarm_name_val = "N/A"; // 기본값 "N/A" (Not Available)
  String alarm_time_val = "N/A";
  int is_active_val = -1;        // 기본값 -1 (유효하지 않은 값)

  int name_index = res.indexOf("\"alarm_name\":"); // 각 데이터 키의 시작 위치를 찾습니다
  int time_index = res.indexOf("\"alarm_time\":"); // indexOf는 특정 문자열 또는 문자가 어디에 위치하는지 찾아줌
  int active_index = res.indexOf("\"is_active\":");

  /* 이 아래는 이거 대신 작업 
  String alaram_name = res.substring(name_indx + 12, name_indx + 29); // "datetime" + 10(":") + 2(공백) 부터 19글자 (YYYY-MM-DD HH:MM:SS)
  String alaram_time = res.substring(temp + 7, temp + 11);     // "temp" + 5(":") + 2(공백) 부터 4글자 (온도 값)
  String is_active = res.substring(humi + 7, humi + 11);     // "humi" + 5(":") + 2(공백) 부터 4글자 (습도 값)
  */

  if (name_index != -1)// 응답 문자열에 "datetime"이라는 단어가 포함되어 있다면 (데이터베이스에서 값을 받았다고 판단)
  { // "alarm_name" 키를 찾았다면
    // "alarm_name": "VALUE" 에서 "VALUE"의 시작 위치를 찾습니다.
    // 키워드 ("alarm_name":) 다음의 첫 번째 따옴표(") 바로 다음 위치
    // "alarm_name":". <-- 여기서 `.` 위치를 찾기 위해 "alarm_name": (13글자) 다음의 따옴표를 찾고 (+1)
    int startNameValue = res.indexOf("\"", name_index + 13) + 1; // ex)   [0]은 { \"이 따옴표를 찾는다 [1]== 1(name_index) +13("alarm_name":)=14(") +1 하면 '기(15)'
    int endNameValue = res.indexOf("\"", startNameValue); //        ex)  기상"<-이 따옴표를 찾는다 , (17)
    alarm_name_val = res.substring(startNameValue, endNameValue); // 15이상 17미만
  }
  if (time_index != -1) 
  { 
    int startTimeValue = res.indexOf("\"", time_index + 13) + 1; // "/""를 찾고 "alarm_time": 의 길이 13
    int endTimeValue = res.indexOf("\"", startTimeValue); 
    alarm_time_val = res.substring(startTimeValue, endTimeValue);
  }
  if (active_index != -1) 
  { 
    int startActiveValue = active_index + 12; // "is_active": 의 길이 12
    int endActiveValue = res.indexOf(",", startActiveValue); 
    String is_active_temp_str = res.substring(startActiveValue, endActiveValue);
    is_active_temp_str.trim(); 
    is_active_val = is_active_temp_str.toInt();
  }

  Serial.println("alarm_name: " + alarm_name_val); 
  Serial.println("alarm_time: " + alarm_time_val); 
  Serial.println("is_active: " + String(is_active_val));
  
  else
  {
    Serial.println("유효한 데이터가 아닙니다."); // 응답에 "datetime" 키워드가 없다면 유효하지 않은 데이터로 판단
  }
}