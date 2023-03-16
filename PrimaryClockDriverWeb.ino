/*
* ​Описание.
 *
 * Программа Первичных часов.
 *  Реализован на ESP8266 (Node MCU), L298N и блок питания 24В
 *
 * К "Первичным часам" подключен драйвер для шагового двигателя Вторичных часов (управляемых по-минутно).
 * В "Первичных часах" реализован  
 *  - счетчик текущего времени и подача электро импульсов для вторичных часов
 *  - веб сервер удаленного управления\контроля Первичных часов
 *  - ПЛАН синхронизация с временем Интеренет, что требует умение определения положения стрелок, хотя бы 1 раз в 12 часов.
 *  
 *  ESP-модуль поключается к домашнему роутеру и запускает свой web-сервер.
 *  Доступ к серверу по IP, выданный домашним роутером
 *  
 *  
 *  
 *  
 
 */
 
//#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>


/*
* Если объявить переменную, Первичные часы будут работат автономно, как одиночный сервер
* Инача будет произведено подключение к Роутеру. 
*/
#define WifiPointMode 1
 

 /**
  * Если объявлена переменная, будут отобрадаться отоладочная информация
  */ 
#define DEBUG 1
 
/* * Подключение
 * Источник питания    L298N
 * +24В                Пин +12В
 * -24В                Пин GND
 *
 * В модуле L298N подключить перемычку запитывания модуля внутренним стабилизатором
 *
 * ESP8266           L298N
 * Vin               +5В
 * GND               GND
 *  D0 (GPIO16) 4    Ena
 *  D1 (GPIO5) 20    In1
 *  D2 (GPIO4) 19    In2
 */
 
 
 
 
 
// Motor A
#define motor1Pin1  D1
#define motor1Pin2  D2
#define enable1Pin  D0 //требуется аналог, т.к это pwm
 
 
/**
 * Возможные режимы работы первичных часов
 */
 #define modeClock           0 //стандартный режим часов. Ежеминутные команды на вторичные
 #define modeAdjustmentMove  1 //Режим подведения стрелок к реальному времени
 #define modeAdjustmentWait  2 //Режим ожидания, чтобы реальное время подошло к показанию стрелок
 
 
//минимальная длительность имульса на шаговый двигатель Вторичных часов, чтобы гарантировано переместить стрелку
#define PULSE_LONG 450
 

 #ifdef WifiPointMode
 
 /* Установите здесь свои SSID и пароль */
const char* ssid = "PrimaryClock";       // SSID
const char* password = "12345678";  // пароль

/* Настройки IP адреса 
Не используем, будет по умолчанию 
IPAddress local_ip(192,168,1,1);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);
*/
#else

/* Установить свой SSID и пароль */
const char* ssid = "тфьу";
const char* password = "65456";

#endif

ESP8266WebServer server(80);
 
 
 
 
/**
 * текущее время Вторичных часов в формате HH:MM
 *                Время расчетное, увеличивается каждую минуту одновременно подавая импульс
 *                на вторичные часы
 *                часы 1..12, минуты 0..59
 */
unsigned char currentHour = 0, currentMin = 0;
 
 
 

 
/**
 * При режиме
 *  modeAdjustmentMove количество импульсов, которые нужно подать на Вторичные часы, чтобы их показания скорректировать
 *                         до реального времени
 *  modeAdjustmentMoveWait количество минут, которые нужно ждать, чтобы реальное время "догнало" показания Вторичных часов
 */
int adjMinute = 0;
 
/**
 * Текущий режим работы Первичных часов
 */
unsigned char mode = 0;
 
bool direction = false;
 
 
 
 
void moveStop(){
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, LOW);
  digitalWrite(enable1Pin, LOW);
}
 
void moveDirect(){
 digitalWrite(motor1Pin1, LOW);
 digitalWrite(motor1Pin2, HIGH);  
 digitalWrite(enable1Pin, HIGH);
}
 
void moveIndirect(){
 digitalWrite(motor1Pin1, HIGH);
 digitalWrite(motor1Pin2, LOW);
 digitalWrite(enable1Pin, HIGH);
}
 
 
 
 
 
/**
 * Конфигурация драйвера двигателя вторичных часов
 */
