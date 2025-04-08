#include <WiFi.h>
#include "time.h"
#include <Adafruit_NeoPixel.h>

#define RESET_PIN 15
#define TOUCH_PIN 5

#define PIN_WS2812B 4
#define NUM_PIXELS 100

const char *ssid = yourSSIDhere;
const char *password = yourPASSWORDhere;

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

int brightness = 1;

bool secondsblinkon = false;

struct rgb
{
  int r;
  int g;
  int b;
};

struct hsv
{
  int h;
  int s;
  int v;
};

hsv rgb2hsv(rgb in)
{
  hsv out;
  double min, max, delta;

  min = in.r < in.g ? in.r : in.g;
  min = min < in.b ? min : in.b;

  max = in.r > in.g ? in.r : in.g;
  max = max > in.b ? max : in.b;

  out.v = max; // v
  delta = max - min;
  if (delta < 0.00001)
  {
    out.s = 0;
    out.h = 0; // undefined, maybe nan?
    return out;
  }
  if (max > 0.0)
  {                        // NOTE: if Max is == 0, this divide would cause a crash
    out.s = (delta / max); // s
  }
  else
  {
    // if max is 0, then r = g = b = 0
    // s = 0, h is undefined
    out.s = 0.0;
    out.h = 0.0; // its now undefined
    return out;
  }
  if (in.r >= max)                 // > is bogus, just keeps compilor happy
    out.h = (in.g - in.b) / delta; // between yellow & magenta
  else if (in.g >= max)
    out.h = 2.0 + (in.b - in.r) / delta; // between cyan & yellow
  else
    out.h = 4.0 + (in.r - in.g) / delta; // between magenta & cyan

  out.h *= 60.0; // degrees

  if (out.h < 0.0)
    out.h += 360.0;

  return out;
}

rgb hsv2rgb(hsv in)
{
  double hh, p, q, t, ff;
  long i;
  rgb out;

  if (in.s <= 0.0)
  { // < is bogus, just shuts up warnings
    out.r = in.v;
    out.g = in.v;
    out.b = in.v;
    return out;
  }
  hh = in.h;
  if (hh >= 360.0)
    hh = 0.0;
  hh /= 60.0;
  i = (long)hh;
  ff = hh - i;
  p = in.v * (1.0 - in.s);
  q = in.v * (1.0 - (in.s * ff));
  t = in.v * (1.0 - (in.s * (1.0 - ff)));

  switch (i)
  {
  case 0:
    out.r = in.v;
    out.g = t;
    out.b = p;
    break;
  case 1:
    out.r = q;
    out.g = in.v;
    out.b = p;
    break;
  case 2:
    out.r = p;
    out.g = in.v;
    out.b = t;
    break;

  case 3:
    out.r = p;
    out.g = q;
    out.b = in.v;
    break;
  case 4:
    out.r = t;
    out.g = p;
    out.b = in.v;
    break;
  case 5:
  default:
    out.r = in.v;
    out.g = p;
    out.b = q;
    break;
  }
  return out;
}

int color_gradient[20][3] = {
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1}};

bool numbers_test[10][5][3] = {
    {{true, true, true},
     {true, false, true},
     {true, false, true},
     {true, false, true},
     {true, true, true}},
    {{false, false, true},
     {false, false, true},
     {false, false, true},
     {false, false, true},
     {false, false, true}},
    {{true, true, true},
     {false, false, true},
     {true, true, true},
     {true, false, false},
     {true, true, true}},
    {{true, true, true},
     {false, false, true},
     {true, true, true},
     {false, false, true},
     {true, true, true}},
    {{true, false, true},
     {true, false, true},
     {true, true, true},
     {false, false, true},
     {false, false, true}},
    {{true, true, true},
     {true, false, false},
     {true, true, true},
     {false, false, true},
     {true, true, true}},
    {{true, true, true},
     {true, false, false},
     {true, true, true},
     {true, false, true},
     {true, true, true}},
    {{true, true, true},
     {false, false, true},
     {false, false, true},
     {false, false, true},
     {false, false, true}},
    {{true, true, true},
     {true, false, true},
     {true, true, true},
     {true, false, true},
     {true, true, true}},
    {{true, true, true},
     {true, false, true},
     {true, true, true},
     {false, false, true},
     {true, true, true}},
};

int matrix_old[5][5] = {
    {20, 21, 22, 23, 24},
    {19, 18, 17, 16, 15},
    {10, 11, 12, 13, 14},
    {9, 8, 7, 6, 5},
    {0, 1, 2, 3, 4}};

// Initilize the Pixels
Adafruit_NeoPixel ws2812b_leds(NUM_PIXELS, 4, NEO_GRB + NEO_KHZ800);

struct rgb firstgradientcolor
{
  11, 3, 252
};
struct rgb secondgradientcolor
{
  140, 3, 252
};

