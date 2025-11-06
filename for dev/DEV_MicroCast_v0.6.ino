/*
 * MicroCast v0.6 for M5Cardputer — Client (Wi-Fi onboarding + Proxy)
 * 
 * Features:
 *   - Username registration system (unique names, bound to device_id)
 *   - Recovery Code system - restore account after device reset
 *   - 2 posts per screen with scrolling
 *   - Like/unlike posts (toggle)
 *   - Comments view with scrollable text (3 comments visible)
 *   - Modern UI with borders, hearts, WiFi indicator
 *   - Auto screen sleep after 60s of inactivity (power saving)
 *   - Battery level indicator with color coding (green/yellow/red)
 *   - Push notifications for new posts (sound + RGB LED)
 *   - Smart notification: sound only on new posts, LED blinks until read
 *   - Settings menu in YOU section (S key)
 *   - Configurable notification interval (5-60 min)
 * 
 * Controls:
 *   Feed: ; or . (arrows) or W/A - scroll (2 posts visible)
 *         Enter - toggle like
 *         C - view comments
 *         Fn+Enter (or Tab+Enter) - new post
 *         R - refresh feed & stats (marks posts as read)
 *         Opt+R - show recovery code
 *         U - change username
 *         N - add Wi-Fi network (doesn't disconnect)
 *         S - settings (in YOU section)
 *   Comments: ; . (up/down) - select comment, , / (left/right) - scroll text
 *             Fn+Enter - write comment, ESC - back to feed
 *   Text Input: Enter - submit, Esc - cancel, Del/Backspace - delete char
 *               All printable chars including . and ; are now allowed
 *   WiFi Select: ; . (arrows) or W/A - navigate, Enter - select, R - rescan, Esc - cancel
 *   Screen: Auto-sleep after 60s, any key to wake up
 *   Device: Fn+Opt+Del - clear device (username, WiFi, settings) - requires confirmation
 *   Recovery: Opt+R - show recovery code (save it to restore account)
 */

#include <M5Cardputer.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <FastLED.h>
#include "qrcode.h"

// ====== INSERT YOUR CLOUDFLARE WORKER URL HERE (NO TRAILING SLASH) ======
const char* PROXY_BASE = "https://shrill-bush-ae79.soulstream2012.workers.dev";  // <--- CHANGE THIS

// ====== FALLBACK keycodes ======
#ifndef KEY_ESC
  #define KEY_ESC 0x29
#endif
#ifndef KEY_ENTER
  #define KEY_ENTER 0x28
#endif
#ifndef KEY_BACKSPACE
  #define KEY_BACKSPACE 0x2A
#endif
#ifndef KEY_ARROW_UP
  #define KEY_ARROW_UP 0x52
#endif
#ifndef KEY_ARROW_DOWN
  #define KEY_ARROW_DOWN 0x51
#endif
#ifndef KEY_ARROW_LEFT
  #define KEY_ARROW_LEFT 0x50
#endif
#ifndef KEY_ARROW_RIGHT
  #define KEY_ARROW_RIGHT 0x4F
#endif
#ifndef KEY_TAB
  #define KEY_TAB 0x2B
#endif

// ====== STATE ======
bool wifiOK = false;
String deviceId;
String author;
String recoveryCode; // Recovery code для восстановления аккаунта
Preferences prefs;
int deviceCount = 0; // total registered devices

// ====== RGB LED (WS2812B on GPIO21) ======
#define LED_PIN 21
#define NUM_LEDS 1
CRGB leds[NUM_LEDS];
bool ledBlinkState = false;
unsigned long lastLEDBlinkTime = 0;
const unsigned long LED_BLINK_INTERVAL = 10000; // мигание каждые 10 секунд

// ====== BATTERY ======
int batteryLevel = 100; // процент заряда 0-100
unsigned long lastBatteryCheck = 0; // время последней проверки батареи
const unsigned long BATTERY_CHECK_INTERVAL = 30000; // проверка каждые 30 секунд

// ====== NOTIFICATIONS ======
bool notificationsEnabled = true; // включены ли уведомления
int notificationInterval = 10; // интервал проверки в минутах
bool notificationSound = true; // звуковое уведомление
bool notificationLED = true; // LED уведомление
unsigned long lastNotificationCheck = 0; // время последней проверки
String lastSeenPostId = ""; // ID последнего просмотренного поста
int lastNewPostCount = 0; // количество новых постов при последнем уведомлении
bool hasUnreadPosts = false; // есть ли непрочитанные посты
bool showingSettings = false; // показывать окно настроек
int settingsMenuSelection = 0; // 0=Notifications, 1=WiFi List
bool inSettingsSubmenu = false; // внутри подменю настроек
int wifiListSelection = 0; // выбранная сеть в WiFi List
bool showingDonateQR = false; // показ окна доната

// ====== WIFI NETWORKS ======
#define MAX_WIFI_NETWORKS 5
struct WiFiNetwork {
  String ssid;
  String pass;
};
WiFiNetwork savedNetworks[MAX_WIFI_NETWORKS];
int savedNetworksCount = 0;

// ====== SCREEN POWER SAVING ======
unsigned long lastActivityTime = 0; // время последней активности
const unsigned long SCREEN_TIMEOUT = 60000; // 60 секунд бездействия
bool screenOn = true; // состояние экрана

struct Post { String id, author, text, created_at; int likes = 0; int comments = 0; };
static const size_t FEED_MAX = 50;
Post FEED[FEED_MAX];
size_t feedCount = 0;
int sel = 0; // selected post index in FEED
int scrollOffset = 0; // scroll offset for displaying 2 posts

// ====== COMMENTS ======
struct Comment { String id, author, text, created_at; };
static const size_t COMMENTS_MAX = 20;
Comment COMMENTS[COMMENTS_MAX];
size_t commentCount = 0;
int commentSel = 0; // выбранный комментарий (вертикальная позиция)
int commentScrollOffset = 0; // вертикальный scroll для списка комментариев
int commentTextScroll = 0; // горизонтальный scroll для текста выбранного комментария
String currentPostId = ""; // ID поста для которого открыты комментарии
bool viewingComments = false; // просмотр комментариев

// ====== SECTIONS ======
enum Section { SECTION_NEW, SECTION_TOP, SECTION_YOU, SECTION_SOON };
Section currentSection = SECTION_NEW;
bool isInSectionSelector = false; // true когда на панели выбора разделов
int sectionSel = 0; // 0=NEW, 1=TOP, 2=YOU, 3=SOON
bool isInHeaderSelector = false; // выбор заголовка MicroCast
bool showingInfo = false; // показывать Info окно
bool isLoading = false; // индикатор загрузки

// Profile data (для YOU раздела)
struct Profile {
  String author;
  int totalLikes;
  int postCount;
} profile;

// ====== Screen Power Management ======
void screenSleep() {
  if (screenOn) {
    M5Cardputer.Display.setBrightness(0);
    screenOn = false;
  }
}

void screenWake() {
  if (!screenOn) {
    M5Cardputer.Display.setBrightness(255);
    screenOn = true;
    // Перерисовываем правильный интерфейс в зависимости от состояния
    if (viewingComments) {
      drawCommentsView();
    } else if (showingInfo) {
      drawInfoPopup();
    } else {
      drawUI();
    }
  }
  lastActivityTime = millis();
}

void updateActivity() {
  lastActivityTime = millis();
  if (!screenOn) {
    screenWake();
  }
}

// ====== LED functions ======
void initLED() {
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(50); // яркость 0-255
  leds[0] = CRGB::Black;
  FastLED.show();
}

void setLED(uint8_t r, uint8_t g, uint8_t b) {
  leds[0] = CRGB(r, g, b);
  FastLED.show();
}

void ledOff() {
  leds[0] = CRGB::Black;
  FastLED.show();
}

// Обновление мигания LED для уведомлений
void updateNotificationLED() {
  if (!hasUnreadPosts || !notificationLED) {
    // Нет непрочитанных постов или LED уведомления выключены
    if (ledBlinkState) {
      ledOff();
      ledBlinkState = false;
    }
    return;
  }
  
  // Мигаем каждые 10 секунд
  if (millis() - lastLEDBlinkTime >= LED_BLINK_INTERVAL) {
    if (ledBlinkState) {
      // Выключаем
      ledOff();
      ledBlinkState = false;
      lastLEDBlinkTime = millis();
    } else {
      // Включаем зеленый на 2 секунды
      setLED(0, 255, 0); // зеленый
      ledBlinkState = true;
      lastLEDBlinkTime = millis() - LED_BLINK_INTERVAL + 2000; // выключим через 2000ms (2 сек)
    }
  }
}

// ====== BATTERY functions ======
void updateBatteryLevel() {
  // Обновляем уровень батареи только если прошло достаточно времени
  if (millis() - lastBatteryCheck > BATTERY_CHECK_INTERVAL) {
    batteryLevel = M5Cardputer.Power.getBatteryLevel();
    // Ограничиваем значение 0-100
    if (batteryLevel < 0) batteryLevel = 0;
    if (batteryLevel > 100) batteryLevel = 100;
    lastBatteryCheck = millis();
  }
}

// Рисование badge (красный кружок с цифрой) для уведомлений
void drawNotificationBadge(int x, int y, int count) {
  auto& d = M5Cardputer.Display;
  
  // Красный круг
  int radius = 7;
  d.fillCircle(x, y, radius, 0xF800); // красный
  
  // Белая цифра внутри
  d.setTextColor(0xFFFF, 0xF800); // белый на красном
  d.setTextSize(1);
  
  // Центрируем цифру
  if (count < 10) {
    d.setCursor(x - 3, y - 3);
  } else if (count < 100) {
    d.setCursor(x - 6, y - 3);
  } else {
    // Если больше 99, показываем "99+"
    d.setCursor(x - 6, y - 3);
    d.print("99");
    d.setTextColor(0xFFFF, 0x0000); // возвращаем цвет
    return;
  }
  
  d.print(count);
  d.setTextColor(0xFFFF, 0x0000); // возвращаем цвет
}

// Рисование индикатора батареи с процентом
void drawBatteryIndicator(int x, int y) {
  auto& d = M5Cardputer.Display;
  
  // Определяем цвет по уровню заряда
  uint16_t color;
  if (batteryLevel > 50) {
    color = 0x07E0; // зеленый
  } else if (batteryLevel > 20) {
    color = 0xFFE0; // желтый
  } else {
    color = 0xF800; // красный
  }
  
  // Рисуем корпус батареи (прямоугольник)
  int battW = 20; // ширина батареи
  int battH = 9;  // высота батареи
  d.drawRect(x, y, battW, battH, color);
  
  // Рисуем "носик" батареи справа
  d.fillRect(x + battW, y + 2, 2, 4, color);
  
  // Заливка батареи по уровню заряда
  int fillWidth = (battW - 4) * batteryLevel / 100;
  if (fillWidth > 0) {
    d.fillRect(x + 2, y + 2, fillWidth, battH - 4, color);
  }
  
  // Процент заряда справа от иконки
  d.setCursor(x + battW + 4, y);
  d.setTextColor(color, 0x0000);
  d.print(batteryLevel);
  d.print("%");
}

// ====== helpers ======
String asciiOnly(const String& s, size_t maxLen) {
  String o; o.reserve(s.length());
  for (size_t i=0; i<s.length() && o.length()<maxLen; ++i) {
    char c = s[i]; o += (c>=32 && c<=126) ? c : '?';
  }
  return o;
}
void printLine(const String& s, uint16_t color) {
  M5Cardputer.Display.setTextColor(color, 0x0000);
  for (size_t i=0;i<s.length();++i){ char c=s[i]; M5Cardputer.Display.print((c>=32&&c<=126)?c:'.'); }
  M5Cardputer.Display.print("\n");
}
void header(const char* title="MicroCast"){
  auto& d=M5Cardputer.Display; d.fillScreen(0x0000); d.setCursor(0,0); d.setTextSize(1);
  printLine(title, 0x07FF);
  printLine("========================", 0xFFFF);
}
String maskStars(size_t n){ String s; s.reserve(n); for(size_t i=0;i<n;++i) s+='*'; return s; }

// ====== New UI helpers ======
void drawBox(int x, int y, int w, int h, uint16_t color) {
  auto& d = M5Cardputer.Display;
  d.drawRect(x, y, w, h, color);
}

// Красивое окно с сообщением
// type: 0=info, 1=success, 2=error, 3=warning
void showMessage(const String& title, const String& line1, const String& line2 = "", const String& line3 = "", int type = 0) {
  auto& d = M5Cardputer.Display;
  d.fillScreen(0x0000);
  d.setTextSize(1);
  
  // Определяем цвет по типу
  uint16_t boxColor, titleColor;
  switch(type) {
    case 1: boxColor = 0x07E0; titleColor = 0x07E0; break; // success - зеленый
    case 2: boxColor = 0xF800; titleColor = 0xF800; break; // error - красный
    case 3: boxColor = 0xFFE0; titleColor = 0xFFE0; break; // warning - желтый
    default: boxColor = 0x07FF; titleColor = 0x07FF; break; // info - cyan
  }
  
  // Заголовок
  d.setCursor(0, 0);
  d.setTextColor(titleColor, 0x0000);
  d.println(title);
  d.setTextColor(0xFFFF, 0x0000);
  d.println("----------------------------------------");
  
  // Рамка с сообщением
  int boxHeight = 60;
  if (line3.length() > 0) boxHeight = 70;
  else if (line2.length() == 0) boxHeight = 30;
  
  drawBox(4, 25, 232, boxHeight, boxColor);
  
  // Текст внутри
  int yPos = 32;
  d.setCursor(8, yPos);
  d.setTextColor(0xFFFF, 0x0000);
  d.println(line1);
  
  if (line2.length() > 0) {
    yPos += 12;
    d.setCursor(8, yPos);
    d.setTextColor(0xFFFF, 0x0000);
    d.println(line2);
  }
  
  if (line3.length() > 0) {
    yPos += 12;
    d.setCursor(8, yPos);
    d.setTextColor(0xFFFF, 0x0000);
    d.println(line3);
  }
  
  // Подсказка
  d.setCursor(0, 110);
  d.setTextColor(0x8410, 0x0000);
  d.println("Press any key to continue...");
}

// Рисование маленького сердечка
void drawHeart(int x, int y, uint16_t color) {
  auto& d = M5Cardputer.Display;
  // Верхняя часть (два круга)
  d.fillCircle(x-2, y, 2, color);
  d.fillCircle(x+2, y, 2, color);
  // Нижняя часть (треугольник)
  d.fillTriangle(x-4, y+1, x+4, y+1, x, y+6, color);
}

// Рисование индикатора Wi-Fi (кружок)
void drawWiFiIndicator(int x, int y, bool connected) {
  auto& d = M5Cardputer.Display;
  uint16_t color = connected ? 0x07E0 : 0xF800; // зеленый или красный
  d.fillCircle(x, y, 3, color);
}