void driverSetup(){
  #ifdef DEBUG
   Serial.println("driverSetup()");
 #endif
   pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(enable1Pin, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
}
 
 
 
unsigned long lastBall = 0;
 
unsigned long tmBeforeAdj = 0;
 
/**
 * Увеличить значения времени  Первичных часов на 1 минуту
 */
void currentTimeUp1Min(){
 
      #ifdef DEBUG
  Serial.println("void currentTimeUp1Min() "+int2strtime(currentHour)+":"+int2strtime(currentMin) );
  #endif
 
 
  currentMin++;
  if( currentMin > 59 ){
    currentMin = 0;
    currentHour++;
    if( currentHour >= 13 ) currentHour = 1;
  }
}
 
/*
 * Управление импульсом на Вторичные часы
 * Проверить, что прошла минута, если прошла выдать импульс на вторичные часы
 */
void driverProcess(){
 
 
//В режиме часов импульс каждую минуту
  if( mode == modeClock )
  if ( millis() - lastBall > (60000 - PULSE_LONG) ){
 
      digitalWrite(LED_BUILTIN, LOW);
      if( direction ) moveDirect();
      else moveIndirect();
 
      //Включить напряжение на Вторичные часы
      delay( PULSE_LONG );
       
 
       moveStop();
       digitalWrite(LED_BUILTIN, HIGH);
       direction = !direction;       
       //Выключить напряжение на Вторичные часы
       lastBall = millis();
       currentTimeUp1Min();   
  }
 
 
//В режиме подвода часов импульс без минутного ожидания
 if( mode == modeAdjustmentMove )
  if ( millis() - lastBall > PULSE_LONG ){
 
    #ifdef DEBUG
   Serial.print("(adjMinute=" + String(adjMinute) + ")  " );
  #endif      
  digitalWrite(LED_BUILTIN, LOW);
      if( direction ) moveDirect();
      else moveIndirect();
 
      //Включить напряжение на Вторичные часы
      delay( PULSE_LONG );
       
 
       moveStop();
       digitalWrite(LED_BUILTIN, HIGH);
       direction = !direction;       
         
       lastBall = millis();
       currentTimeUp1Min();
 
 
        //Следим за исполненным количеством импульсов корректировки
        adjMinute--;
        if( adjMinute <= 0 ){
 
          #ifdef DEBUG
            Serial.println("driverProcess   adjMinute="+String(adjMinute) );
          #endif  
           //Высчитаем реально затраченное на коррекцию время...
          int lo = millis() - tmBeforeAdj; tmBeforeAdj = 0;
 
           //дополнительное число импульсов для коррекции времени, учитывающее время исполнения предыдущей коррекции
          int additMinute = (int) lo / 60000;
          #ifdef DEBUG
            Serial.println("driverProcess   дополнительное число импульсов additMinute="+String(additMinute) );
          #endif  
          if( additMinute > 0 ) beginAdjClock( additMinute );
          else mode = modeClock;
          
        }   
 }
 
}
 
 
/**
 * Конфигурация веб-сервера
 */
void webSetup(){
  #ifdef DEBUG
   Serial.println("void webSetup()");
  #endif
 

  #ifdef WifiPointMode
   
    WiFi.mode(WIFI_AP);
    //По умолчанию будет 192.168.4.1
   //WiFi.softAPConfig(local_ip, gateway, subnet);
    WiFi.softAP(ssid, password);
    delay(500);

    IPAddress myIP = WiFi.softAPIP();
   	Serial.print("IP адрес первичных часов: ");
	  Serial.println(myIP);
  #else
    WiFi.begin(ssid, password);
 
    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      #ifdef DEBUG
        Serial.print(".");
      #endif  
    }
    #ifdef DEBUG
      Serial.println("");
      Serial.print("Connected to ");
      Serial.println(ssid);
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    #endif 
 
  #endif
 
  server.on("/", handle_onConnect);
  server.on("/clock", handle_Clock);
  server.on("/settime", handle_settime);
  server.on("/stop", handle_stop);
  server.on("/setadjmin", handle_adjmin);
  server.onNotFound(handle_NotFound);
  
  
  delay(500);
  server.begin(); 
  #ifdef DEBUG
   Serial.println("HTTP сервер запущен"); 
  #endif 
}
 
String int2strtime( int v ){
  String S;
  if( v < 10 ) S = "0" + String( v );
  else S = String( v );
  return( S );
}
 
 
/**
 * Формирование страницы
 */