boolean summertime_EU(int year, byte month, byte day, byte hour, byte tzHours)
{
  if (month < 3 || month > 10)
    return false;
  if (month > 3 && month < 10)
    return true;
  if (month == 3 && (hour + 24 * day) >= (1 + tzHours + 24 * (31 - (5 * year / 4 + 4) % 7)) || month == 10 && (hour + 24 * day) < (1 + tzHours + 24 * (31 - (5 * year / 4 + 1) % 7)))
    return true;
  else
    return false;
}

void printnumber(int x, int number)
{
  int x_mod = x % 5;
  int num_x = x / 5;

  for (int i = 0; i < 3; i++)
  {
    for (int f = 0; f < 5; f++)
    {
      if (numbers_test[number][f][i])
      {
        uint32_t color = ws2812b_leds.Color(brightness * color_gradient[x + i][0] + 10, brightness * color_gradient[x + i][1], brightness * color_gradient[x + i][2]);
        ws2812b_leds.setPixelColor(matrix_old[f][x_mod + i] + num_x * 25, color);
      }
    }
  }
}

void generate_gradient(rgb color1, rgb color2)
{
  hsv hsvcolor1 = rgb2hsv(color1);
  hsv hsvcolor2 = rgb2hsv(color2);

  int gradient_length = hsvcolor2.h - hsvcolor1.h;
  int gradient_pieces = gradient_length / 20;

  for (int i = 0; i < 20; i++)
  {
    int result_gradient = gradient_length + i * gradient_pieces;
    if (result_gradient > 360)
    {
      result_gradient = result_gradient - 360;
    }
    else if (result_gradient < 0)
    {
      result_gradient = result_gradient + 360;
    }
    struct hsv resultcolor
    {
      result_gradient, hsvcolor1.s, hsvcolor1.v
    };
    struct rgb result_rgb = hsv2rgb(resultcolor);

    color_gradient[i][0] = result_rgb.r;
    color_gradient[i][1] = result_rgb.g;
    color_gradient[i][2] = result_rgb.b;
  }
}

void foramtedLocalTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%H:%M:%S");
}

int getseconds()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
  }
  return timeinfo.tm_sec;
}

int getminutes()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
  }
  return timeinfo.tm_min;
}

int gethours()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
  }
  return timeinfo.tm_hour;
}

void connectToWifi()
{
  // connect to WiFi
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" CONNECTED");
}

void disconnectWifi()
{
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

TaskHandle_t buttonTask;

void buttonHandler(void *parameter)
{
  pinMode(TOUCH_PIN, INPUT);
  pinMode(RESET_PIN, INPUT);

  while (1)
  {
    int TouchState = digitalRead(TOUCH_PIN);
    int ResetState = digitalRead(RESET_PIN);

    if (TouchState == HIGH)
    {
      Serial.println("Button Pressed!");
      delay(1000);
    }
    else if (ResetState == HIGH)
    {
      ESP.restart();
    }

    delay(50); // Adjust delay based on your requirements
  }
}

void setup()
{
  Serial.begin(115200);

  ws2812b_leds.begin();

  connectToWifi();

  // init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  foramtedLocalTime();

  disconnectWifi();

  generate_gradient(firstgradientcolor, secondgradientcolor);

  ws2812b_leds.clear();
  ws2812b_leds.show();

  xTaskCreatePinnedToCore(
      buttonHandler, // Task function
      "ButtonTask",  // Task name
      10000,         // Stack size (bytes)
      NULL,          // Task parameters
      1,             // Priority
      &buttonTask,   // Task handle
      0              // Core to run the task on (0 or 1)
  );
}

void loop()
{
  delay(1000);

  ws2812b_leds.clear();

  int hours = gethours();
  int seconddigithours = hours % 10;
  int firstdigithours = (hours - seconddigithours) / 10;

  printnumber(1, firstdigithours);
  printnumber(5, seconddigithours);

  int minutes = getminutes();
  int seconddigitminutes = minutes % 10;
  int firstdigitminutes = (minutes - seconddigitminutes) / 10;

  printnumber(11, firstdigitminutes);
  printnumber(15, seconddigitminutes);

  if (!secondsblinkon)
  {
    ws2812b_leds.setPixelColor(30, ws2812b_leds.Color(0, 0, 0));
    ws2812b_leds.setPixelColor(40, ws2812b_leds.Color(0, 0, 0));
    secondsblinkon = true;
  }
  else
  {
    ws2812b_leds.setPixelColor(30, ws2812b_leds.Color(brightness, brightness, brightness));
    ws2812b_leds.setPixelColor(40, ws2812b_leds.Color(brightness, brightness, brightness));
    secondsblinkon = false;
  }

  ws2812b_leds.show();
}