// Рисование иконки комментария (речевой пузырь)
void drawCommentBubble(int x, int y, uint16_t color) {
  auto& d = M5Cardputer.Display;
  // Прямоугольник (основа пузыря)
  d.drawRect(x, y, 6, 5, color);
  // Хвостик (маленький треугольник снизу слева)
  d.drawPixel(x, y+5, color);
  d.drawPixel(x-1, y+6, color);
}
String wrapText(const String& text, size_t maxWidth) {
  // простейшая функция переноса текста
  String result = "";
  size_t pos = 0;
  while (pos < text.length()) {
    size_t end = min(pos + maxWidth, text.length());
    if (end < text.length() && text[end] != ' ') {
      // попытка найти пробел для разрыва
      size_t space = text.lastIndexOf(' ', end);
      if (space > pos && space < end) end = space;
    }
    result += text.substring(pos, end);
    if (end < text.length()) result += "\n";
    pos = end;
    if (pos < text.length() && text[pos] == ' ') pos++; // пропуск пробела
  }
  return result;
}

bool keyUpPressed(){
  // Стрелка вверх: ; (символ на клавише с оранжевой стрелкой)
  return M5Cardputer.Keyboard.isKeyPressed(';');
}

bool keyDownPressed(){
  // Стрелка вниз: . (символ на клавише с оранжевой стрелкой)
  return M5Cardputer.Keyboard.isKeyPressed('.');
}

bool keyLeftPressed(){
  // Стрелка влево: , (запятая на клавише с оранжевой стрелкой)
  return M5Cardputer.Keyboard.isKeyPressed(',');
}

bool keyRightPressed(){
  // Стрелка вправо: / (слэш на клавише с оранжевой стрелкой)
  return M5Cardputer.Keyboard.isKeyPressed('/');
}

// ====== Функция подтверждения действия ======
// Возвращает: 0 = cancel, 1 = confirm, 2 = edit
int confirmAction(const String& text, const String& actionName) {
  auto& d = M5Cardputer.Display;
  d.fillScreen(0x0000);
  d.setTextSize(1);
  
  // Заголовок
  d.setCursor(0, 0);
  d.setTextColor(0x07FF, 0x0000); // cyan
  d.println("Confirm");
  d.setTextColor(0xFFFF, 0x0000);
  d.println("----------------------------------------");
  
  // Название действия
  d.setCursor(4, 18);
  d.setTextColor(0xFFE0, 0x0000); // yellow
  d.println(actionName + ":");
  
  // Рамка для preview текста
  drawBox(4, 32, 232, 50, 0x07E0); // зеленая рамка
  
  // Текст (многострочный, если длинный)
  d.setCursor(8, 36);
  d.setTextColor(0xFFFF, 0x0000);
  
  // Разбиваем на строки по 38 символов
  String displayText = text;
  int charsPerLine = 38;
  int lineY = 36;
  for (int i = 0; i < displayText.length() && lineY < 76; i += charsPerLine) {
    String line = displayText.substring(i, min((int)displayText.length(), i + charsPerLine));
    d.setCursor(8, lineY);
    d.print(line);
    lineY += 8;
  }
  
  // Вопрос
  d.setCursor(4, 90);
  d.setTextColor(0xFFE0, 0x0000);
  d.println("Send this?");
  
  // Подсказки по управлению
  d.setCursor(0, 105);
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[Enter]");
  d.setTextColor(0x8410, 0x0000);
  d.print(" Yes  ");
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[E]");
  d.setTextColor(0x8410, 0x0000);
  d.print(" Edit  ");
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[`]");
  d.setTextColor(0x8410, 0x0000);
  d.print(" Cancel");
  
  // Ожидание ответа
  while (true) {
    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
      // Обновляем таймер активности при каждом нажатии
      lastActivityTime = millis();
      
      if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
        return 1; // confirm
      }
      if (M5Cardputer.Keyboard.isKeyPressed('E') || M5Cardputer.Keyboard.isKeyPressed('e')) {
        return 2; // edit
      }
      if (M5Cardputer.Keyboard.isKeyPressed(KEY_ESC) || M5Cardputer.Keyboard.isKeyPressed('`')) {
        return 0; // cancel
      }
    }
    delay(10);
  }
}

String promptInput(const String& title, size_t maxLen, bool mask=false, const String& initialText="") {
  auto& d = M5Cardputer.Display;
  d.fillScreen(0x0000);
  d.setTextSize(1);
  
  // Заголовок с иконкой
  d.setCursor(0, 0);
  d.setTextColor(0x07FF, 0x0000); // cyan
  d.println("Input");
  d.setTextColor(0xFFFF, 0x0000);
  d.println("----------------------------------------");
  
  // Название поля
  d.setCursor(4, 18);
  d.setTextColor(0xFFE0, 0x0000); // yellow
  d.println(title);
  
  // Рамка для ввода
  drawBox(4, 32, 232, 24, 0x07E0); // зеленая рамка
  
  // Подсказка по управлению
  d.setCursor(0, 100);
  d.setTextColor(0x8410, 0x0000); // gray
  d.println("Controls:");
  d.setCursor(0, 108);
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[Enter]");
  d.setTextColor(0x8410, 0x0000);
  d.print(" OK  ");
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[`]");
  d.setTextColor(0x8410, 0x0000);
  d.print(" Cancel");
  d.setCursor(0, 118);
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[Fn+,/]");
  d.setTextColor(0x8410, 0x0000);
  d.print(" Move  ");
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[Del]");
  d.setTextColor(0x8410, 0x0000);
  d.print(" Erase");
  
  String s = initialText;  // Начальный текст (для редактирования)
  int cursorPos = s.length(); // Курсор в конце начального текста
  bool cancelled = false;
  
  // Начальная отрисовка текста (если есть initialText)
  if (s.length() > 0) {
    d.fillRect(6, 34, 228, 20, 0x0000);
    d.setCursor(8, 38);
    d.setTextColor(0xFFFF, 0x0000);
    String displayText = mask ? maskStars(s.length()) : s;
    if (displayText.length() > 38) {
      int scrollOffset = max(0, (int)displayText.length() - 38);
      displayText = displayText.substring(scrollOffset);
    }
    d.print(displayText);
  }
  
  while (true) {
    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
      // Обновляем таймер активности при каждом нажатии
      lastActivityTime = millis();
      
      if (M5Cardputer.Keyboard.isKeyPressed(KEY_ESC) || M5Cardputer.Keyboard.isKeyPressed('`')) { 
        cancelled = true;
        break; 
      }
      
      Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
      if (status.enter) break;
      
      // Навигация курсором ТОЛЬКО с Fn
      if (M5Cardputer.Keyboard.isKeyPressed(KEY_FN)) {
        if (M5Cardputer.Keyboard.isKeyPressed(',')) {
          cursorPos = max(0, cursorPos - 1);
        } else if (M5Cardputer.Keyboard.isKeyPressed('/')) {
          cursorPos = min((int)s.length(), cursorPos + 1);
        }
      }
      // Del/Backspace - удаление символа перед курсором
      else if (status.del && cursorPos > 0) {
        s.remove(cursorPos - 1, 1);
        cursorPos--;
      }
      // Добавление символов в позицию курсора
      else if (!M5Cardputer.Keyboard.isKeyPressed(KEY_FN)) {
        for (char c : status.word) {
          // Разрешены все печатные символы, включая . и ;
          // Фильтруем только служебные: ESC и backtick
          if (c != 0x1B && c != '`' &&
              c >= 32 && c <= 126 && s.length() < maxLen) {
            s = s.substring(0, cursorPos) + String(c) + s.substring(cursorPos);
            cursorPos++;
          }
        }
      }
      
      // Отрисовка текста в рамке
      d.fillRect(6, 34, 228, 20, 0x0000); // очистка внутри рамки
      d.setCursor(8, 38);
      d.setTextColor(0xFFFF, 0x0000);
      
      String displayText = mask ? maskStars(s.length()) : s;
      int scrollOffset = 0;
      
      // Прокрутка если текст длинный
      if (displayText.length() > 38) {
        if (cursorPos > 35) {
          scrollOffset = cursorPos - 35;
        }
        displayText = displayText.substring(scrollOffset, min((int)displayText.length(), scrollOffset + 38));
      }
      
      // Рисуем текст с курсором
      for (int i = 0; i < displayText.length(); i++) {
        int realPos = i + scrollOffset;
        if (realPos == cursorPos && (millis() / 500) % 2 == 0) {
          // Мигающий курсор в текущей позиции (инвертированный символ)
          d.setTextColor(0x0000, 0x07E0);
          d.print(displayText[i]);
          d.setTextColor(0xFFFF, 0x0000);
        } else {
          d.print(displayText[i]);
        }
      }
      
      // Курсор в конце строки
      if (cursorPos >= s.length() && (millis() / 500) % 2 == 0) {
        d.print("_");
      }
    }
    delay(8);
  }
  
  if (cancelled) return "";
  return asciiOnly(s, maxLen);
}

// ====== Donate QR Popup ======
void drawDonateQRPopup() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(0x0000);
  d.setTextSize(1);
  d.setCursor(100, 3);
  d.setTextColor(0x07FF, 0x0000);
  d.println("Donate");

  // avoid any automatic wrapping
  d.setTextWrap(false);

  // Чуть поднимаем контент, добавляем зазор от рамки
  int contentY = 16;
  int contentH = 108;
  // Левая колонка строго квадратная под QR, сдвинута влево, но с отступом от рамки
  int frameX = 2;
  int leftX = frameX + 4; // 4px отступ от рамки слева
  int leftBox = contentH; // максимум по высоте для более крупного QR
  drawBox(frameX, contentY-2, 236, contentH+4, 0x07FF);

  const char* url = "https://ko-fi.com/kotov";
  uint8_t version = 3;
  uint8_t ecc = ECC_QUARTILE;
  QRCode qr;
  uint8_t buf[qrcode_getBufferSize(version)];
  qrcode_initText(&qr, buf, version, ecc, url);

  int size = qr.size;
  // Тихая зона 3 модуля (компромисс между стандартом и размером)
  int qz = 3; // quiet zone в модулях
  int scale = (leftBox) / (size + 2 * qz); // целочисленный масштаб по полной ширине
  if (scale < 1) scale = 1;

  int qrPix = size * scale;               // пиксели самой матрицы
  int totalPix = (size + 2 * qz) * scale; // матрица + тихая зона
  // draw white square background exactly leftBox x leftBox
  d.fillRect(leftX, contentY, leftBox, leftBox, 0xFFFF);
  // Центрируем общий квадрат (включая тихую зону), а модули сдвигаем на qz*scale
  int originX = leftX + (leftBox - totalPix) / 2;
  int originY = contentY + (leftBox - totalPix) / 2;
  int startX = originX + qz * scale;
  int startY = originY + qz * scale;
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      if (qrcode_getModule(&qr, x, y)) {
        d.fillRect(startX + x * scale, startY + y * scale, scale, scale, 0x0000);
      }
    }
  }

  // Правая колонка текста — после квадрата QR, с фиксированным отступом
  int textX = leftX + leftBox + 5;
  int textY = contentY + 2;
  d.setCursor(textX, textY);
  d.setTextColor(0xFFE0, 0x0000);
  d.println("Support development");
  d.setCursor(textX, textY + 12);
  d.setTextColor(0xFFFF, 0x0000);
  d.println("Scan QR on the left");

  int lineBase = textY + 54;
  d.setCursor(textX, lineBase);
  d.setTextColor(0x07FF, 0x0000); // Telegram label: cyan
  d.println("Telegram:");
  d.setCursor(textX, lineBase + 10);
  d.setTextColor(0xFFFF, 0x0000); // handle: white
  d.println("@microcast_app");
  d.setCursor(textX, lineBase + 20);
  d.setTextColor(0xFD20, 0x0000); // Reddit label: orange
  d.println("Reddit:");
  d.setCursor(textX, lineBase + 30);
  d.setTextColor(0xFFFF, 0x0000); // handle: white
  d.println("r/MicroCast");

  d.setCursor(55, 125);
  d.setTextColor(0x8410, 0x0000);
  d.print("Press any key to close");
}

// ====== WiFi Networks Management ======
// Forward declaration
bool tryConnectWiFi(const String& ssid, const String& pass, int tries=30);

void loadSavedNetworks() {
  prefs.begin("microcast", false);
  savedNetworksCount = prefs.getInt("wifi_count", 0);
  if (savedNetworksCount > MAX_WIFI_NETWORKS) savedNetworksCount = MAX_WIFI_NETWORKS;
  
  for (int i = 0; i < savedNetworksCount; i++) {
    String key_ssid = "wifi_ssid_" + String(i);
    String key_pass = "wifi_pass_" + String(i);
    savedNetworks[i].ssid = prefs.getString(key_ssid.c_str(), "");
    savedNetworks[i].pass = prefs.getString(key_pass.c_str(), "");
  }
  prefs.end();
}

void saveNetworks() {
  prefs.begin("microcast", false);
  prefs.putInt("wifi_count", savedNetworksCount);
  
  for (int i = 0; i < savedNetworksCount; i++) {
    String key_ssid = "wifi_ssid_" + String(i);
    String key_pass = "wifi_pass_" + String(i);
    prefs.putString(key_ssid.c_str(), savedNetworks[i].ssid);
    prefs.putString(key_pass.c_str(), savedNetworks[i].pass);
  }
  prefs.end();
}

bool addNetwork(const String& ssid, const String& pass) {
  // Проверяем, не сохранена ли уже эта сеть
  for (int i = 0; i < savedNetworksCount; i++) {
    if (savedNetworks[i].ssid == ssid) {
      // Обновляем пароль
      savedNetworks[i].pass = pass;
      saveNetworks();
      return true;
    }
  }
  
  // Добавляем новую сеть
  if (savedNetworksCount < MAX_WIFI_NETWORKS) {
    savedNetworks[savedNetworksCount].ssid = ssid;
    savedNetworks[savedNetworksCount].pass = pass;
    savedNetworksCount++;
    saveNetworks();
    return true;
  }
  
  // Список полон - заменяем последнюю
  savedNetworks[MAX_WIFI_NETWORKS - 1].ssid = ssid;
  savedNetworks[MAX_WIFI_NETWORKS - 1].pass = pass;
  saveNetworks();
  return true;
}

bool tryConnectToSavedNetworks() {
  // Сканируем доступные сети
  int n = WiFi.scanNetworks();
  if (n == 0) return false;
  
  // Проверяем каждую сохраненную сеть
  for (int i = 0; i < savedNetworksCount; i++) {
    // Ищем эту сеть среди доступных
    for (int j = 0; j < n; j++) {
      if (WiFi.SSID(j) == savedNetworks[i].ssid) {
        // Нашли! Пробуем подключиться
        if (tryConnectWiFi(savedNetworks[i].ssid, savedNetworks[i].pass)) {
          return true;
        }
      }
    }
  }
  
  return false;
}