String SendHTML( bool itsBadInputValue, bool redirectHomePage ){
 
String ptr = "<!DOCTYPE html>\n\n";
ptr +="<html>";
    ptr +="<head>";
        ptr +="<title>Первичные часы</title>";
        ptr +="<meta charset='UTF-8'>";
       ptr +="<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
 
       if( redirectHomePage ) ptr += "<meta http-equiv='Refresh' content='3;url=/clock' />";

       //В режиме перевода стрелок, важно, чтобы обновлялась страница  и был виден этап работы
       if( mode == modeAdjustmentMove) ptr += "<meta http-equiv='Refresh' content='10;url=/clock' />";
       
        ptr +="<style>";
            ptr +=".dtm{width: 50px;}";
        ptr +="</style>";

      //Нижеследующий скрипт будет на web-клиенте всегда отображать реальное время 
      ptr +="<script>";
          ptr +="function updateDtm(){";
          ptr +="var currentdate = new Date(); ";
          ptr +="var H = currentdate.getHours();";
          ptr +="var M = currentdate.getMinutes();"; 
          ptr +="if( H > 12 ) H = H -12;";
          ptr +="document.getElementById('idRealHour').value = H;";
          ptr +="document.getElementById('idRealMin').value = M;";
        ptr +="}";
 
        ptr +="function init(){";
            ptr +="setInterval(updateDtm, 1000);";
        ptr +="}";
         
      ptr +="</script>";
    ptr +="</head>";
    ptr +="<body onload='init()'>";
        ptr +="<h1>Режим часов</h1>";
        ptr +="<h2>";
        if( mode == modeClock) ptr +="Текущее время";
        if( mode == modeAdjustmentMove || mode == modeAdjustmentWait ) ptr +="Корректировка времени";
        ptr +="</h2>";
        
        // Если режим текущего времени mode = modeClock
if( mode == modeClock){        
        ptr +="Текущее время (из памяти Первичных часов) (час=1...12, мин=00..59) <B>" + int2strtime( currentHour ) + ":" + int2strtime( currentMin ) + "</B><BR>";
        ptr +="<small>Допускается, что Текущее время не соответствует реальному или показаниям на Вторичных часах. Главное, чтобы показания Вторичных часов совпадало с реальным.";
        ptr +="Если время на Вторичных часах не соответствует реальному, заполните нижеследующую форму и отправьте на исполенение.";
        ptr +="После этого Текущее время станет равным введенному.</small>";
        ptr +="<BR>";
        if( itsBadInputValue ){
          }
          
 
            ptr +="<form action='settime' method='get'>";
            ptr +="Реальное время (задает веб-баузер) (часы 1..12, минуты 0..59) ";
            ptr +="<input id='idRealHour' name='idRealHour' class='dtm' value='0' type='number' min='1' max='12'>";
            ptr += ":<input id='idRealMin' name='idRealMin' class='dtm' value='0' type='number' min='0' max='59'> ";
        
        ptr +="<BR>";
        ptr +="Время на Вторичных часах (часы 1..12, минуты 0..59) ";
            ptr +="<input id='idCurHour' name='idCurHour' class='dtm' value='" + String(currentHour) + "' type='number' min='1' max='12'>";
            ptr +=":<input id='idCurMin'  name='idCurMin' class='dtm' value='" + String(currentMin) + "' type='number' min='0' max='59'> ";
            ptr +="<input type='submit' value='Установить'>";
            ptr +="</form>";
 ptr +="<HR>";
 
 
 
            ptr +="<form action='setadjmin' method='get'>";
            ptr +="Показания Вторичных часов перевести вперед на ";
            ptr +="<input id='idAdjMin' name='idAdjMin' class='dtm' value='0' type='number' min='1' max='9999'> минут";
            ptr +="<input type='submit' value='Выполнить'>";
            ptr +="</form>";            
}
    
        
        //Если режим коррекция времени  mode = modeAdjustmentMove
if( mode == modeAdjustmentMove){        
        ptr +="Для завершении коррекции времени осталось " + String(adjMinute) + " шагов (возможно еще дополнительные коррктирующие минуты будут).";
            ptr +="<form action='stop' method='get'>";
            ptr +="<input type='submit' value='Отменить'>";
            ptr +="</form>";
}
        
    ptr +="</body>";
ptr +="</html>";
 
 
  return ptr;
}
 
 
 
