/**
 * A Boost basic example to connect a boost hub, set the led color and the name of the hub and
 * do some basic movements on the boost map grid
 *
 * (c) Copyright 2020 - Cornelius Munz
 * Released under MIT License
 *
 */

#include <Arduino.h>

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "Boost.h"

const char *ssid = "Karantin35.WeeksLater";
const char *password = "nais___082704291327";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// HTTP-запрос
String header;

// текущее состояние кнопки
String output26State = "off";
String output27State = "off";

// Номера выводов
const int output26 = 26;
const int output27 = 27;

// create a hub instance
Boost myMoveHub;
byte portA = (byte)MoveHubPort::A;
byte portB = (byte)MoveHubPort::B;

bool isInitialized = false;

const gpio_num_t buttons[] = {GPIO_NUM_4, GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_21};
bool buttonsState[] = {false, false, false, false};
bool buttonsStateOld[] = {false, false, false, false};
uint32_t buttonsTimers[] = {0, 0, 0, 0};

const int rotateValue = 35;

void readButtons()
{
  for (int i = 0; i < 4; i++)
  {
    bool btnState = digitalRead(buttons[i]);
    if (btnState && !buttonsState[i] && millis() - buttonsTimers[i] > 100)
    {

      buttonsState[i] = true;
      buttonsTimers[i] = millis();

      Serial.printf("%d: Button #%d Enable\n", millis(), i + 1);

      if (myMoveHub.isConnected() && isInitialized)
      {
        if (i == 0)
        {
          myMoveHub.setAbsoluteMotorPosition(portB, 100, -rotateValue, 100, BrakingStyle::BRAKE);
        }
        if (i == 1)
        {
          myMoveHub.setAbsoluteMotorPosition(portB, 100, rotateValue, 100, BrakingStyle::BRAKE);
        }
        if (i == 2)
        {
          myMoveHub.setBasicMotorSpeed(portA, 100);
        }
        if (i == 3)
        {
          myMoveHub.setBasicMotorSpeed(portA, -100);
        }
      }
    }

    if (!btnState && buttonsState[i] && millis() - buttonsTimers[i] > 100)
    {

      buttonsState[i] = false;
      buttonsTimers[i] = millis();

      Serial.printf("%d: Button #%d Disable\n", millis(), i + 1);

      if (myMoveHub.isConnected() && isInitialized)
      {
        if (i == 0 || i == 1)
        {
          myMoveHub.setAbsoluteMotorPosition(portB, 100, 0, 100, BrakingStyle::BRAKE);
        }
        if (i == 2 || i == 3)
        {
          myMoveHub.stopBasicMotor(portA);
        }
      }
    }
  }
}

void validateButtons(String type, String code)
{
  Serial.printf("code: %s type: %s\n", code.c_str(), type.c_str());

  if (code == "4")
  {
    buttonsState[3] = type == "1";
    Serial.printf("button 3 set %d\n", buttonsState[3]);
  }
  else if (code == "3")
  {
    buttonsState[2] = type == "1";
    Serial.printf("button 2 set %d\n", buttonsState[2]);
  }
  else if (code == "1")
  {
    buttonsState[0] = type == "1";
    Serial.printf("button 0 set %d\n", buttonsState[0]);
  }
  else if (code == "2")
  {
    buttonsState[1] = type == "1";
    Serial.printf("button 1 set %d\n", buttonsState[1]);
  }
}

void wsOnEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT)
  {
    // client connected
    Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
    client->printf("Hello Client %u :)", client->id());
    client->ping();
  }
  else if (type == WS_EVT_DISCONNECT)
  {
    // client disconnected
    Serial.printf("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
  }
  else if (type == WS_EVT_ERROR)
  {
    // error was received from the other end
    Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t *)arg), (char *)data);
  }
  else if (type == WS_EVT_PONG)
  {
    // pong message was received (in response to a ping request maybe)
    Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? (char *)data : "");
  }
  else if (type == WS_EVT_DATA)
  {
    // data packet
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && info->index == 0 && info->len == len)
    {
      // the whole message is in a single frame and we got all of it's data
      Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT) ? "text" : "binary", info->len);
      if (info->opcode == WS_TEXT)
      {
        data[len] = 0;
        Serial.printf("_message: %s\n", (char *)data);
    
        String message((char*)data);

        validateButtons(
          message.substring(0, message.indexOf(",")), 
          message.substring(message.indexOf(',') + 1));
      }
      else
      {
        for (size_t i = 0; i < info->len; i++)
        {
          Serial.printf("%02x ", data[i]);
        }
        Serial.printf("\n");
      }
      if (info->opcode == WS_TEXT)
        client->text("I got your text message");
      else
        client->binary("I got your binary message");
    }
    else
    {
      // message is comprised of multiple frames or the frame is split into multiple packets
      if (info->index == 0)
      {
        if (info->num == 0)
          Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
        Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT) ? "text" : "binary", info->index, info->index + len);
      if (info->message_opcode == WS_TEXT)
      {
        data[len] = 0;
        Serial.printf("%s\n", (char *)data);
      }
      else
      {
        for (size_t i = 0; i < len; i++)
        {
          Serial.printf("%02x ", data[i]);
        }
        Serial.printf("\n");
      }

      if ((info->index + len) == info->len)
      {
        Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if (info->final)
        {
          Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
          if (info->message_opcode == WS_TEXT)
            client->text("I got your text message");
          else
            client->binary("I got your binary message");
        }
      }
    }
  }
}