// ====== WiFi ======
int scanAndSelectNetwork(String& ssid, bool& openNet) {
  auto& d = M5Cardputer.Display;
  
  // Scanning screen
  d.fillScreen(0x0000);
  d.setTextSize(1);
  
  // Заголовок
  d.setCursor(0, 0);
  d.setTextColor(0x07FF, 0x0000);
  d.println("WiFi Setup");
  d.setTextColor(0xFFFF, 0x0000);
  d.println("----------------------------------------");
  
  drawBox(4, 20, 232, 40, 0x07E0);
  d.setCursor(8, 30);
  d.setTextColor(0xFFE0, 0x0000);
  d.println("  Scanning networks...");
  d.setCursor(8, 42);
  d.setTextColor(0x8410, 0x0000);
  d.println("  Please wait");
  
  int n = WiFi.scanNetworks();
  
  if (n <= 0) {
    d.fillRect(6, 22, 228, 36, 0x0000);
    d.setCursor(8, 32);
    d.setTextColor(0xF800, 0x0000);
    d.println("  No networks found!");
    delay(1500);
    return -1;
  }
  
  int idx = 0;
  while (true) {
    d.fillScreen(0x0000);
    d.setTextSize(1);
    
    // Заголовок
    d.setCursor(0, 0);
    d.setTextColor(0x07FF, 0x0000);
    d.print("WiFi Networks [");
    d.setTextColor(0xF800, 0x0000);
    d.print(n);
    d.setTextColor(0x07FF, 0x0000);
    d.println("]");
    d.setTextColor(0xFFFF, 0x0000);
    d.println("----------------------------------------");
    
    // Список сетей (показываем до 6 штук)
    int start = max(0, idx - 3);
    int end = min(n, start + 6);
    int yPos = 18;
    
    for (int i = start; i < end; ++i) {
      bool selected = (i == idx);
      uint16_t boxColor = selected ? 0x07E0 : 0x8410;
      
      // Рамка для сети
      drawBox(4, yPos, 232, 14, boxColor);
      
      d.setCursor(8, yPos + 3);
      d.setTextColor(selected ? 0x07FF : 0xFFFF, 0x0000);
      
      // SSID (обрезаем если длинный)
      String ssid = WiFi.SSID(i);
      if (ssid.length() > 22) ssid = ssid.substring(0, 22);
      d.print(ssid);
      
      // Lock icon для защищенных сетей
      if (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) {
        d.setCursor(180, yPos + 3);
        d.setTextColor(0xFFE0, 0x0000);
        d.print("[L]");
      }
      
      // RSSI
      d.setCursor(204, yPos + 3);
      int rssi = WiFi.RSSI(i);
      uint16_t rssiColor = rssi > -60 ? 0x07E0 : (rssi > -75 ? 0xFFE0 : 0xF800);
      d.setTextColor(rssiColor, 0x0000);
      d.print(rssi);
      
      yPos += 16;
    }
    
    // Подсказки управления
    d.setCursor(0, 116);
    d.setTextColor(0xFFE0, 0x0000);
    d.print("[W/A]"); 
    d.setTextColor(0x8410, 0x0000);
    d.print(" Nav ");
    d.setTextColor(0xFFE0, 0x0000);
    d.print("[Enter]");
    d.setTextColor(0x8410, 0x0000);
    d.print(" OK ");
    d.setTextColor(0xFFE0, 0x0000);
    d.print("[R]");
    d.setTextColor(0x8410, 0x0000);
    d.print(" Scan ");
    d.setTextColor(0xFFE0, 0x0000);
    d.print("[`]");
    d.setTextColor(0x8410, 0x0000);
    d.print(" Exit");
    
    // Input
    for (;;) {
      M5Cardputer.update();
      if (!(M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed())) {
        delay(10);
        continue;
      }
      // Обновляем таймер активности при каждом нажатии
      lastActivityTime = millis();
      
      if (keyUpPressed()) {
        idx = (idx > 0) ? idx - 1 : 0;
        break;
      }
      if (keyDownPressed()) {
        idx = (idx < n - 1) ? idx + 1 : n - 1;
        break;
      }
      if (M5Cardputer.Keyboard.isKeyPressed('R') || M5Cardputer.Keyboard.isKeyPressed('r')) {
        return scanAndSelectNetwork(ssid, openNet);
      }
      if (M5Cardputer.Keyboard.isKeyPressed(KEY_ESC) || M5Cardputer.Keyboard.isKeyPressed('`')) {
        return -1;
      }
      if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
        ssid = WiFi.SSID(idx);
        openNet = (WiFi.encryptionType(idx) == WIFI_AUTH_OPEN);
        return idx;
      }
    }
  }
}
bool tryConnectWiFi(const String& ssid, const String& pass, int tries){
  auto& d = M5Cardputer.Display;
  d.fillScreen(0x0000);
  d.setTextSize(1);
  
  // Заголовок
  d.setCursor(0, 0);
  d.setTextColor(0x07FF, 0x0000);
  d.println("WiFi Connection");
  d.setTextColor(0xFFFF, 0x0000);
  d.println("----------------------------------------");
  
  // Рамка с информацией
  drawBox(4, 20, 232, 50, 0xFFE0); // желтая рамка
  
  d.setCursor(8, 26);
  d.setTextColor(0x8410, 0x0000);
  d.println("Connecting to:");
  
  d.setCursor(8, 38);
  d.setTextColor(0xFFFF, 0x0000);
  String displaySsid = ssid;
  if (displaySsid.length() > 30) displaySsid = displaySsid.substring(0, 30);
  d.println(displaySsid);
  
  d.setCursor(8, 52);
  d.setTextColor(0x8410, 0x0000);
  d.print("Status: ");
  d.setTextColor(0xFFE0, 0x0000);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.length() ? pass.c_str() : nullptr);
  
  int dotCount = 0;
  for (int i = 0; i < tries; ++i) {
    if (WiFi.status() == WL_CONNECTED) {
      // Success!
      d.fillRect(6, 22, 228, 46, 0x0000);
      drawBox(4, 20, 232, 50, 0x07E0); // зеленая рамка
      
      d.setCursor(90, 40);
      d.setTextColor(0x07E0, 0x0000);
      d.setTextSize(1);
      d.println("Connected!");
      
      delay(500);
      return true;
    }
    
    // Анимация точек
    if (i % 3 == 0) {
      d.fillRect(68, 52, 100, 8, 0x0000);
      d.setCursor(68, 52);
      d.setTextColor(0xFFE0, 0x0000);
      for (int j = 0; j < (dotCount % 4); ++j) {
        d.print(".");
      }
      dotCount++;
    }
    
    delay(300);
  }
  
  // Failed
  d.fillRect(6, 22, 228, 46, 0x0000);
  drawBox(4, 20, 232, 50, 0xF800); // красная рамка
  
  d.setCursor(8, 35);
  d.setTextColor(0xF800, 0x0000);
  d.println("  Connection Failed!");
  
  d.setCursor(8, 50);
  d.setTextColor(0x8410, 0x0000);
  d.println("  Check password/signal");
  
  delay(2000);
  return false;
}
bool wifiOnboarding(){
  String ssid; bool openNet=false; int idx = scanAndSelectNetwork(ssid, openNet);
  if (idx<0) return false;
  String pass=""; if (!openNet) { pass = promptInput("WiFi password for "+ssid, 64, true); if (pass.length()==0) return false; }
  bool ok = tryConnectWiFi(ssid, pass);
  if (ok){ 
    addNetwork(ssid, pass); // Добавляем в список сохраненных сетей
  }
  return ok;
}
void wifiLoadOrOnboard(){
  // Загружаем сохраненные сети
  loadSavedNetworks();
  
  // Миграция: если есть старый формат (одна сеть) - переносим его
  prefs.begin("microcast", false);
  String oldSsid = prefs.getString("ssid", "");
  String oldPass = prefs.getString("pass", "");
  prefs.end();
  
  if (oldSsid.length() > 0 && savedNetworksCount == 0) {
    // Мигрируем старую сеть в новый формат
    addNetwork(oldSsid, oldPass);
    // Удаляем старый формат
    prefs.begin("microcast", false);
    prefs.remove("ssid");
    prefs.remove("pass");
    prefs.end();
  }
  
  // Пробуем подключиться к любой сохраненной сети
  if (savedNetworksCount > 0) {
    wifiOK = tryConnectToSavedNetworks();
    if (!wifiOK) {
      // Не удалось подключиться ни к одной - запускаем onboarding
      wifiOK = wifiOnboarding();
    }
  } else {
    // Нет сохраненных сетей - запускаем onboarding
    wifiOK = wifiOnboarding();
  }
}

void wifiForgetAndReconfig(){
  // Очищаем все сохраненные сети
  prefs.begin("microcast", false);
  prefs.putInt("wifi_count", 0);
  for (int i = 0; i < MAX_WIFI_NETWORKS; i++) {
    String key_ssid = "wifi_ssid_" + String(i);
    String key_pass = "wifi_pass_" + String(i);
    prefs.remove(key_ssid.c_str());
    prefs.remove(key_pass.c_str());
  }
  prefs.end();
  
  savedNetworksCount = 0;
  WiFi.disconnect(true,true); 
  delay(300); 
  wifiOK = wifiOnboarding();
}

// ====== Device / Author ======
void ensureDevice() {
  prefs.begin("microcast", false);
  deviceId = prefs.getString("device_id", "");
  if (deviceId.length() == 0) {
    deviceId = prefs.getString("dev", ""); // старый ключ для совместимости
  }
  author   = prefs.getString("author", "");
  recoveryCode = prefs.getString("recovery_code", "");
  
  // Если нет device_id - предлагаем выбор: новая регистрация или восстановление
  if (deviceId.length() < 8) {
    auto& d = M5Cardputer.Display;
    d.setTextSize(1);
    
    int selection = 0; // 0 = New Registration, 1 = Restore Account
    int lastSelection = -1; // для отслеживания изменений
    
    // Функция отрисовки меню (вызывается только при изменении)
    auto drawMenu = [&]() {
      d.fillScreen(0x0000);
      
      // Заголовок (координата Y: 20)
      d.setCursor(95, 20);
      d.setTextColor(0x07E0, 0x0000);
      d.println("MicroCast");
      
      d.drawFastHLine(20, 30, 200, 0x8410);
      
      // Опции (координата Y: 50, 70)
      d.setCursor(60, 50);
      if (selection == 0) {
        d.setTextColor(0x0000, 0xFFFF); // инверсия
        d.print(" > New Registration ");
        d.setTextColor(0xFFFF, 0x0000);
      } else {
        d.setTextColor(0xFFFF, 0x0000);
        d.print("   New Registration ");
      }
      
      d.setCursor(60, 70);
      if (selection == 1) {
        d.setTextColor(0x0000, 0xFFFF); // инверсия
        d.print(" > Restore Account  ");
        d.setTextColor(0xFFFF, 0x0000);
      } else {
        d.setTextColor(0xFFFF, 0x0000);
        d.print("   Restore Account  ");
      }
      
      // Подсказки (координата Y: 110)
      d.setCursor(30, 110);
      d.setTextColor(0x8410, 0x0000);
      d.print("[;/.] Navigate  [Enter] Select");
    };
    
    // Первая отрисовка
    drawMenu();
    lastSelection = selection;
    
    while (true) {
      // Ждем нажатия
      M5Cardputer.update();
      if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        if (M5Cardputer.Keyboard.isKeyPressed(';') || M5Cardputer.Keyboard.isKeyPressed('w') || 
            M5Cardputer.Keyboard.isKeyPressed('W')) {
          if (selection != 0) {
            selection = 0;
            drawMenu(); // перерисовываем только при изменении
            lastSelection = selection;
          }
        }
        else if (M5Cardputer.Keyboard.isKeyPressed('.') || M5Cardputer.Keyboard.isKeyPressed('s') || 
                 M5Cardputer.Keyboard.isKeyPressed('S')) {
          if (selection != 1) {
            selection = 1;
            drawMenu(); // перерисовываем только при изменении
            lastSelection = selection;
          }
        }
        else if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
          if (selection == 0) {
            // Новая регистрация - генерируем device_id
            uint32_t r = esp_random();
            deviceId = String((uint32_t)millis(), HEX) + "-" + String(r, HEX);
            prefs.putString("device_id", deviceId);
            prefs.putString("dev", deviceId); // для совместимости
            break;
          } else {
            // Восстановление - запрашиваем recovery code
            String code = promptInput("Enter recovery code", 12);
            
            if (code.length() == 0) {
              // ESC - отменено, создаем новый device_id
              uint32_t r = esp_random();
              deviceId = String((uint32_t)millis(), HEX) + "-" + String(r, HEX);
              prefs.putString("device_id", deviceId);
              prefs.putString("dev", deviceId);
              break;
            }
            
            // Проверяем код на сервере
            if (wifiOK && apiRecoveryVerify(code)) {
              // Успешно восстановлено! deviceId и author уже установлены в apiRecoveryVerify
              showMessage("Success!", "Account restored:", author, "", 2);
              delay(1000);
              while (M5Cardputer.Keyboard.isPressed()) { M5Cardputer.update(); delay(10); }
              while (!M5Cardputer.Keyboard.isChange()) { M5Cardputer.update(); delay(10); }
              prefs.end();
              return; // выходим, device_id и author уже восстановлены
            } else {
              // Ошибка восстановления
              showMessage("Error", "Invalid recovery code", "Try again or press ESC", "", 2);
              delay(1000);
              while (M5Cardputer.Keyboard.isPressed()) { M5Cardputer.update(); delay(10); }
              while (!M5Cardputer.Keyboard.isChange()) { M5Cardputer.update(); delay(10); }
              // Возвращаемся к выбору - перерисовываем меню
              drawMenu();
              continue;
            }
          }
        }
      }
      delay(10);
    }
  }
  // Register device if author not set
  if (author.length() == 0) {
    String newAuthor = "";
    while (true) {
      newAuthor = asciiOnly(promptInput("Username (3-24 chars)", 24), 24);
      if (newAuthor.length() == 0) {
        // ESC pressed - use temporary name
        author = "user_" + deviceId.substring(0, 6);
        prefs.putString("author", author);
        showMessage("Registration Skipped", "Using temp name:", author, "Press [U] to register later", 3);
        while (!M5Cardputer.Keyboard.isChange()) { M5Cardputer.update(); delay(10); }
        break;
      }
      if (newAuthor.length() < 3) {
        showMessage("Error", "Username too short!", "Minimum 3 characters", "ESC=Skip, Any=Retry", 2);
        delay(1000); // Задержка чтобы пользователь успел отпустить клавишу
        // Очищаем состояние клавиатуры
        while (M5Cardputer.Keyboard.isPressed()) { M5Cardputer.update(); delay(10); }
        // Ждем нового нажатия
        while (!M5Cardputer.Keyboard.isChange()) { M5Cardputer.update(); delay(10); }
        continue;
      }
      // Try to register (WiFi already connected at this point)
      if (apiRegister(newAuthor)) {
        showMessage("Success!", "Username registered:", author, "", 1);
        delay(1000);
        while (M5Cardputer.Keyboard.isPressed()) { M5Cardputer.update(); delay(10); }
        while (!M5Cardputer.Keyboard.isChange()) { M5Cardputer.update(); delay(10); }
        
        // Показываем recovery code если он был получен
        if (recoveryCode.length() > 0) {
          showRecoveryCodePopup(recoveryCode);
        }
        
        break;
      } else {
        showMessage("Registration Failed", "Username may be taken", "or server error", "ESC=Skip, Any=Retry", 2);
        delay(1000);
        while (M5Cardputer.Keyboard.isPressed()) { M5Cardputer.update(); delay(10); }
        while (!M5Cardputer.Keyboard.isChange()) { M5Cardputer.update(); delay(10); }
      }
    }
  }
  prefs.end();
}
void setAuthor(const String& a) {
  String v = asciiOnly(a, 24); if (!v.length()) v = "anon";
  prefs.begin("microcast", false); prefs.putString("author", v); prefs.end(); author = v;
}