void handle_onConnect() {
   #ifdef DEBUG
  Serial.println("void handle_onConnect()");
  #endif
  server.send(200, "text/html", SendHTML( false, true ));
}
 
 
 
 /**
  * Обработчик команды от человека на корректировку показания Вторичных
  * часов на заданное человеком количество минут (корректировка только "вперед")
*/ 
void handle_adjmin(){
  int adjMin    = server.arg("idAdjMin").toInt();
  #ifdef DEBUG
    Serial.println("void handle_adjmin() adjMin="+String(adjMin) );
  #endif

  if( adjMin < 0 || adjMin > 9999 ){
    adjMin = 0;
    #ifdef DEBUG
      Serial.println("Ошибочное значение корректировки минут "+ String(adjMin) );
    #endif    
    server.send(200, "text/html", SendHTML( true , false) );
    return;
  }
  
  beginAdjClock( adjMin );
 
  //Обновить страницу клиента с перенаправление страницы в режим Текущее время
  server.send(200, "text/html", SendHTML(false, true));
}
 

 
/**
* Отобразить web-страницу текущего состояния Первичных часов
*/ 
void handle_Clock() {
  #ifdef DEBUG
    Serial.println("void handle_Clock()");
  #endif
 
  server.send(200, "text/html", SendHTML( false, false ));
}
 

/*
* Обработка web-запроса несуществующей страницы
*/
void handle_NotFound(){
  #ifdef DEBUG
    Serial.println("void handle_NotFound()");
  #endif
  server.send(404, "text/plain", "Not found");
}
 
 
 
/**
 * Исполнение подвода часов на указанное число минут
 */
 void beginAdjClock( int minute ){
 
 
  #ifdef DEBUG
  Serial.println("void beginAdjClock( int "+String( minute )+" )" );
  #endif
 
 
  mode = modeAdjustmentMove;
  /**
 * Важный момент. Каждый корректирующий импульс (в режиме modeAdjustmentMove) должен выполнятся
 * в течении минимального времени (определяемый физическими воможностями шаговых двигателей Вторичных часов)
 * На выполнение задания корректировки движением показаний Вторичных часов требуется
 * время. Если на это требуется N минут, тогда количество корректирующих имульсов adjMinute нужно увеличить
 * на N
 * Для этого запоминаем время начала коррекции. По завершению корреции, высчитаем реальное
 * затраченного время на коррекцию
 */
 tmBeforeAdj = millis();
 lastBall    = tmBeforeAdj - 2 * PULSE_LONG;//сделаем так, чтобы коррекция началась сразу
 adjMinute = minute;
 
}
 
 
/**
* Обработка web-запроса корректировки времени
 * Подвести показания часов ко времени,
 * указанное пользователем
 * В параметрах от web-клиента
 *   текущее время, которое показывают вторичные часы  idCurHour, idCurMin
 *   реальное время (к которому нужно подвести показания стрелок) idRealHour, idRealMin
 */
