/*
 * MicroCast beta0.4 for M5Cardputer — Client (Wi-Fi onboarding + Proxy)
 * 
 * Features:
 *   - Username registration system (unique names, bound to device_id)
 *   - 2 posts per screen with scrolling
 *   - Like/unlike posts (toggle)
 *   - Comments view with scrollable text (3 comments visible)
 *   - Modern UI with borders, hearts, WiFi indicator
 *   - Auto screen sleep after 60s of inactivity (power saving)
 * 
 * Controls:
 *   Feed: ; or . (arrows) or W/A - scroll (2 posts visible)
 *         Enter - toggle like
 *         C - view comments
 *         Fn+Enter (or Tab+Enter) - new post
 *         R - refresh feed & stats
 *         U - change username
 *         N - change Wi-Fi network
 *   Comments: ; . (up/down) - select comment, , / (left/right) - scroll text
 *             Fn+Enter - write comment, ESC - back to feed
 *   Text Input: Enter - submit, Esc - cancel, Del/Backspace - delete char
 *               All printable chars including . and ; are now allowed
 *   WiFi Select: ; . (arrows) or W/A - navigate, Enter - select, R - rescan, Esc - cancel
 *   Screen: Auto-sleep after 60s, any key to wake up
 */

#include <M5Cardputer.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>

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
Preferences prefs;
int deviceCount = 0; // total registered devices

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
  d.println("========================================");
  
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
  d.println("========================================");
  
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
  d.println("========================================");
  
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

// ====== Wi-Fi onboarding ======
int scanAndSelectNetwork(String& outSsid, bool& openNet) {
  auto& d = M5Cardputer.Display;
  
  // Scanning screen
  d.fillScreen(0x0000);
  d.setTextSize(1);
  d.setCursor(0, 0);
  d.setTextColor(0x07FF, 0x0000);
  d.println("WiFi Setup");
  d.setTextColor(0xFFFF, 0x0000);
  d.println("========================================");
  
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
    d.println("========================================");
    
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
      if (keyUpPressed()) {
        idx = (idx > 0) ? idx - 1 : 0;
        break;
      }
      if (keyDownPressed()) {
        idx = (idx < n - 1) ? idx + 1 : n - 1;
        break;
      }
      if (M5Cardputer.Keyboard.isKeyPressed('R') || M5Cardputer.Keyboard.isKeyPressed('r')) {
        return scanAndSelectNetwork(outSsid, openNet);
      }
      if (M5Cardputer.Keyboard.isKeyPressed(KEY_ESC) || M5Cardputer.Keyboard.isKeyPressed('`')) {
        return -1;
      }
      if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
        outSsid = WiFi.SSID(idx);
        openNet = (WiFi.encryptionType(idx) == WIFI_AUTH_OPEN);
        return idx;
      }
    }
  }
}
bool tryConnectWiFi(const String& ssid, const String& pass, int tries=30){
  auto& d = M5Cardputer.Display;
  d.fillScreen(0x0000);
  d.setTextSize(1);
  
  // Заголовок
  d.setCursor(0, 0);
  d.setTextColor(0x07FF, 0x0000);
  d.println("WiFi Connection");
  d.setTextColor(0xFFFF, 0x0000);
  d.println("========================================");
  
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
  if (ok){ prefs.begin("microcast", false); prefs.putString("ssid", ssid); prefs.putString("pass", pass); prefs.end(); }
  return ok;
}
void wifiLoadOrOnboard(){
  prefs.begin("microcast", false);
  String ssid = prefs.getString("ssid", ""); String pass = prefs.getString("pass", "");
  prefs.end();
  if (ssid.length()==0){ wifiOK = wifiOnboarding(); }
  else { wifiOK = tryConnectWiFi(ssid, pass); if (!wifiOK) wifiOK = wifiOnboarding(); }
}
void wifiForgetAndReconfig(){
  prefs.begin("microcast", false); prefs.remove("ssid"); prefs.remove("pass"); prefs.end();
  WiFi.disconnect(true,true); delay(300); wifiOK = wifiOnboarding();
}