bool apiRegister(const String& newAuthor) {
  StaticJsonDocument<256> doc; doc["device_id"]=deviceId; doc["author"]=newAuthor;
  String payload; serializeJson(doc, payload);
  String resp; int code=0; 
  if (!httpPostJSON(String(PROXY_BASE)+"/v1/register", payload, resp, code)) return false;
  if (code==200) {
    StaticJsonDocument<512> respDoc;
    DeserializationError e = deserializeJson(respDoc, resp);
    if (!e && respDoc["ok"] == true) {
      author = String((const char*)respDoc["author"]);
      
      // Получаем recovery_code если он есть в response
      if (respDoc.containsKey("recovery_code")) {
        recoveryCode = String((const char*)respDoc["recovery_code"]);
      }
      
      prefs.begin("microcast", false); 
      prefs.putString("author", author);
      if (recoveryCode.length() > 0) {
        prefs.putString("recovery_code", recoveryCode);
      }
      prefs.end();
      return true;
    }
  }
  return false;
}

bool apiChangeName(const String& newAuthor, String& errorMsg) {
  StaticJsonDocument<256> doc; doc["device_id"]=deviceId; doc["new_author"]=newAuthor;
  String payload; serializeJson(doc, payload);
  String resp; int code=0;
  if (!httpPostJSON(String(PROXY_BASE)+"/v1/change_name", payload, resp, code)) {
    errorMsg = "Network error";
    return false;
  }
  if (code==200) {
    StaticJsonDocument<256> respDoc;
    DeserializationError e = deserializeJson(respDoc, resp);
    if (!e && respDoc["ok"] == true) {
      author = String((const char*)respDoc["author"]);
      prefs.begin("microcast", false); 
      prefs.putString("author", author); 
      prefs.end();
      return true;
    }
  }
  // Parse error from response
  StaticJsonDocument<256> respDoc;
  DeserializationError e = deserializeJson(respDoc, resp);
  if (!e && respDoc.containsKey("error")) {
    String err = String((const char*)respDoc["error"]);
    if (err == "username_taken") errorMsg = "Username taken";
    else if (err == "device_not_found") errorMsg = "Device not found";
    else errorMsg = err;
  } else {
    errorMsg = "Server error";
  }
  return false;
}

// ====== RECOVERY CODE функции ======

// Показать полноэкранный popup с recovery code
void showRecoveryCodePopup(const String& code) {
  auto& d = M5Cardputer.Display;
  d.fillScreen(0x0000);
  d.setTextSize(1);
  
  // Заголовок
  d.setCursor(75, 20);
  d.setTextColor(0x07E0, 0x0000); // зеленый
  d.println("Recovery Code");
  
  // Линия
  d.drawFastHLine(20, 30, 200, 0x8410);
  
  // Код крупно
  d.setTextSize(2);
  d.setCursor(72, 50);
  d.setTextColor(0xFFE0, 0x0000); // желтый
  d.println(code);
  d.setTextSize(1);
  
  // Предупреждение
  d.setCursor(72, 80);
  d.setTextColor(0xF800, 0x0000); // красный
  d.println("WRITE IT DOWN!");
  
  d.setCursor(40, 95);
  d.setTextColor(0xFFFF, 0x0000);
  d.println("You'll need it to restore");
  d.setCursor(35, 105);
  d.println("account if device is reset.");
  
  // Кнопки
  d.setCursor(60, 125);
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[Enter]");
  d.setTextColor(0xFFFF, 0x0000);
  d.print(" I saved it");
  
  // Ждем нажатия
  delay(300);
  while (M5Cardputer.Keyboard.isPressed()) { M5Cardputer.update(); delay(10); }
  
  while (true) {
    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
      if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
        // Отмечаем что показали
        prefs.begin("microcast", false);
        prefs.putBool("recovery_shown", true);
        prefs.end();
        break;
      }
    }
    delay(10);
  }
}

// Проверка recovery code и восстановление аккаунта
bool apiRecoveryVerify(const String& code) {
  StaticJsonDocument<256> doc;
  doc["recovery_code"] = code;
  
  String payload;
  serializeJson(doc, payload);
  
  String resp;
  int httpCode = 0;
  
  if (!httpPostJSON(String(PROXY_BASE) + "/v1/recovery_verify", payload, resp, httpCode)) {
    return false;
  }
  
  if (httpCode == 200) {
    StaticJsonDocument<512> respDoc;
    DeserializationError e = deserializeJson(respDoc, resp);
    
    if (!e && respDoc["ok"] == true) {
      // Восстанавливаем device_id и username
      deviceId = String((const char*)respDoc["device_id"]);
      author = String((const char*)respDoc["username"]);
      recoveryCode = code;
      
      // Сохраняем в Preferences
      prefs.begin("microcast", false);
      prefs.putString("device_id", deviceId);
      prefs.putString("author", author);
      prefs.putString("recovery_code", recoveryCode);
      prefs.putBool("recovery_shown", true); // уже знает код
      prefs.end();
      
      return true;
    }
  }
  
  return false;
}

// ====== HTTP helpers ======
bool httpPostJSON(const String& url, const String& payload, String& resp, int& code) {
  if (!wifiOK) return false; HTTPClient http; http.begin(url);
  http.useHTTP10(true); http.setReuse(false);
  http.addHeader("Content-Type","application/json"); http.addHeader("Connection","close");
  code = http.POST((uint8_t*)payload.c_str(), payload.length());
  if (code>0) resp = http.getString(); http.end(); return code>0;
}
bool httpGetJSON(const String& url, String& resp, int& code) {
  if (!wifiOK) return false; HTTPClient http; http.begin(url);
  http.useHTTP10(true); http.setReuse(false); code = http.GET();
  if (code>0) resp = http.getString(); http.end(); return code>0;
}

// ====== NOTIFICATIONS functions ======
// Воспроизведение звука уведомления
void playNotificationSound() {
  if (!notificationSound) return;
  
  // Двойной бип: 1000Hz 200ms, пауза 100ms, 1200Hz 200ms
  M5Cardputer.Speaker.tone(1000, 200);
  delay(300);
  M5Cardputer.Speaker.tone(1200, 200);
}

// Проверка новых постов (тихая, без обновления UI)
void checkForNewPosts() {
  if (!wifiOK || !notificationsEnabled) return;
  
  // Проверяем интервал
  unsigned long interval = (unsigned long)notificationInterval * 60000; // минуты в миллисекунды
  if (millis() - lastNotificationCheck < interval) return;
  
  lastNotificationCheck = millis();
  
  // Делаем тихий запрос к API
  String resp;
  int code = 0;
  if (!httpGetJSON(String(PROXY_BASE) + "/v1/feed", resp, code)) return;
  if (code != 200) return;
  
  StaticJsonDocument<8192> doc;
  DeserializationError e = deserializeJson(doc, resp);
  if (e) return;
  
  JsonArray arr = doc["items"].as<JsonArray>();
  if (arr.size() == 0) return;
  
  // Получаем ID первого (самого нового) поста
  JsonObject firstPost = arr[0];
  String newPostId = String((const char*)(firstPost["id"] | ""));
  
  if (newPostId.length() == 0) return;
  
  // Если lastSeenPostId пустой (первый запуск) - просто сохраняем
  if (lastSeenPostId.length() == 0) {
    lastSeenPostId = newPostId;
    return;
  }
  
  // Проверяем: есть ли новые посты?
  if (newPostId != lastSeenPostId) {
    // Подсчитываем количество новых постов
    int newCount = 0;
    for (JsonObject post : arr) {
      String postId = String((const char*)(post["id"] | ""));
      if (postId == lastSeenPostId) break;
      newCount++;
    }
    
    // Проверяем: это совсем новые посты или добавились к существующим?
    if (!hasUnreadPosts || newCount > lastNewPostCount) {
      // Новые посты! Или добавились ещё новые
      hasUnreadPosts = true;
      lastNewPostCount = newCount;
      
      // Триггерим уведомление
      playNotificationSound();
      
      // LED начнет мигать автоматически через updateNotificationLED()
    }
  }
}

// Сброс уведомлений (когда пользователь открыл ленту)
void markPostsAsRead() {
  if (feedCount > 0) {
    lastSeenPostId = FEED[0].id;
    hasUnreadPosts = false;
    lastNewPostCount = 0;
    ledOff();
    ledBlinkState = false;
    
    // Сохраняем в Preferences
    prefs.begin("microcast", false);
    prefs.putString("last_post_id", lastSeenPostId);
    prefs.end();
  }
}

// Загрузка настроек уведомлений из Preferences
void loadNotificationSettings() {
  prefs.begin("microcast", false);
  notificationsEnabled = prefs.getBool("notif_enabled", true);
  notificationInterval = prefs.getInt("notif_interval", 10);
  notificationSound = prefs.getBool("notif_sound", true);
  notificationLED = prefs.getBool("notif_led", true);
  lastSeenPostId = prefs.getString("last_post_id", "");
  prefs.end();
}

// Сохранение настроек уведомлений
void saveNotificationSettings() {
  prefs.begin("microcast", false);
  prefs.putBool("notif_enabled", notificationsEnabled);
  prefs.putInt("notif_interval", notificationInterval);
  prefs.putBool("notif_sound", notificationSound);
  prefs.putBool("notif_led", notificationLED);
  prefs.end();
}

// ====== API ======
bool apiPost(const String& text) {
  // Author is taken from database by device_id, not sent from client
  StaticJsonDocument<384> doc; doc["text"]=text; doc["device_id"]=deviceId;
  String payload; serializeJson(doc, payload);
  String resp; int code=0; if (!httpPostJSON(String(PROXY_BASE)+"/v1/post", payload, resp, code)) return false;
  return code==200;
}
bool apiLike(const String& postId) {
  StaticJsonDocument<256> doc; doc["post_id"]=postId; doc["device_id"]=deviceId;
  String payload; serializeJson(doc, payload);
  String resp; int code=0; if (!httpPostJSON(String(PROXY_BASE)+"/v1/like", payload, resp, code)) return false;
  return code==200;
}
bool apiFeed() {
  String resp; int code=0; if (!httpGetJSON(String(PROXY_BASE)+"/v1/feed", resp, code)) return false;
  if (code!=200) return false;
  StaticJsonDocument<8192> doc; DeserializationError e = deserializeJson(doc, resp);
  if (e) return false; JsonArray arr = doc["items"].as<JsonArray>();
  feedCount = 0; for (JsonObject o: arr) {
    if (feedCount>=FEED_MAX) break; Post p;
    p.id = (const char*)(o["id"]|""); p.author = asciiOnly((const char*)(o["author"]|""), 24);
    p.text = asciiOnly((const char*)(o["text"]|""), 120);
    p.created_at = (const char*)(o["created_at"]|""); p.likes = (int)(o["likes"]|0);
    p.comments = (int)(o["comments"]|0);
    FEED[feedCount++] = p;
  }
  if (sel >= (int)feedCount) sel = (int)feedCount-1; if (sel<0) sel=0; return true;
}
bool apiStats() {
  String resp; int code=0; if (!httpGetJSON(String(PROXY_BASE)+"/v1/stats", resp, code)) return false;
  if (code!=200) return false;
  StaticJsonDocument<512> doc; DeserializationError e = deserializeJson(doc, resp);
  if (e) return false;
  deviceCount = (int)(doc["devices"]|0);
  
  // Проверяем recovery_code (для миграции существующих пользователей)
  if (doc.containsKey("recovery_code")) {
    String newCode = String((const char*)doc["recovery_code"]);
    bool isNewCode = doc["is_new_code"] | false; // флаг что код только что сгенерирован
    
    if (newCode.length() > 0) {
      recoveryCode = newCode;
      
      // Сохраняем код в Preferences
      prefs.begin("microcast", false);
      prefs.putString("recovery_code", newCode);
      bool recoveryShown = prefs.getBool("recovery_shown", false);
      prefs.end();
      
      // Если код новый и мы еще не показывали - показываем popup
      if (isNewCode && !recoveryShown) {
        showRecoveryCodePopup(newCode);
      }
    }
  }
  
  return true;
}

bool apiBest() {
  String resp; int code=0; if (!httpGetJSON(String(PROXY_BASE)+"/v1/best", resp, code)) return false;
  if (code!=200) return false;
  StaticJsonDocument<8192> doc; DeserializationError e = deserializeJson(doc, resp);
  if (e) return false; JsonArray arr = doc["items"].as<JsonArray>();
  feedCount = 0; for (JsonObject o: arr) {
    if (feedCount>=FEED_MAX) break; Post p;
    p.id = (const char*)(o["id"]|""); p.author = asciiOnly((const char*)(o["author"]|""), 24);
    p.text = asciiOnly((const char*)(o["text"]|""), 120);
    p.created_at = (const char*)(o["created_at"]|""); p.likes = (int)(o["likes"]|0);
    p.comments = (int)(o["comments"]|0);
    FEED[feedCount++] = p;
  }
  if (sel >= (int)feedCount) sel = (int)feedCount-1; if (sel<0) sel=0; return true;
}

bool apiProfile() {
  String url = String(PROXY_BASE) + "/v1/profile?device_id=" + deviceId;
  String resp; int code=0; if (!httpGetJSON(url, resp, code)) return false;
  if (code!=200) return false;
  StaticJsonDocument<8192> doc; DeserializationError e = deserializeJson(doc, resp);
  if (e) return false;
  
  profile.author = String((const char*)(doc["author"]|""));
  profile.postCount = (int)(doc["post_count"]|0);
  profile.totalLikes = (int)(doc["total_likes"]|0);
  
  JsonArray arr = doc["posts"].as<JsonArray>();
  feedCount = 0; for (JsonObject o: arr) {
    if (feedCount>=FEED_MAX) break; Post p;
    p.id = (const char*)(o["id"]|""); p.author = asciiOnly((const char*)(o["author"]|""), 24);
    p.text = asciiOnly((const char*)(o["text"]|""), 120);
    p.created_at = (const char*)(o["created_at"]|""); p.likes = (int)(o["likes"]|0);
    p.comments = (int)(o["comments"]|0);
    FEED[feedCount++] = p;
  }
  if (sel >= (int)feedCount) sel = (int)feedCount-1; if (sel<0) sel=0; return true;
}

// ====== COMMENTS API ======
bool apiComments(const String& postId) {
  String url = String(PROXY_BASE) + "/v1/comments?post_id=" + postId;
  String resp; int code=0; if (!httpGetJSON(url, resp, code)) return false;
  if (code!=200) return false;
  StaticJsonDocument<4096> doc; DeserializationError e = deserializeJson(doc, resp);
  if (e) return false;
  
  JsonArray arr = doc["items"].as<JsonArray>();
  commentCount = 0;
  for (JsonObject o: arr) {
    if (commentCount >= COMMENTS_MAX) break;
    Comment c;
    c.id = (const char*)(o["id"]|"");
    c.author = asciiOnly((const char*)(o["author"]|""), 24);
    c.text = asciiOnly((const char*)(o["text"]|""), 120);
    c.created_at = (const char*)(o["created_at"]|"");
    COMMENTS[commentCount++] = c;
  }
  
  if (commentSel >= (int)commentCount) commentSel = (int)commentCount-1;
  if (commentSel < 0) commentSel = 0;
  return true;
}