void handle_settime() {
  #ifdef DEBUG
    Serial.println("void handle_settime()");
  #endif
 
  if( mode == modeAdjustmentMove ){
    //Повторно туже команду не исполняем
    #ifdef DEBUG
      Serial.println("Повторнно команду не исполняем");
    #endif    
    server.send(200, "text/html", SendHTML(false , false) );
    return;
  }
 
  bool itsErrDtmValue = false;
  #ifdef DEBUG
    Serial.println("void handle_settime()");
    Serial.println("Число аргументов = "+String(server.args() ) + ". Список аргументов ");
    for (int i = 0; i < server.args(); i++){
      Serial.print("Arg nº" + (String)i + " –>");
      Serial.print(server.argName(i) + ": ");
      Serial.println( server.arg(i) + "");
    }
  #endif
   
 
  //"Текущее время" - это время, переданного от человека, которое фактически показываются Вторичными часами 
  int curHour    = server.arg("idCurHour").toInt();
  int curMinute  = server.arg("idCurMin").toInt();
 
 
  //"Реальное время" - это время, переданное от человека, которое должно отображаться на Вторичных часах
  int realHour, realMin;
 

  //Если Текущее отличается от Реального, значит, Вторичные часы нужно подводить к Реальному 

  realHour    = server.arg("idRealHour").toInt();
  realMin     = server.arg("idRealMin").toInt();
 
 
 
  #ifdef DEBUG
    Serial.println(" Параметры  от клиента cur = " +  server.arg("idCurHour") + ":" + server.arg("idCurMin") +"    real=" + server.arg("idRealHour") + ":" + server.arg("idRealMin") );
  #endif
 
  //Проверка на корректность
  if( curHour < 1 || curHour > 12 ){
    curHour = 1;
    itsErrDtmValue = true;
    #ifdef DEBUG
      Serial.println("Ошибочное значение текущих часов " + String(curHour)  );
    #endif    
  }
  if( curMinute < 0 || curMinute > 59 ){
    curMinute = 0;
    itsErrDtmValue = true;
    #ifdef DEBUG
      Serial.println("Ошибочное значение текущих минут "+ String(curMinute) );
    #endif    
  }
  if( realHour < 1 || realHour > 12 ){
    realHour = 1;
    itsErrDtmValue = true;
    #ifdef DEBUG
      Serial.println("Ошибочное значение реальных часов "+ String(realHour));
    #endif    
  }
  if( realMin < 0 || realMin > 59 ){
    realMin = 0;
    itsErrDtmValue = true;
    #ifdef DEBUG
      Serial.println("Ошибочное значение реальных минут "+ String(realMin));
    #endif    
  }
 
 
  /**
  * Если пользователь ввел ошибочные значения параметров,
  * вернуть на web-клиент сообщение об этом.
  * И прекратить обработку
  */
  if( itsErrDtmValue ){
    server.send(200, "text/html", SendHTML(itsErrDtmValue , false) );
    return;
  }
 
 
 
  //Скорректируем "внутренне время Первичных часов"
  currentHour = (unsigned char) curHour;
  currentMin  = (unsigned char) curMinute;
 
  /**
  * Стратегия корректировки показания вторичных часов может быть
  *  - двигать стрелки к реальному времени
  *  - ждать, когда реальное время подойдет к показанию вторичных часов
  *  
  *  Выбрать то, что быстрее.
  *  Выбираю простой вариант, но наглядный: всегда двигать стрелки. Тогда пользователь
  *  увидит обратную связь с его командой
  */
 
  //Все переводим в минуты
  int tmpRealMin = realHour * 60 + realMin;
  int curMin  = curHour  * 60 + curMinute;


/**
* На основе стратегии "всегда передовить стрелки"
*  Если настоящее время tmpRealMin больше времени Вторичных (curMin) , 
*   то означает что Вторичные нужно подвести вперед на (tmpRealMin - curMin) минут
*  Если настоящее время tmpRealMin меньше времени Вторичных (curMin) , 
*    то означает Вторичные часы отстают на более 12 часов (здесь лучше стратегия подождать)
     значит Вторичные нжно подвести на 12 часов +  (tmpRealMin-curMin)    
*/
  int adjMinute = 0;
  if(tmpRealMin != curMin  ){
    if( tmpRealMin > curMin ){
        adjMinute = tmpRealMin - curMin;
    }else{
        adjMinute = tmpRealMin + 12*60 - curMin;
    }
 
    beginAdjClock( adjMinute );
  }else{
    #ifdef DEBUG
      Serial.println("Корректировку проводить не нужно");
    #endif
  }
 
  #ifdef DEBUG
     Serial.print("mode = ");
    Serial.println(mode);
 
    Serial.print(" adjMinute = ");
    Serial.print( adjMinute );
   
    Serial.println("");
  #endif
 
 
 
  server.send(200, "text/html", SendHTML(false, true));
}
 



/**
 * Обработчик web-запроса
 * Отменить текущий режим часов и перевести его в режим
 * по умолчанию
 */
void handle_stop() {
 
  #ifdef DEBUG
    Serial.println("void handle_stop() ");
  #endif
 
  if( mode == modeClock ){
    //Повторно туже команду не исполняем
    #ifdef DEBUG
      Serial.println("Повторнно команду не исполняем");
    #endif    
    //При исполнении команды Стоп, требуется обновление страницы в режим Часы
    server.send(200, "text/html", SendHTML(true , false) );
    return;
  }
 
 
  mode = modeClock;
  server.send(200, "text/html", SendHTML(false, true));
}
 
 
 
 /**
 *
 */
void setup() {
  #ifdef DEBUG
   Serial.begin(115200);
  #endif
 
 
  //Режим часов по умолчанию "считать текущее время с переводом вторичных часов"
  mode = modeClock;
 
  driverSetup();
  webSetup();
}
 
void loop() {
  server.handleClient();
 
  //Осчет внутреннего времени и управление Вторичными часами
  driverProcess();
 
}