void setup()
{
  for (int i = 0; i < 4; i++)
  {
    pinMode(buttons[i], INPUT);
    gpio_set_pull_mode(buttons[i], GPIO_PULLDOWN_ONLY);
  }

  Serial.begin(115200);

  // Подключаемся к Wi-Fi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  // Выводим локальный IP-адрес и запускаем сервер
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/html; charset=UTF-8",
                            "<!DOCTYPE html>\
<html>\
<head>\
    <meta name='viewport' content='width=device-width, initial-scale=1'>\
    <link rel='icon' href='data:,'>\
    <style>\
        html {\
            font-family: Helvetica;\
            display: inline-block;\
            margin: 0px auto;\
            text-align: center;\
        }\
        .button {\
            background-color: #4CAF50;\
            border: none;\
            color: white;\
            padding: 16px 40px;\
            text-decoration: none;\
            font-size: 30px;\
            margin: 2px;\
            cursor: pointer;\
        }\
        .down {\
            background-color: #555555;\
        }\
        .wrapper {\
            display: grid;\
            grid-template-columns: 1fr 1fr 1fr 1fr;\
        }\
    </style>\
</head>\
<body>\
    <img style='display: block;-webkit-user-select: none;margin: auto;' src='http://192.168.0.10:8080/video' width='231' height='411'/>\
    <div class='wrapper'> \
		<button id='KeyA' class='button'>Left</button> \
		<button id='KeyD' class='button'>Right</button>\
		<button id='KeyS' class='button'>Down</button> \
		<button id='KeyW' class='button'>Up</button> \
	</div>\
    <div id='log' />\
</body>\
<script>\
    var socket = new WebSocket('ws://' + location.host + '/ws');\
    var buttons = document.getElementsByClassName('button');\
    for (var i = 0; i < buttons.length; i++) {\
        buttons.item(i).addEventListener('mousedown', function (event) { logKey(event.srcElement.id, 'keydown'); }, false);\
        buttons.item(i).addEventListener('mouseup', function (event) { logKey(event.srcElement.id, 'keyup'); }, false);\
    }\
    document.addEventListener('keydown', function (e) { logKey(e.code, e.type); });\
    document.addEventListener('keyup', function (e) { logKey(e.code, e.type); });\
    function logKey(code, type) {\
        var b = document.getElementById(code);\
        if (b == null) return;\
        if (socket.readyState == WebSocket.OPEN){\
            var data = [0,0];\
            if (type == 'keydown') data[0] = 1;\
            if (type == 'keyup') data[0] = 2;\
            if (code === 'KeyA') data[1] = 1;\
            if (code === 'KeyD') data[1] = 2;\
            if (code === 'KeyS') data[1] = 3;\
            if (code === 'KeyW') data[1] = 4;\
            socket.send(data);\
        }\
        if (type === 'keydown') {\
            b.classList.add('down');\
        }\
        else if (type === 'keyup') {\
            b.classList.remove('down');\
        }\
    }\
</script>\
</html>"); });

  server.addHandler(&ws);
  ws.onEvent(wsOnEvent);

  server.begin();
  myMoveHub.init();
}

// main loop
void loop()
{
  if (myMoveHub.isConnecting())
  {
    myMoveHub.connectHub();
    if (myMoveHub.isConnected())
    {
      Serial.println("Connected to HUB");
    }
    else
    {
      Serial.println("Failed to connect to HUB");
    }
  }

  if (myMoveHub.isConnected() && !isInitialized)
  {
    char hubName[] = "myMoveHub";
    myMoveHub.setHubName(hubName);
    myMoveHub.setLedColor(GREEN);
    delay(1000);

    isInitialized = true;
  }

  if (myMoveHub.isConnected() && isInitialized)
  {
    if (buttonsState[0] && !buttonsStateOld[0])
    {
      myMoveHub.setAbsoluteMotorPosition(portB, 100, rotateValue * -1, 100, BrakingStyle::BRAKE);
    }
    if (buttonsState[1] && !buttonsStateOld[1])
    {
      myMoveHub.setAbsoluteMotorPosition(portB, 100, rotateValue, 100, BrakingStyle::BRAKE);
    }
    if ((!buttonsState[0] && buttonsStateOld[0]) || (!buttonsState[1] && buttonsStateOld[1]))
    {
      myMoveHub.setAbsoluteMotorPosition(portB, 100, 0, 100, BrakingStyle::BRAKE);
      myMoveHub.setAbsoluteMotorPosition(portB, 100, 0, 100, BrakingStyle::BRAKE);
    }

    if (buttonsState[2] && !buttonsStateOld[2])
    {
      myMoveHub.setBasicMotorSpeed(portA, 100);
    }
    if (buttonsState[3] && !buttonsStateOld[3])
    {
      myMoveHub.setBasicMotorSpeed(portA, -100);
    }
    if ((!buttonsState[2] && buttonsStateOld[2]) || (!buttonsState[3] && buttonsStateOld[3]))
    {
      myMoveHub.stopBasicMotor(portA);
    }

    for (auto i = 0; i < 4; i++)
      buttonsStateOld[i] = buttonsState[i];
  }
} // End of loop