bool apiComment(const String& postId, const String& text) {
  StaticJsonDocument<512> doc;
  doc["post_id"] = postId;
  doc["device_id"] = deviceId;
  doc["text"] = text;
  String payload;
  serializeJson(doc, payload);
  
  String resp;
  int code = 0;
  if (!httpPostJSON(String(PROXY_BASE) + "/v1/comment", payload, resp, code)) return false;
  return code == 200;
}

// Функция загрузки данных текущего раздела
void loadCurrentSection() {
  // SOON раздел - показываем анимацию вместо загрузки данных
  if (currentSection == SECTION_SOON) {
    drawSpaceMatrix();
    drawUI();
    return;
  }
  
  if (!wifiOK) return;
  isLoading = true;
  drawUI(); // показываем "Loading..."
  
  bool success = false;
  switch(currentSection) {
    case SECTION_NEW: success = apiFeed(); break;
    case SECTION_TOP: success = apiBest(); break;
    case SECTION_YOU: success = apiProfile(); break;
    case SECTION_SOON: break; // уже обработан выше
  }
  
  isLoading = false;
  sel = 0;
  scrollOffset = 0;
  drawUI();
}

// Тихое обновление (без Loading и без сброса позиции)
void refreshCurrentSectionSilent() {
  if (!wifiOK) return;
  
  // Сохраняем текущую позицию
  int oldSel = sel;
  int oldScrollOffset = scrollOffset;
  
  bool success = false;
  switch(currentSection) {
    case SECTION_NEW: success = apiFeed(); break;
    case SECTION_TOP: success = apiBest(); break;
    case SECTION_YOU: success = apiProfile(); break;
  }
  
  // Восстанавливаем позицию
  sel = min(oldSel, (int)feedCount - 1);
  if (sel < 0) sel = 0;
  scrollOffset = min(oldScrollOffset, max(0, (int)feedCount - 1));
  
  drawUI();
}

// ====== Splash Screen ======
void showSplashScreen() {
  auto& d = M5Cardputer.Display;
  const char* text = "MicroCast";
  const int textLen = 9;
  const char glitchChars[] = "@#$%&*!?<>[]{}";
  const int glitchCount = 14;
  
  d.fillScreen(0x0000); // черный фон
  d.setTextSize(2); // большой шрифт
  
  // Вычисляем позицию для центрирования
  // Размер символа при textSize=2: ~12x16 пикселей
  int charWidth = 12;
  int totalWidth = textLen * charWidth;
  int startX = (240 - totalWidth) / 2; // центрируем по горизонтали
  int startY = 60; // центрируем по вертикали
  
  // Анимация: glitch + typewriter (2 секунды)
  unsigned long startTime = millis();
  int currentChar = 0;
  
  while (millis() - startTime < 2000) {
    d.fillRect(0, startY - 5, 240, 25, 0x0000); // очищаем область текста
    d.setCursor(startX, startY);
    d.setTextColor(0x07E0, 0x0000); // зеленый цвет
    
    // Определяем сколько символов уже "напечатано"
    unsigned long elapsed = millis() - startTime;
    currentChar = min((int)(elapsed / 200), textLen); // 200ms на символ = ~1.8 сек на весь текст
    
    // Отрисовка
    for (int i = 0; i < textLen; i++) {
      if (i < currentChar) {
        // Уже напечатанные символы - показываем правильно
        d.print(text[i]);
      } else if (i == currentChar) {
        // Текущий печатающийся символ - glitch эффект
        if (random(0, 3) == 0) { // 33% шанс показать glitch
          d.print(glitchChars[random(0, glitchCount)]);
        } else {
          d.print(text[i]);
        }
      } else {
        // Еще не напечатанные - случайные символы с низкой яркостью
        d.setTextColor(0x0320, 0x0000); // темно-зеленый
        d.print(glitchChars[random(0, glitchCount)]);
        d.setTextColor(0x07E0, 0x0000); // возвращаем яркий зеленый
      }
    }
    
    delay(50); // обновление 20 FPS
    
    // Проверка на пропуск (любая клавиша)
    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
      break;
    }
  }
  
  // Финальный кадр - чистый текст
  d.fillRect(0, startY - 5, 240, 25, 0x0000);
  d.setCursor(startX, startY);
  d.setTextColor(0x07E0, 0x0000);
  d.print(text);
  delay(500); // показываем финальный текст 0.5 сек
  
  d.setTextSize(1); // возвращаем стандартный размер шрифта
}

// ====== Space Matrix Animation ======
void drawSpaceMatrix() {
  auto& d = M5Cardputer.Display;
  const char* spaceChars = ".':*+=#@";
  const int numChars = 8;
  const int cols = 40; // символов по ширине (240/6)
  const int rows = 21; // символов по высоте (128/6)
  
  // Матрица символов и их яркости
  static char matrix[21][40];
  static int brightness[21][40];
  static int prevBrightness[21][40]; // предыдущая яркость для отслеживания изменений
  static bool initialized = false;
  
  // Инициализация при первом вызове
  if (!initialized) {
    d.fillScreen(0x0000); // очищаем экран только один раз
    d.setTextSize(1);
    
    // Заголовок (рисуем один раз)
    d.setCursor(105, 0);
    d.setTextColor(0xFFE0, 0x0000);
    d.println("#");
    
    // Подсказка внизу (рисуем один раз)
    d.setCursor(70, 122);
    d.setTextColor(0x8410, 0x0000);
    d.print("Press ESC to exit");
    
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        matrix[y][x] = spaceChars[random(0, numChars)];
        brightness[y][x] = random(0, 8);
        prevBrightness[y][x] = -1; // форсируем первую отрисовку
      }
    }
    initialized = true;
  }
  
  unsigned long startTime = millis();
  
  while (millis() - startTime < 30000) { // 30 секунд анимации
    // Обновление и отрисовка только изменённых символов
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        int oldBrightness = brightness[y][x];
        
        // Случайное мерцание (реже для плавности)
        if (random(0, 30) == 0) {
          brightness[y][x] = random(0, 8);
          if (random(0, 15) == 0) {
            matrix[y][x] = spaceChars[random(0, numChars)];
          }
        }
        
        // Затухание (медленнее для плавности)
        if (brightness[y][x] > 0 && random(0, 5) == 0) {
          brightness[y][x]--;
        }
        
        // Рисуем только если яркость изменилась
        if (brightness[y][x] != prevBrightness[y][x]) {
          // Отрисовка с градацией цвета
          uint16_t color;
          switch(brightness[y][x]) {
            case 7: color = 0xFFFF; break; // белый
            case 6: color = 0x07FF; break; // голубой яркий
            case 5: color = 0x05DF; break; // голубой средний
            case 4: color = 0x03BF; break; // голубой тусклый
            case 3: color = 0x021F; break; // синий яркий
            case 2: color = 0x0119; break; // синий средний
            case 1: color = 0x0010; break; // синий тусклый
            default: color = 0x0000; break; // черный
          }
          
          d.setCursor(x * 6, y * 6 + 10);
          d.setTextColor(color, 0x0000);
          if (brightness[y][x] > 0) {
            d.print(matrix[y][x]);
          } else {
            d.print(' '); // стираем символ
          }
          
          prevBrightness[y][x] = brightness[y][x];
        }
      }
    }
    
    // Проверка на выход
    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
      if (M5Cardputer.Keyboard.isKeyPressed(KEY_ESC) || M5Cardputer.Keyboard.isKeyPressed('`')) {
        initialized = false; // сброс для следующего входа
        return;
      }
      lastActivityTime = millis(); // обновляем активность при любом нажатии
    }
    
    // Обновляем активность, чтобы экран не выключился во время анимации
    if ((millis() - lastActivityTime) > (SCREEN_TIMEOUT / 2)) {
      lastActivityTime = millis();
    }
    
    delay(80); // ~12 FPS для более плавной анимации
  }
  
  initialized = false;
}

// ====== Settings Menu ======
void drawSettingsMainMenu() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(0x0000);
  d.setTextSize(1);
  
  // Заголовок
  d.setCursor(0, 0);
  d.setTextColor(0x07FF, 0x0000); // cyan
  d.println("Settings");
  d.setTextColor(0xFFFF, 0x0000);
  d.println("----------------------------------------");
  
  // Пункт 1: Notifications
  d.setCursor(6, 20);
  if (settingsMenuSelection == 0) {
    d.setTextColor(0x0000, 0x07E0); // инверсия для выбранного
    d.print(" > Notifications         ");
  } else {
    d.setTextColor(0xFFFF, 0x0000);
    d.print("   Notifications");
  }
  
  // Пункт 2: WiFi List
  d.setCursor(6, 32);
  if (settingsMenuSelection == 1) {
    d.setTextColor(0x0000, 0x07E0);
    d.print(" > WiFi List             ");
  } else {
    d.setTextColor(0xFFFF, 0x0000);
    d.print("   WiFi List");
  }
  
  // Controls
  d.setCursor(0, 100);
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[;/.]");
  d.setTextColor(0x8410, 0x0000);
  d.print(" Navigate  ");
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[Enter]");
  d.setTextColor(0x8410, 0x0000);
  d.print(" Select");
  
  d.setCursor(0, 110);
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[ESC]");
  d.setTextColor(0x8410, 0x0000);
  d.print(" Back");
}

void drawNotificationsSettings() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(0x0000);
  d.setTextSize(1);
  
  // Заголовок
  d.setCursor(0, 0);
  d.setTextColor(0x07FF, 0x0000);
  d.println("Settings > Notifications");
  d.setTextColor(0xFFFF, 0x0000);
  d.println("----------------------------------------");
  
  // Enable notifications
  d.setCursor(6, 20);
  d.setTextColor(0xFFFF, 0x0000);
  d.print("[");
  d.setTextColor(notificationsEnabled ? 0x07E0 : 0x8410, 0x0000);
  d.print(notificationsEnabled ? "X" : " ");
  d.setTextColor(0xFFFF, 0x0000);
  d.print("] Enable notifications");
  
  // Check interval
  d.setCursor(6, 32);
  d.setTextColor(0x8410, 0x0000);
  d.print("Check interval: ");
  d.setTextColor(0x07FF, 0x0000);
  d.print(notificationInterval);
  d.setTextColor(0x8410, 0x0000);
  d.print(" min (1-60)");
  
  // Sound alert
  d.setCursor(6, 44);
  d.setTextColor(0xFFFF, 0x0000);
  d.print("[");
  d.setTextColor(notificationSound ? 0x07E0 : 0x8410, 0x0000);
  d.print(notificationSound ? "X" : " ");
  d.setTextColor(0xFFFF, 0x0000);
  d.print("] Sound alert");
  
  // LED alert
  d.setCursor(6, 56);
  d.setTextColor(0xFFFF, 0x0000);
  d.print("[");
  d.setTextColor(notificationLED ? 0x07E0 : 0x8410, 0x0000);
  d.print(notificationLED ? "X" : " ");
  d.setTextColor(0xFFFF, 0x0000);
  d.print("] LED indicator");
  
  // Status
  d.setCursor(2, 70);
  d.setTextColor(0x8410, 0x0000);
  d.print("Last check: ");
  if (lastNotificationCheck == 0) {
    d.print("never");
  } else {
    unsigned long elapsed = (millis() - lastNotificationCheck) / 60000;
    d.print(elapsed);
    d.print(" min ago");
  }
  
  // Unread posts
  if (hasUnreadPosts) {
    d.setCursor(2, 80);
    d.setTextColor(0x07E0, 0x0000);
    d.print("Unread: ");
    d.print(lastNewPostCount);
    d.print(" post");
    if (lastNewPostCount != 1) d.print("s");
  }
  
  // Controls
  d.setCursor(0, 100);
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[1]");
  d.setTextColor(0x8410, 0x0000);
  d.print(" Toggle  ");
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[2]");
  d.setTextColor(0x8410, 0x0000);
  d.print(" Sound  ");
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[3]");
  d.setTextColor(0x8410, 0x0000);
  d.print(" LED");
  
  d.setCursor(0, 110);
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[+/-]");
  d.setTextColor(0x8410, 0x0000);
  d.print(" Interval  ");
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[ESC]");
  d.setTextColor(0x8410, 0x0000);
  d.print(" Back");
}

void drawWiFiListSettings() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(0x0000);
  d.setTextSize(1);
  
  // Заголовок
  d.setCursor(0, 0);
  d.setTextColor(0x07FF, 0x0000);
  d.println("Settings > WiFi List");
  d.setTextColor(0xFFFF, 0x0000);
  d.println("----------------------------------------");
  
  // Список сохраненных сетей
  if (savedNetworksCount == 0) {
    d.setCursor(6, 20);
    d.setTextColor(0x8410, 0x0000);
    d.println("No saved networks");
  } else {
    for (int i = 0; i < savedNetworksCount; i++) {
      d.setCursor(6, 20 + i * 12);
      if (i == wifiListSelection) {
        d.setTextColor(0x0000, 0x07E0); // выбранная сеть
        d.print(" > ");
      } else {
        d.setTextColor(0xFFFF, 0x0000);
        d.print("   ");
      }
      
      // SSID (обрезаем если длинный)
      String displaySsid = savedNetworks[i].ssid;
      if (displaySsid.length() > 25) {
        displaySsid = displaySsid.substring(0, 22) + "...";
      }
      d.print(displaySsid);
      
      // Индикатор текущего подключения
      if (WiFi.SSID() == savedNetworks[i].ssid) {
        d.setTextColor(0x07E0, (i == wifiListSelection) ? 0x07E0 : 0x0000);
        d.print(" [*]");
      }
    }
  }
  
  // Controls
  d.setCursor(0, 100);
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[;/.]");
  d.setTextColor(0x8410, 0x0000);
  d.print(" Select  ");
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[A]");
  d.setTextColor(0x8410, 0x0000);
  d.print(" Add");
  
  d.setCursor(0, 110);
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[D]");
  d.setTextColor(0x8410, 0x0000);
  d.print(" Delete  ");
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[ESC]");
  d.setTextColor(0x8410, 0x0000);
  d.print(" Back");
}