// ====== Device / Author ======
void ensureDevice() {
  prefs.begin("microcast", false);
  deviceId = prefs.getString("dev", "");
  author   = prefs.getString("author", "");
  if (deviceId.length() < 8) {
    uint32_t r = esp_random();
    deviceId = String((uint32_t)millis(), HEX) + "-" + String(r, HEX);
    prefs.putString("dev", deviceId);
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
        while (!M5Cardputer.Keyboard.isChange()) { M5Cardputer.update(); delay(10); }
        continue;
      }
      // Try to register (WiFi already connected at this point)
      if (apiRegister(newAuthor)) {
        showMessage("Success!", "Username registered:", author, "", 1);
        while (!M5Cardputer.Keyboard.isChange()) { M5Cardputer.update(); delay(10); }
        break;
      } else {
        showMessage("Registration Failed", "Username may be taken", "or server error", "ESC=Skip, Any=Retry", 2);
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
  StaticJsonDocument<256> doc; DeserializationError e = deserializeJson(doc, resp);
  if (e) return false;
  deviceCount = (int)(doc["devices"]|0);
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

// ====== Info Popup ======
void drawInfoPopup() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(0x0000);
  d.setTextSize(1);
  
  // Центральное окно с рамкой
  drawBox(10, 20, 220, 95, 0x07FF); // cyan border
  
  // Заголовок
  d.setCursor(95, 25);
  d.setTextColor(0x07FF, 0x0000);
  d.println("Controls");
  
  // Подсказки
  d.setCursor(15, 38);
  d.setTextColor(0xFFE0, 0x0000); // yellow
  d.print("[Up/Dn]");
  d.setTextColor(0xFFFF, 0x0000); // white
  d.print(" Scroll  ");
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[Enter]");
  d.setTextColor(0xFFFF, 0x0000);
  d.print(" Like");
  
  d.setCursor(15, 48);
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[Lt/Rt]");
  d.setTextColor(0xFFFF, 0x0000);
  d.print(" Sections  ");
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[R]");
  d.setTextColor(0xFFFF, 0x0000);
  d.print(" Refresh");
  
  d.setCursor(15, 58);
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[Fn+Enter]");
  d.setTextColor(0xFFFF, 0x0000);
  d.print(" Post  ");
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[C]");
  d.setTextColor(0xFFFF, 0x0000);
  d.print(" Comments");
  
  d.setCursor(15, 68);
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[U]");
  d.setTextColor(0xFFFF, 0x0000);
  d.print(" Username  ");
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[N]");
  d.setTextColor(0xFFFF, 0x0000);
  d.print(" WiFi  ");
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[I]");
  d.setTextColor(0xFFFF, 0x0000);
  d.print(" Info");
  
  // Разделы
  d.setCursor(15, 80);
  d.setTextColor(0xFFE0, 0x0000);
  d.print("NEW");
  d.setTextColor(0x8410, 0x0000);
  d.print(" latest  ");
  d.setTextColor(0xFFE0, 0x0000);
  d.print("TOP");
  d.setTextColor(0x8410, 0x0000);
  d.print(" best  ");
  d.setTextColor(0xFFE0, 0x0000);
  d.print("YOU");
  d.setTextColor(0x8410, 0x0000);
  d.print(" profile");
  
  d.setCursor(15, 92);
  d.setTextColor(0xFFE0, 0x0000);
  d.print("SOON");
  d.setTextColor(0x8410, 0x0000);
  d.print(" space matrix animation");
  
  // Подсказка закрытия
  d.setCursor(50, 103);
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
      int maxChars = 38;
      
      if (isSelected && commentText.length() > maxChars) {
        // Для выбранного комментария - показываем с прокруткой
        int scrollPos = min(commentTextScroll, max(0, (int)commentText.length() - maxChars));
        String visibleText = commentText.substring(scrollPos, min((int)commentText.length(), scrollPos + maxChars));
        
        // Индикатор прокрутки влево
        if (scrollPos > 0) {
          d.setTextColor(0xFFE0, 0x0000);
          d.print("<");
          d.setTextColor(0xFFFF, 0x0000);
          d.print(visibleText.substring(0, maxChars - 2));
        } else {
          d.print(visibleText.substring(0, maxChars - 1));
        }
        
        // Индикатор прокрутки вправо
        if (scrollPos + maxChars < commentText.length()) {
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
  
  // Строка 1: "MicroCast [156]" (циан) + "beta0.1" (красный) справа
  d.setTextColor(0x07FF, 0x0000); // cyan
  d.print("MicroCast [");
  d.setTextColor(0xF800, 0x0000); // red
  d.print(deviceCount);
  d.setTextColor(0x07FF, 0x0000); // cyan
  d.print("]");
  
  // beta0.4 справа
  d.setCursor(190, 0);
  d.setTextColor(0xF800, 0x0000); // red
  d.println("beta0.4");
  
  // Строка 2: подчеркивание
  d.setCursor(0, 8);
  d.setTextColor(0xFFFF, 0x0000); // white
  d.println("========================================");
  
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
    
    // Posts (ниже Likes)
    d.setCursor(2, profileY + 30);
    d.setTextColor(0xFFE0, 0x0000); // yellow
    d.print("Posts:");
    d.setTextColor(0xFFFF, 0x0000);
    d.print(profile.postCount);
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
    lastActivityTime = millis();
    
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
    
    // WiFi reconfig
    if (M5Cardputer.Keyboard.isKeyPressed('N') || M5Cardputer.Keyboard.isKeyPressed('n')) { 
      wifiForgetAndReconfig(); 
      if (wifiOK) { apiStats(); loadCurrentSection(); }
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
        // В режиме выбора разделов - выходим к постам
        isInSectionSelector = false;
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
        if (commentSel < commentCount && COMMENTS[commentSel].text.length() > 38) {
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
        if (commentSel < commentCount && COMMENTS[commentSel].text.length() > 38) {
          int maxScroll = COMMENTS[commentSel].text.length() - 38;
          commentTextScroll = min(maxScroll, commentTextScroll + 5); // скролл по 5 символов
          drawCommentsView();
        }
      } else if (isInSectionSelector) {
        sectionSel = min(3, sectionSel + 1); // теперь 4 раздела (0-3)
        drawUI();
      }
    }
    
    // Refresh
    else if (M5Cardputer.Keyboard.isKeyPressed('R') || M5Cardputer.Keyboard.isKeyPressed('r')) { 
      if (wifiOK) { 
        apiStats(); 
        loadCurrentSection();
      }
      drawUI(); 
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
      if (isInSectionSelector) {
        // Подтверждение выбора раздела
        currentSection = (Section)sectionSel;
        isInSectionSelector = false;
        loadCurrentSection();
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
          delay(2000);
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
              while (!M5Cardputer.Keyboard.isChange()) { M5Cardputer.update(); delay(10); }
            } else {
              showMessage("Error", errorMsg, "", "", 2);
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
      d.println("========================================");
      
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