// ====== Info Popup ======
void drawInfoPopup() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(0x0000);
  d.setTextSize(1);
  
  // Центральное окно с рамкой
  drawBox(10, 15, 220, 110, 0x07FF); // cyan border
  
  // Заголовок
  d.setCursor(90, 23);
  d.setTextColor(0x07FF, 0x0000);
  d.println("Controls");
  
  // Сетка: две колонки, 6 строк
  // Координаты
  int xL = 18, xR = 128; int y0 = 40; int h = 10;
  d.setTextWrap(false);

  // Левая колонка
  d.setCursor(xL, y0 + 0*h); d.setTextColor(0xFFE0,0x0000); d.print("[M] "); d.setTextColor(0xFFFF,0x0000); d.print("Donate QR");
  d.setCursor(xL, y0 + 1*h); d.setTextColor(0xFFE0,0x0000); d.print("[Up/Dn] "); d.setTextColor(0xFFFF,0x0000); d.print("Scroll");
  d.setCursor(xL, y0 + 2*h); d.setTextColor(0xFFE0,0x0000); d.print("[Lt/Rt] "); d.setTextColor(0xFFFF,0x0000); d.print("Sections");
  d.setCursor(xL, y0 + 3*h); d.setTextColor(0xFFE0,0x0000); d.print("[Enter] "); d.setTextColor(0xFFFF,0x0000); d.print("Like/OK");
  d.setCursor(xL, y0 + 4*h); d.setTextColor(0xFFE0,0x0000); d.print("[C] "); d.setTextColor(0xFFFF,0x0000); d.print("Comments");
  d.setCursor(xL, y0 + 5*h); d.setTextColor(0xFFE0,0x0000); d.print("[Fn+Enter] "); d.setTextColor(0xFFFF,0x0000); d.print("Post/Comm");

  // Правая колонка
  d.setCursor(xR, y0 + 0*h); d.setTextColor(0xFFE0,0x0000); d.print("[R] "); d.setTextColor(0xFFFF,0x0000); d.print("Refresh");
  d.setCursor(xR, y0 + 1*h); d.setTextColor(0xFFE0,0x0000); d.print("[U] "); d.setTextColor(0xFFFF,0x0000); d.print("User");
  d.setCursor(xR, y0 + 2*h); d.setTextColor(0xFFE0,0x0000); d.print("[N] "); d.setTextColor(0xFFFF,0x0000); d.print("WiFi");
  d.setCursor(xR, y0 + 3*h); d.setTextColor(0xFFE0,0x0000); d.print("[S] "); d.setTextColor(0xFFFF,0x0000); d.print("Settings");
  d.setCursor(xR, y0 + 4*h); d.setTextColor(0xFFE0,0x0000); d.print("[Opt+R] "); d.setTextColor(0xFFFF,0x0000); d.print("Recovery");

  // Разделы (снизу)
  d.setCursor(18, 110);
  d.setTextColor(0xFFE0, 0x0000); d.print("NEW");
  d.setTextColor(0x8410, 0x0000); d.print(" latest   ");
  d.setTextColor(0xFFE0, 0x0000); d.print("TOP");
  d.setTextColor(0x8410, 0x0000); d.print(" best   ");
  d.setTextColor(0xFFE0, 0x0000); d.print("YOU");
  d.setTextColor(0x8410, 0x0000); d.print(" profile");

  // Подсказка закрытия
  d.setCursor(50, 122);
  d.setTextColor(0x8410, 0x0000);
  d.print("Press any key to close");
}

// ====== COMMENTS VIEW ======
void drawCommentsView() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(0x0000);
  d.setTextSize(1);
  
  // Заголовок
  d.setCursor(0, 0);
  d.setTextColor(0x07E0, 0x0000); // cyan
  d.print("Comments");
  d.setTextColor(0xFFFF, 0x0000);
  d.println(" ===============================");
  
  // Оригинальный пост (компактно)
  if (feedCount > 0 && sel < feedCount) {
    Post& p = FEED[sel];
    
    // Автор с темным фоном
    int authorWidth = p.author.length() * 6 + 2;
    d.fillRect(2, 9, authorWidth, 9, 0x0000);
    d.setCursor(3, 10);
    d.setTextColor(0x07FF, 0x0000); // cyan
    d.print(p.author);
    
    // Текст поста (макс 2 строки)
    d.setCursor(3, 18);
    d.setTextColor(0xFFFF, 0x0000);
    String txt = p.text;
    if (txt.length() > 38) {
      d.print(txt.substring(0, 38));
      d.setCursor(3, 28);
      if (txt.length() > 76) {
        d.print(txt.substring(38, 76) + "...");
      } else {
        d.print(txt.substring(38));
      }
    } else {
      d.print(txt);
    }
    
    // Статистика: лайки + комментарии (с иконками)
    // Сердечко + лайки
    d.fillRect(3, 34, 12, 10, 0x0000); // темный фон
    drawHeart(7, 38, 0xF800); // +2px вниз (было 38)
    d.setCursor(14, 36);
    d.setTextColor(0xF800, 0x0000);
    d.print(p.likes);
    
    // Пузырь + комментарии
    d.fillRect(30, 34, 12, 10, 0x0000); // темный фон
    drawCommentBubble(34, 37, 0x07FF); // +2px вниз (было 33)
    d.setCursor(41, 36);
    d.setTextColor(0x07FF, 0x0000);
    d.print(p.comments);
    
    d.setCursor(55, 36);
    d.setTextColor(0x8410, 0x0000);
    d.print("comments");
  }
  
  // Линия разделитель
  d.setCursor(0, 45);
  d.setTextColor(0x8410, 0x0000);
  d.println("----------------------------------------");
  
  // Комментарии (макс 3 на экране)
  int yPos = 53;
  int commentsPerScreen = 3;
  int commentHeight = 18; // высота одного комментария
  
  if (commentCount == 0) {
    d.setCursor(60, 70);
    d.setTextColor(0x8410, 0x0000);
    d.print("No comments yet");
  } else {
    for (int i = 0; i < commentsPerScreen; i++) {
      int idx = commentScrollOffset + i;
      if (idx >= commentCount) break;
      
      Comment& c = COMMENTS[idx];
      int boxY = yPos + i * commentHeight;
      bool isSelected = (idx == commentSel);
      
      // Рамка для выбранного комментария
      if (isSelected) {
        drawBox(2, boxY - 1, 236, commentHeight, 0x07E0); // зеленая рамка
      }
      
      // Автор комментария (голубой) с черным фоном - сдвиг на 3px вправо к центру
      int authorWidth = c.author.length() * 6 + 2;
      d.fillRect(5, boxY, authorWidth, 9, 0x0000); // черный фон под автором (5 вместо 2)
      d.setCursor(6, boxY - 1); // на 1px вверх, на 3px вправо (6 вместо 3)
      d.setTextColor(isSelected ? 0x07E0 : 0x07FF, 0x0000);
      d.print(c.author);
      
      // Дата и время (в правом углу, серый) с черным фоном - сдвиг на 3px влево к центру
      if (c.created_at.length() >= 19) {
        String date = c.created_at.substring(8, 10) + "." +
                      c.created_at.substring(5, 7) + " " +
                      c.created_at.substring(11, 16); // DD.MM HH:MM
        int dateWidth = date.length() * 6 + 4;
        d.fillRect(235 - dateWidth, boxY, dateWidth, 9, 0x0000); // черный фон под датой (235 вместо 238)
        d.setCursor(237 - dateWidth, boxY - 1); // на 1px вверх, на 3px влево (237 вместо 240)
        d.setTextColor(0x8410, 0x0000);
        d.print(date);
      }
      
      // Текст комментария с горизонтальной прокруткой для выбранного
      d.setCursor(3, boxY + 8);
      d.setTextColor(0xFFFF, 0x0000);
      String commentText = c.text;
      int maxChars = 39; // максимум символов на строке (с учетом стрелок)
      
      if (isSelected && commentText.length() > maxChars) {
        // Для выбранного комментария - показываем с прокруткой
        // Учитываем что с обеими стрелками показываем 37 символов (39 - 2)
        int effectiveTextChars = maxChars - 2; // с двумя стрелками
        int maxScrollPos = max(0, (int)commentText.length() - effectiveTextChars);
        int scrollPos = min(commentTextScroll, maxScrollPos);
        
        bool hasLeft = (scrollPos > 0);
        bool hasRight = (scrollPos + effectiveTextChars < commentText.length());
        
        // Вычисляем сколько символов текста показывать
        int textChars = effectiveTextChars;
        if (!hasLeft) textChars++;  // если нет <, можем показать +1 символ
        if (!hasRight) textChars++; // если нет >, можем показать +1 символ
        
        // Стрелка влево
        if (hasLeft) {
          d.setTextColor(0xFFE0, 0x0000);
          d.print("<");
        }
        
        // Текст
        d.setTextColor(0xFFFF, 0x0000);
        String visibleText = commentText.substring(scrollPos, min((int)commentText.length(), scrollPos + textChars));
        d.print(visibleText);
        
        // Стрелка вправо
        if (hasRight) {
          d.setTextColor(0xFFE0, 0x0000);
          d.print(">");
        }
      } else if (commentText.length() > maxChars) {
        // Для невыбранных - просто обрезаем с ...
        d.print(commentText.substring(0, maxChars - 3) + "...");
      } else {
        d.print(commentText);
      }
    }
  }
  
  // Подсказки внизу
  d.setCursor(0, 115);
  d.setTextColor(0xFFE0, 0x0000); // yellow
  d.print("[Up/Dn]");
  d.setTextColor(0x8410, 0x0000);
  d.print(" Nav  ");
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[Lt/Rt]");
  d.setTextColor(0x8410, 0x0000);
  d.print(" Scroll");
  
  d.setCursor(0, 125);
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[Fn+Enter]");
  d.setTextColor(0x8410, 0x0000);
  d.print(" Write  ");
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[ESC]");
  d.setTextColor(0x8410, 0x0000);
  d.print(" Back");
}

// ====== UI main ======
void drawUI() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(0x0000); // черный фон
  d.setTextSize(1);
  d.setCursor(0, 0);
  
  // Строка 1: "MicroCast [156]" (циан) + батарея + "beta0.4" (красный) справа
  d.setCursor(2, 0);
  int mcX = 2;
  int mcW = 9 * 6; // ширина слова MicroCast при TextSize=1
  if (isInHeaderSelector) {
    d.fillRect(mcX, 0, mcW, 9, 0x07FF);
    d.setCursor(mcX, 0);
    d.setTextColor(0x0000, 0x07FF);
    d.print("MicroCast");
    d.setTextColor(0x07FF, 0x0000);
    d.print(" [");
  } else {
    d.setTextColor(0x07FF, 0x0000);
    d.print("MicroCast [");
  }
  d.setTextColor(0xF800, 0x0000); // red
  d.print(deviceCount);
  d.setTextColor(0x07FF, 0x0000); // cyan
  d.print("]");
  
  // Индикатор батареи по центру-справа
  drawBatteryIndicator(155, 1);
  
  // beta0.4 справа
  d.setCursor(212, 0);
  d.setTextColor(0xF800, 0x0000); // red
  d.println("v0.6");
  
  // Строка 2: подчеркивание
  d.setCursor(0, 8);
  d.setTextColor(0xFFFF, 0x0000); // white
  d.println("----------------------------------------");
  
  // Строка 3: "[N]WiFi🟢 [U]User: name [I]Info"
  d.setCursor(2, 16);
  d.setTextColor(0xFFE0, 0x0000); // yellow
  d.print("[N]");
  d.setTextColor(0xFFFF, 0x0000); // white
  d.print("WiFi");
  
  // Индикатор Wi-Fi (зеленый/красный кружок)
  drawWiFiIndicator(48, 19, wifiOK);
  
  d.setCursor(65, 16);
  d.setTextColor(0xFFE0, 0x0000); // yellow
  d.print("[U]");
  d.setTextColor(0xFFFF, 0x0000); // white
  d.print("User:");
  d.setTextColor(0x07FF, 0x0000); // cyan для имени
  d.print(author);
  
  // [I] Info кнопка справа
  d.setCursor(196, 16);
  d.setTextColor(0xFFE0, 0x0000); // yellow
  d.print("[I]");
  d.setTextColor(0xFFFF, 0x0000); // white
  d.print("Info");
  
  // Строка 4: Красивые кнопки разделов с рамками (симметрично на всю ширину)
  int btnY = 27;
  int btnW = 70; // ширина кнопки
  int btnH = 14; // высота кнопки
  int spacing = 2; // отступ между кнопками (1 пиксель)
  int startX = 2; // начальная позиция
  int soonBtnW = 20; // ширина кнопки SOON (на 4px короче)
  
  // NEW кнопка
  int newX = startX;
  uint16_t newColor = (isInSectionSelector && sectionSel == 0) ? 0x07E0 : 
                      (currentSection == SECTION_NEW) ? 0x07E0 : 0x8410;
  uint16_t newBg = (isInSectionSelector && sectionSel == 0) ? 0x07E0 : 0x0000;
  uint16_t newFg = (isInSectionSelector && sectionSel == 0) ? 0x0000 : newColor;
  
  drawBox(newX, btnY, btnW, btnH, newColor);
  if (isInSectionSelector && sectionSel == 0) {
    d.fillRect(newX + 1, btnY + 1, btnW - 2, btnH - 2, newBg);
  }
  d.setCursor(newX + 25, btnY + 3);
  d.setTextColor(newFg, newBg);
  d.print("NEW");
  
  // Badge с количеством непрочитанных постов (красный кружок)
  if (hasUnreadPosts && lastNewPostCount > 0) {
    // Рисуем badge в правом верхнем углу кнопки NEW
    int badgeX = newX + btnW - 16;  // правый край - радиус
    int badgeY = btnY + 6;          // центр по вертикали
    drawNotificationBadge(badgeX, badgeY, lastNewPostCount);
  }
  
  // TOP кнопка
  int topX = newX + btnW + spacing;
  uint16_t topColor = (isInSectionSelector && sectionSel == 1) ? 0x07E0 : 
                      (currentSection == SECTION_TOP) ? 0x07E0 : 0x8410;
  uint16_t topBg = (isInSectionSelector && sectionSel == 1) ? 0x07E0 : 0x0000;
  uint16_t topFg = (isInSectionSelector && sectionSel == 1) ? 0x0000 : topColor;
  
  drawBox(topX, btnY, btnW, btnH, topColor);
  if (isInSectionSelector && sectionSel == 1) {
    d.fillRect(topX + 1, btnY + 1, btnW - 2, btnH - 2, topBg);
  }
  d.setCursor(topX + 27, btnY + 3);
  d.setTextColor(topFg, topBg);
  d.print("TOP");
  
  // YOU кнопка
  int youX = topX + btnW + spacing;
  uint16_t youColor = (isInSectionSelector && sectionSel == 2) ? 0x07E0 : 
                      (currentSection == SECTION_YOU) ? 0x07E0 : 0x8410;
  uint16_t youBg = (isInSectionSelector && sectionSel == 2) ? 0x07E0 : 0x0000;
  uint16_t youFg = (isInSectionSelector && sectionSel == 2) ? 0x0000 : youColor;
  
  drawBox(youX, btnY, btnW, btnH, youColor);
  if (isInSectionSelector && sectionSel == 2) {
    d.fillRect(youX + 1, btnY + 1, btnW - 2, btnH - 2, youBg);
  }
  d.setCursor(youX + 25, btnY + 3);
  d.setTextColor(youFg, youBg);
  d.print("YOU");
  
  // SOON кнопка (желтая, меньше, с символом #)
  int soonX = youX + btnW + spacing;
  uint16_t soonColor = (isInSectionSelector && sectionSel == 3) ? 0xFFE0 : 
                       (currentSection == SECTION_SOON) ? 0xFFE0 : 0x8410;
  uint16_t soonBg = (isInSectionSelector && sectionSel == 3) ? 0xFFE0 : 0x0000;
  uint16_t soonFg = (isInSectionSelector && sectionSel == 3) ? 0x0000 : soonColor;
  
  drawBox(soonX, btnY, soonBtnW, btnH, soonColor);
  if (isInSectionSelector && sectionSel == 3) {
    d.fillRect(soonX + 1, btnY + 1, soonBtnW - 2, btnH - 2, soonBg);
  }
  d.setCursor(soonX + 8, btnY + 3);
  d.setTextColor(soonFg, soonBg);
  d.print("#");
  
  // Profile info для YOU раздела (ПОСЛЕ кнопок)
  if (currentSection == SECTION_YOU) {
    int profileY = btnY + btnH + 4; // начальная Y для профиля
    
    // Имя пользователя
    d.setCursor(2, profileY);
    d.setTextColor(0x07FF, 0x0000); // cyan
    d.print("User:");
    d.setTextColor(0xFFFF, 0x0000); // white
    d.print(profile.author);
    
    // Device ID (короткий)
    d.setCursor(2, profileY + 10);
    d.setTextColor(0x8410, 0x0000); // gray
    d.print("ID:");
    d.setTextColor(0xFFFF, 0x0000); // white
    String shortId = deviceId.substring(0, min(16, (int)deviceId.length()));
    d.print(shortId);
    
    // Total Likes (ВЫШЕ Posts)
    d.setCursor(2, profileY + 20);
    drawHeart(4, profileY + 20, 0xF800);
    d.setCursor(10, profileY + 20);
    d.setTextColor(0x8410, 0x0000); // gray
    d.print("Total Likes:");
    d.setTextColor(0xF800, 0x0000); // red
    d.print(profile.totalLikes);
    
    // Recovery Code button (напротив Total Likes)
    d.setCursor(140, profileY + 20);
    d.setTextColor(0x07E0, 0x0000); // green
    d.print("[Opt+R]");
    d.setTextColor(0x8410, 0x0000); // gray
    d.print(" Code");
    
    // Posts (ниже Likes)
    d.setCursor(2, profileY + 30);
    d.setTextColor(0xFFE0, 0x0000); // yellow
    d.print("Posts:");
    d.setTextColor(0xFFFF, 0x0000);
    d.print(profile.postCount);
    
    // Settings button (напротив Posts)
    d.setCursor(140, profileY + 30);
    d.setTextColor(0xFFE0, 0x0000); // yellow
    d.print("[S]");
    d.setTextColor(0x8410, 0x0000); // gray
    d.print(" Settings");
  }
  
  // Loading индикатор
  if (isLoading) {
    d.setCursor(90, 85);
    d.setTextColor(0xFFE0, 0x0000);
    d.print("Loading...");
    return; // не рисуем посты пока грузится
  }
  
  // SOON раздел - показываем сообщение вместо постов
  if (currentSection == SECTION_SOON) {
    d.setCursor(90, 60);
    d.setTextColor(0xFFE0, 0x0000);
    d.println("Coming soon");
    d.setCursor(30, 75);
    d.setTextColor(0x8410, 0x0000);
    d.print("Press Enter to start animation");
    return;
  }
  
  // Отображаем посты начиная с scrollOffset
  // Для YOU раздела: 1 пост ниже (после профиля)
  // Для NEW/TOP: 2 поста сразу после кнопок
  int yPos = (currentSection == SECTION_YOU) ? 87 : 43;
  int postsToShow = (currentSection == SECTION_YOU) ? 1 : 2;
  int boxHeight = 42;
  int boxSpacing = 3;
  
  for (int i = 0; i < postsToShow; i++) {
    int feedIdx = scrollOffset + i;
    if (feedIdx >= (int)feedCount) break;
    
    Post& p = FEED[feedIdx];
    int boxY = yPos + i * (boxHeight + boxSpacing);
    
    // Рамка (зеленая для выбранного, серая для остальных)
    uint16_t boxColor = (feedIdx == sel && !isInSectionSelector) ? 0x07E0 : 0x8410;
    drawBox(2, boxY, 236, boxHeight, boxColor);
    
    // Автор (верхняя строка внутри блока) - голубой с темным фоном
    // Темный фон под именем (чтобы не сливалось с рамкой)
    int authorWidth = p.author.length() * 6 + 2; // ширина текста
    d.fillRect(5, boxY + 1, authorWidth, 9, 0x0000); // черный фон
    d.setCursor(6, boxY + 0);
    d.setTextColor(0x07FF, 0x0000); // cyan (голубой)
    d.print(p.author);
    
    // Текст поста (многострочный, до 3 строк)
    d.setTextColor(0xFFFF, 0x0000);
    String txt = p.text;
    int lineY = boxY + 10;
    int charsPerLine = 38;
    for (int line = 0; line < 3 && txt.length() > 0; line++) {
      d.setCursor(6, lineY);
      String part = txt.substring(0, min((int)txt.length(), charsPerLine));
      d.print(part);
      txt = txt.substring(min((int)txt.length(), charsPerLine));
      lineY += 8;
    }
    
    // Heart + likes count (bottom left)
    d.fillRect(7, boxY + 32, 12, 10, 0x0000);
    drawHeart(11, boxY + 36, 0xF800); // +2px вниз (было 36)
    d.setCursor(18, boxY + 34);
    d.setTextColor(0xF800, 0x0000);
    d.print(p.likes);
    
    // Comment bubble + count (всегда показываем, как лайки)
    int commentX = 38; // позиция после лайков
    d.fillRect(commentX, boxY + 32, 12, 10, 0x0000); // темный фон
    drawCommentBubble(commentX + 2, boxY + 35, 0x07FF); // +2px вниз (было 33)
    d.setCursor(commentX + 9, boxY + 34);
    d.setTextColor(0x07FF, 0x0000); // cyan
    d.print(p.comments);
    
    // Post creation date (bottom right)
    if (p.created_at.length() >= 19) {
      String date = p.created_at.substring(8, 10) + "." +
                    p.created_at.substring(5, 7) + "." +
                    p.created_at.substring(2, 4) + " " +
                    p.created_at.substring(11, 16);
      d.setCursor(150, boxY + 34);
      d.setTextColor(0x8410, 0x0000);
      d.print(date);
    }
  }
}

void setup() {
  auto cfg=M5.config(); M5Cardputer.begin(cfg,true);
  M5Cardputer.Display.setRotation(1); M5Cardputer.Display.setTextSize(1);
  
  // Инициализация таймера активности
  lastActivityTime = millis();
  
  // Инициализация RGB LED
  initLED();
  
  // Получаем реальный уровень батареи сразу при старте
  batteryLevel = M5Cardputer.Power.getBatteryLevel();
  if (batteryLevel < 0) batteryLevel = 0;
  if (batteryLevel > 100) batteryLevel = 100;
  lastBatteryCheck = millis();
  
  // Загрузка настроек уведомлений
  loadNotificationSettings();
  
  // Показываем заставку
  showSplashScreen();
  
  // ВАЖНО: сначала WiFi, потом регистрация!
  wifiLoadOrOnboard();
  ensureDevice();
  
  if (wifiOK) {
    apiStats();
    apiFeed();
  }
  drawUI();
}
void loop(){
  M5Cardputer.update();
  wifiOK = (WiFi.status()==WL_CONNECTED);
  
  // Периодическая проверка уровня батареи
  updateBatteryLevel();
  
  // Проверка новых постов (фоновая)
  checkForNewPosts();
  
  // Обновление мигания LED для уведомлений
  updateNotificationLED();
  
  // Проверка таймаута экрана
  if (screenOn && (millis() - lastActivityTime > SCREEN_TIMEOUT)) {
    screenSleep();
  }
  
  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()){
    // Если экран выключен - включаем его и игнорируем нажатие
    if (!screenOn) {
      screenWake();
      return;
    }
    
    // Обновляем время последней активности при любом нажатии
    // (включая стрелки ;./,, Enter, все клавиши управления)
    lastActivityTime = millis();

    // Если открыт донатное окно QR — закрываем по любому нажатию
    if (showingDonateQR) {
      showingDonateQR = false;
      drawUI();
      return;
    }
    
    // ОЧИСТКА УСТРОЙСТВА: Fn+Opt+Del (сложная комбинация для безопасности)
    if (M5Cardputer.Keyboard.isKeyPressed(KEY_FN) && 
        M5Cardputer.Keyboard.isKeyPressed(KEY_OPT) && 
        M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
      
      // Показываем предупреждение
      auto& d = M5Cardputer.Display;
      d.fillScreen(0x0000);
      d.setTextSize(1);
      d.setCursor(40, 30);
      d.setTextColor(0xF800, 0x0000); // красный
      d.println("⚠️ CLEAR DEVICE ⚠️");
      d.setCursor(20, 50);
      d.setTextColor(0xFFFF, 0x0000);
      d.println("This will delete:");
      d.setCursor(20, 60);
      d.println("- Username");
      d.setCursor(20, 70);
      d.println("- WiFi networks");
      d.setCursor(20, 80);
      d.println("- All settings");
      d.setCursor(20, 100);
      d.setTextColor(0xFFE0, 0x0000);
      d.print("[Enter]");
      d.setTextColor(0xFFFF, 0x0000);
      d.print(" Confirm  ");
      d.setTextColor(0xFFE0, 0x0000);
      d.print("[ESC]");
      d.setTextColor(0xFFFF, 0x0000);
      d.print(" Cancel");
      
      // Ждем подтверждения
      delay(300);
      while (M5Cardputer.Keyboard.isPressed()) { M5Cardputer.update(); delay(10); }
      
      while (true) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
          if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
            // Подтверждено - очищаем все
            prefs.begin("microcast", false);
            prefs.clear();
            prefs.end();
            
            d.fillScreen(0x0000);
            d.setCursor(60, 60);
            d.setTextColor(0x07E0, 0x0000);
            d.println("Device cleared!");
            d.setCursor(40, 80);
            d.setTextColor(0xFFFF, 0x0000);
            d.println("Restarting...");
            delay(2000);
            ESP.restart();
          }
          if (M5Cardputer.Keyboard.isKeyPressed(KEY_ESC) || M5Cardputer.Keyboard.isKeyPressed('`')) {
            // Отменено
            drawUI();
            break;
          }
        }
        delay(10);
      }
      return;
    }
    
    // Donate QR — открыть по клавише M
    if (M5Cardputer.Keyboard.isKeyPressed('M') || M5Cardputer.Keyboard.isKeyPressed('m')) {
      showingDonateQR = true;
      drawDonateQRPopup();
      return;
    }

    // Settings окно - toggle (только в разделе YOU)
    if ((M5Cardputer.Keyboard.isKeyPressed('S') || M5Cardputer.Keyboard.isKeyPressed('s')) 
        && currentSection == SECTION_YOU && !viewingComments && !showingInfo) {
      showingSettings = !showingSettings;
      settingsMenuSelection = 0;
      inSettingsSubmenu = false;
      if (showingSettings) drawSettingsMainMenu();
      else drawUI();
      return;
    }
    
    // Обработка клавиш в Settings меню
    if (showingSettings) {
      // ESC - выход или возврат назад
      if (M5Cardputer.Keyboard.isKeyPressed(KEY_ESC) || M5Cardputer.Keyboard.isKeyPressed('`')) {
        if (inSettingsSubmenu) {
          // Возврат в главное меню Settings
          inSettingsSubmenu = false;
          saveNotificationSettings();
          drawSettingsMainMenu();
        } else {
          // Выход из Settings
          showingSettings = false;
          drawUI();
        }
        return;
      }
      
      // Главное меню Settings
      if (!inSettingsSubmenu) {
        // Навигация
        if (keyUpPressed() || M5Cardputer.Keyboard.isKeyPressed('W') || M5Cardputer.Keyboard.isKeyPressed('w')) {
          settingsMenuSelection = (settingsMenuSelection > 0) ? settingsMenuSelection - 1 : 0;
          drawSettingsMainMenu();
          return;
        }
        if (keyDownPressed() || M5Cardputer.Keyboard.isKeyPressed('A') || M5Cardputer.Keyboard.isKeyPressed('a')) {
          settingsMenuSelection = (settingsMenuSelection < 1) ? settingsMenuSelection + 1 : 1;
          drawSettingsMainMenu();
          return;
        }
        
        // Enter - войти в подменю
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
          inSettingsSubmenu = true;
          wifiListSelection = 0;
          if (settingsMenuSelection == 0) {
            drawNotificationsSettings();
          } else {
            loadSavedNetworks(); // Обновляем список
            drawWiFiListSettings();
          }
          return;
        }
        return;
      }
      
      // Подменю: Notifications
      if (settingsMenuSelection == 0) {
        // Toggle notifications
        if (M5Cardputer.Keyboard.isKeyPressed('1')) {
          notificationsEnabled = !notificationsEnabled;
          if (!notificationsEnabled) {
            hasUnreadPosts = false;
            ledOff();
          }
          drawNotificationsSettings();
          return;
        }
        
        // Toggle sound
        if (M5Cardputer.Keyboard.isKeyPressed('2')) {
          notificationSound = !notificationSound;
          drawNotificationsSettings();
          return;
        }
        
        // Toggle LED
        if (M5Cardputer.Keyboard.isKeyPressed('3')) {
          notificationLED = !notificationLED;
          if (!notificationLED) ledOff();
          drawNotificationsSettings();
          return;
        }
        
        // Increase interval
        if (M5Cardputer.Keyboard.isKeyPressed('+') || M5Cardputer.Keyboard.isKeyPressed('=')) {
          if (notificationInterval < 5) notificationInterval = 5;
          else if (notificationInterval < 10) notificationInterval = 10;
          else if (notificationInterval < 15) notificationInterval = 15;
          else if (notificationInterval < 30) notificationInterval = 30;
          else if (notificationInterval < 60) notificationInterval = 60;
          drawNotificationsSettings();
          return;
        }
        
        // Decrease interval
        if (M5Cardputer.Keyboard.isKeyPressed('-') || M5Cardputer.Keyboard.isKeyPressed('_')) {
          if (notificationInterval > 30) notificationInterval = 30;
          else if (notificationInterval > 15) notificationInterval = 15;
          else if (notificationInterval > 10) notificationInterval = 10;
          else if (notificationInterval > 5) notificationInterval = 5;
          else if (notificationInterval > 1) notificationInterval = 1;
          drawNotificationsSettings();
          return;
        }
        return;
      }
      
      // Подменю: WiFi List
      if (settingsMenuSelection == 1) {
        // Навигация по списку (только ; и .)
        if (keyUpPressed()) {
          if (wifiListSelection > 0) wifiListSelection--;
          drawWiFiListSettings();
          return;
        }
        if (keyDownPressed()) {
          if (wifiListSelection < savedNetworksCount - 1) wifiListSelection++;
          drawWiFiListSettings();
          return;
        }
        
        // [A] Add new network - открывает список доступных сетей
        if (M5Cardputer.Keyboard.isKeyPressed('A') || M5Cardputer.Keyboard.isKeyPressed('a')) {
          // Запускаем выбор новой сети
          bool success = wifiOnboarding();
          if (success) {
            // Успешно подключились и добавили сеть
            loadSavedNetworks(); // Обновляем список
            wifiListSelection = savedNetworksCount - 1; // Выбираем последнюю (новую)
          }
          // Если нажали ESC - просто возвращаемся в меню
          drawWiFiListSettings();
          return;
        }
        
        // [D] Delete selected
        if ((M5Cardputer.Keyboard.isKeyPressed('D') || M5Cardputer.Keyboard.isKeyPressed('d')) && savedNetworksCount > 0) {
          // Удаляем выбранную сеть
          for (int i = wifiListSelection; i < savedNetworksCount - 1; i++) {
            savedNetworks[i] = savedNetworks[i + 1];
          }
          savedNetworksCount--;
          if (wifiListSelection >= savedNetworksCount && wifiListSelection > 0) {
            wifiListSelection--;
          }
          saveNetworks();
          drawWiFiListSettings();
          return;
        }
        return;
      }
      
      return;
    }
    
    // Info окно - toggle
    if (M5Cardputer.Keyboard.isKeyPressed('I') || M5Cardputer.Keyboard.isKeyPressed('i')) {
      showingInfo = !showingInfo;
      if (showingInfo) drawInfoPopup();
      else drawUI();
      return;
    }
    
    // Закрытие Info окна любой клавишей
    if (showingInfo) {
      showingInfo = false;
      drawUI();
      return;
    }
    
    // Открытие комментариев (клавиша C)
    if ((M5Cardputer.Keyboard.isKeyPressed('C') || M5Cardputer.Keyboard.isKeyPressed('c')) 
        && !viewingComments && !isInSectionSelector && feedCount > 0) {
      // Открываем комментарии для текущего поста
      currentPostId = FEED[sel].id;
      viewingComments = true;
      commentScrollOffset = 0;
      commentSel = 0;
      commentTextScroll = 0; // сброс горизонтального скролла
      
      // Загружаем комментарии
      if (wifiOK) {
        apiComments(currentPostId);
      }
      drawCommentsView();
      return;
    }
    
    // Закрытие комментариев (ESC в режиме комментариев)
    if (viewingComments && (M5Cardputer.Keyboard.isKeyPressed(KEY_ESC) || M5Cardputer.Keyboard.isKeyPressed('`'))) {
      viewingComments = false;
      currentPostId = "";
      drawUI();
      return;
    }
    
    // Написать комментарий (Fn+Enter в режиме комментариев)
    if (viewingComments && M5Cardputer.Keyboard.isKeyPressed(KEY_FN) && M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
      // Ввод комментария с подтверждением
      String commentText = "";
      bool cancelled = false;
      
      while (true) {
        commentText = promptInput("Comment (3-120)", 120, false, commentText);
        
        if (commentText.length() == 0) {
          cancelled = true;
          break;
        }
        
        if (commentText.length() < 3) {
          showMessage("Error", "Comment too short", "Min 3 chars required", "", 2);
          delay(2000);
          continue;
        }
        
        // Подтверждение
        int confirm = confirmAction(commentText, "Post comment");
        
        if (confirm == 1) {
          // Отправляем
          if (wifiOK && apiComment(currentPostId, commentText)) {
            // Перезагружаем комментарии
            apiComments(currentPostId);
            // Обновляем feed чтобы увидеть новый счетчик
            refreshCurrentSectionSilent();
          }
          lastActivityTime = millis(); // Обновляем время активности чтобы экран не тух
          break;
        } else if (confirm == 2) {
          // Редактировать
          continue;
        } else {
          // Отменено
          cancelled = true;
          break;
        }
      }
      
      drawCommentsView();
      return;
    }
    
    // WiFi - add new network (не отключаясь от текущей)
    if (M5Cardputer.Keyboard.isKeyPressed('N') || M5Cardputer.Keyboard.isKeyPressed('n')) { 
      // Просто добавляем новую сеть, не отключаясь
      if (wifiOnboarding()) {
        // Успешно добавили новую сеть
        if (wifiOK) { 
          apiStats(); 
          loadCurrentSection(); 
        }
      }
      // Если нажали ESC - просто возвращаемся, Wi-Fi остается подключенным
      drawUI(); 
    }
    
    // Навигация: вверх/вниз
    else if (keyUpPressed()) {
      if (viewingComments) {
        // Выбор предыдущего комментария
        if (commentSel > 0) {
          commentSel--;
          commentTextScroll = 0; // сброс горизонтальной прокрутки
          // Корректировка вертикального скролла
          if (commentSel < commentScrollOffset) {
            commentScrollOffset = commentSel;
          }
          drawCommentsView();
        }
      } else if (isInSectionSelector) {
        // Из выбора разделов переходим к заголовку MicroCast
        isInSectionSelector = false;
        isInHeaderSelector = true;
        drawUI();
      } else if (sel == 0 && scrollOffset == 0) {
        // Если на первом посту - переходим к выбору разделов
        isInSectionSelector = true;
        sectionSel = (int)currentSection; // текущий раздел выбран
        drawUI();
      } else {
        // Обычная прокрутка постов вверх
        sel = max(0, sel-1);
        if (sel < scrollOffset) scrollOffset = sel;
        drawUI();
      }
    }
    else if (keyDownPressed()) {
      if (viewingComments) {
        // Выбор следующего комментария
        if (commentSel < (int)commentCount - 1) {
          commentSel++;
          commentTextScroll = 0; // сброс горизонтальной прокрутки
          // Корректировка вертикального скролла
          int commentsPerScreen = 3;
          if (commentSel >= commentScrollOffset + commentsPerScreen) {
            commentScrollOffset = commentSel - (commentsPerScreen - 1);
          }
          drawCommentsView();
        }
      } else if (isInHeaderSelector) {
        // Из заголовка — в выбор разделов
        isInHeaderSelector = false;
        isInSectionSelector = true;
        drawUI();
      } else if (isInSectionSelector) {
        // Из выбора разделов - возвращаемся к постам
        isInSectionSelector = false;
        drawUI();
      } else {
        // Обычная прокрутка постов вниз
        sel = min((int)feedCount-1, sel+1);
        // Для YOU раздела показываем 1 пост, для остальных 2
        int visiblePosts = (currentSection == SECTION_YOU) ? 1 : 2;
        if (sel >= scrollOffset + visiblePosts) scrollOffset = sel - (visiblePosts - 1);
        drawUI();
      }
    }
    
    // Навигация: влево/вправо (переключение разделов или скролл текста комментария)
    else if (keyLeftPressed()) {
      if (viewingComments) {
        // Горизонтальный скролл текста комментария влево
        if (commentSel < commentCount && COMMENTS[commentSel].text.length() > 39) {
          commentTextScroll = max(0, commentTextScroll - 5); // скролл по 5 символов
          drawCommentsView();
        }
      } else if (isInSectionSelector) {
        sectionSel = max(0, sectionSel - 1);
        drawUI();
      }
    }
    else if (keyRightPressed()) {
      if (viewingComments) {
        // Горизонтальный скролл текста комментария вправо
        if (commentSel < commentCount && COMMENTS[commentSel].text.length() > 39) {
          // Когда обе стрелки < и > показываем 37 символов текста (39 - 2)
          // maxScroll должен быть = length - 37, чтобы дойти до конца
          int maxScroll = COMMENTS[commentSel].text.length() - 37;
          commentTextScroll = min(maxScroll, commentTextScroll + 5); // скролл по 5 символов
          drawCommentsView();
        }
      } else if (isInSectionSelector) {
        sectionSel = min(3, sectionSel + 1); // теперь 4 раздела (0-3)
        drawUI();
      }
    }
    
#ifdef KEY_FN
    else if (M5Cardputer.Keyboard.isKeyPressed(KEY_FN) && M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
#else
    else if (M5Cardputer.Keyboard.isKeyPressed(KEY_TAB) && M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
#endif
      // Создание поста с подтверждением
      String t = "";
      bool postCancelled = false;
      
      while (true) {
        t = promptInput("New post (max 120)", 120, false, t);
        
        if (t.length() == 0) {
          postCancelled = true;
          break; // отменено
        }
        
        if (t.length() < 3) {
          // Слишком короткий пост
          showMessage("Error", "Post too short", "Min 3 chars required", "", 2);
          delay(2000);
          continue; // повторный ввод
        }
        
        // Показываем подтверждение
        int confirm = confirmAction(t, "Post");
        
        if (confirm == 1) {
          // Подтверждено - отправляем
          if (wifiOK) { 
            apiPost(t); 
            apiStats(); 
            refreshCurrentSectionSilent(); // тихое обновление
          }
          lastActivityTime = millis(); // Обновляем время активности чтобы экран не тух
          break;
        } else if (confirm == 2) {
          // Редактировать - повторяем цикл с текущим текстом
          continue;
        } else {
          // Отменено
          postCancelled = true;
          break;
        }
      }
      
      drawUI();
    }
    else if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
      if (isInHeaderSelector) {
        // Enter на заголовке — открыть донат
        showingDonateQR = true;
        drawDonateQRPopup();
        return;
      }
      if (isInSectionSelector) {
        // Подтверждение выбора раздела
        currentSection = (Section)sectionSel;
        isInSectionSelector = false;
        loadCurrentSection();
        // Сбрасываем уведомления если переключились на NEW
        if (currentSection == SECTION_NEW) {
          markPostsAsRead();
        }
      } else if (currentSection == SECTION_SOON) {
        // В разделе SOON - запуск анимации
        drawSpaceMatrix();
        drawUI();
      } else if (feedCount>0 && wifiOK && !isInSectionSelector) {
        // Лайк поста - тихое обновление без сброса позиции
        apiLike(FEED[sel].id);
        // Обновляем ленту тихо (без Loading, сохраняя позицию)
        refreshCurrentSectionSilent();
      } else {
        drawUI();
      }
    }
    else if (M5Cardputer.Keyboard.isKeyPressed('R') || M5Cardputer.Keyboard.isKeyPressed('r')) {
      // [R] в обычном режиме - Refresh
      // Но если нажат Opt+R - показываем recovery code
      if (M5Cardputer.Keyboard.isKeyPressed(KEY_OPT)) {
        // Opt+R - показать recovery code (ТОЛЬКО из локального хранилища!)
        // БЕЗОПАСНО: код НЕ запрашивается с сервера, читается из Preferences
        String code = recoveryCode;
        
        // Если код не загружен в память - читаем из Preferences
        if (code.length() == 0) {
          prefs.begin("microcast", false);
          code = prefs.getString("recovery_code", "");
          prefs.end();
          
          if (code.length() > 0) {
            recoveryCode = code; // кэшируем в памяти
          }
        }
        
        if (code.length() > 0) {
          showRecoveryCodePopup(code);
          drawUI();
        } else {
          showMessage("Error", "Recovery code not found", "Register account first", "", 2);
          delay(1000);
          while (M5Cardputer.Keyboard.isPressed()) { M5Cardputer.update(); delay(10); }
          while (!M5Cardputer.Keyboard.isChange()) { M5Cardputer.update(); delay(10); }
          drawUI();
        }
      } else {
        // Обычный R - refresh feed
        if (wifiOK) { 
          apiStats(); 
          loadCurrentSection();
          // Сбрасываем уведомления при обновлении ленты
          markPostsAsRead();
        }
        drawUI();
      }
    }
    else if (M5Cardputer.Keyboard.isKeyPressed('U') || M5Cardputer.Keyboard.isKeyPressed('u')) {
      // Смена username с подтверждением
      String newName = "";
      bool nameCancelled = false;
      
      while (true) {
        newName = promptInput("New username (3-24)", 24, false, newName);
        
        if (newName.length() == 0) {
          nameCancelled = true;
          break; // отменено
        }
        
        if (newName.length() < 3) {
          // Слишком короткий username
          showMessage("Error", "Username too short", "Min 3 chars required", "", 2);
          delay(1000);
          while (M5Cardputer.Keyboard.isPressed()) { M5Cardputer.update(); delay(10); }
          while (!M5Cardputer.Keyboard.isChange()) { M5Cardputer.update(); delay(10); }
          continue; // повторный ввод
        }
        
        // Показываем подтверждение
        int confirm = confirmAction(newName, "Change username to");
        
        if (confirm == 1) {
          // Подтверждено - меняем
          if (wifiOK) {
            String errorMsg = "";
            bool success = false;
            
            // Try to change name first
            if (apiChangeName(newName, errorMsg)) {
              success = true;
            } else if (errorMsg == "Device not found") {
              // Device not registered yet - register it
              if (apiRegister(newName)) {
                success = true;
                errorMsg = "";
              } else {
                errorMsg = "Registration failed";
              }
            }
            
            if (success) {
              showMessage("Success!", "Username updated:", author, "", 1);
              delay(1000);
              while (M5Cardputer.Keyboard.isPressed()) { M5Cardputer.update(); delay(10); }
              while (!M5Cardputer.Keyboard.isChange()) { M5Cardputer.update(); delay(10); }
            } else {
              showMessage("Error", errorMsg, "", "", 2);
              delay(1000);
              while (M5Cardputer.Keyboard.isPressed()) { M5Cardputer.update(); delay(10); }
              while (!M5Cardputer.Keyboard.isChange()) { M5Cardputer.update(); delay(10); }
            }
          }
          break;
        } else if (confirm == 2) {
          // Редактировать - повторяем цикл с текущим текстом
          continue;
        } else {
          // Отменено
          nameCancelled = true;
          break;
        }
      }
      
      drawUI();
    }
#ifdef KEY_FN
    else if (M5Cardputer.Keyboard.isKeyPressed(KEY_FN) && (M5Cardputer.Keyboard.isKeyPressed('C') || M5Cardputer.Keyboard.isKeyPressed('c'))) {
#else
    else if (M5Cardputer.Keyboard.isKeyPressed(KEY_TAB) && (M5Cardputer.Keyboard.isKeyPressed('C') || M5Cardputer.Keyboard.isKeyPressed('c'))) {
#endif
      auto& d = M5Cardputer.Display;
      d.fillScreen(0x0000);
      d.setTextSize(1);
      
      // Предупреждение
      d.setCursor(0, 0);
      d.setTextColor(0xF800, 0x0000);
      d.println("Clear All Settings");
      d.setTextColor(0xFFFF, 0x0000);
      d.println("----------------------------------------");
      
      drawBox(4, 20, 232, 60, 0xF800);
      d.setCursor(8, 28);
      d.setTextColor(0xFFFF, 0x0000);
      d.println("  This will erase:");
      d.setCursor(8, 40);
      d.println("  - Device ID");
      d.setCursor(8, 52);
      d.println("  - Username");
      d.setCursor(8, 64);
      d.println("  - WiFi settings");
      
      d.setCursor(0, 110);
      d.setTextColor(0xFFE0, 0x0000);
      d.print("[Enter]");
      d.setTextColor(0x8410, 0x0000);
      d.print(" Confirm  ");
      d.setTextColor(0xFFE0, 0x0000);
      d.print("[`]");
      d.setTextColor(0x8410, 0x0000);
      d.println(" Cancel");
      
      while (true) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
          if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
            prefs.begin("microcast", false);
            prefs.clear();
            prefs.end();
            showMessage("Cleared!", "All data erased", "Please restart device", "(unplug & plug back)", 1);
            while(true) { delay(100); } // Halt
          }
          if (M5Cardputer.Keyboard.isKeyPressed(KEY_ESC) || M5Cardputer.Keyboard.isKeyPressed('`')) {
            drawUI();
            break;
          }
        }
        delay(10);
      }
    }
  }
  delay(8);
}