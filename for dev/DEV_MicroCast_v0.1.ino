/*
 * MicroCast v0.1 for M5Cardputer — Client (Wi-Fi onboarding + Proxy)
 * 
 * Features:
 *   - Username registration system (unique names, bound to device_id)
 *   - 2 posts per screen with scrolling
 *   - Like/unlike posts (toggle)
 *   - Modern UI with borders, hearts, WiFi indicator
 * 
 * Controls:
 *   Feed: ; or . (arrows) or W/A - scroll (2 posts visible)
 *         Enter - toggle like
 *         Fn+Enter (or Tab+Enter) - new post
 *         R - refresh feed & stats
 *         U - change username
 *         N - change Wi-Fi network
 *   Text Input: Enter - submit, Esc - cancel, Del/Backspace - delete char
 *   WiFi Select: ; . (arrows) or W/A - navigate, Enter - select, R - rescan, Esc - cancel
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

struct Post { String id, author, text, created_at; int likes = 0; };
static const size_t FEED_MAX = 50;
Post FEED[FEED_MAX];
size_t feedCount = 0;
int sel = 0; // selected post index in FEED
int scrollOffset = 0; // scroll offset for displaying 2 posts

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

String promptInput(const String& title, size_t maxLen, bool mask=false) {
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
  d.print("[,/]");
  d.setTextColor(0x8410, 0x0000);
  d.print(" Move  ");
  d.setTextColor(0xFFE0, 0x0000);
  d.print("[Del]");
  d.setTextColor(0x8410, 0x0000);
  d.print(" Erase");
  
  String s = "";
  int cursorPos = 0; // позиция курсора
  bool cancelled = false;
  
  while (true) {
    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
      if (M5Cardputer.Keyboard.isKeyPressed(KEY_ESC) || M5Cardputer.Keyboard.isKeyPressed('`')) { 
        cancelled = true;
        break; 
      }
      
      Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
      if (status.enter) break;
      
      // Навигация курсором
      if (keyLeftPressed()) {
        cursorPos = max(0, cursorPos - 1);
      } else if (keyRightPressed()) {
        cursorPos = min((int)s.length(), cursorPos + 1);
      }
      // Del/Backspace - удаление символа перед курсором
      else if (status.del && cursorPos > 0) {
        s.remove(cursorPos - 1, 1);
        cursorPos--;
      }
      // Добавление символов в позицию курсора
      else {
        for (char c : status.word) {
          // Фильтруем только стрелки и служебные символы
          if (c != 0x1B && c != '`' && c != ',' && c != '/' && c != ';' && c != '.' &&
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
      
      d.setCursor(8, 40);
      d.setTextColor(0x07E0, 0x0000);
      d.setTextSize(1);
      d.println("    Connected!");
      
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

// ====== UI main ======
void drawUI() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(0x0000); // черный фон
  d.setTextSize(1);
  d.setCursor(0, 0);
  
  // Строка 1: "MicroCast [156]" (циан) + "v0.1" (красный) справа
  d.setTextColor(0x07FF, 0x0000); // cyan
  d.print("MicroCast [");
  d.setTextColor(0xF800, 0x0000); // red
  d.print(deviceCount);
  d.setTextColor(0x07FF, 0x0000); // cyan
  d.print("]");
  
  // v0.1 справа
  d.setCursor(190, 0);
  d.setTextColor(0xF800, 0x0000); // red
  d.println("beta0.1");
  
  // Строка 2: подчеркивание
  d.setCursor(0, 8);
  d.setTextColor(0xFFFF, 0x0000); // white
  d.println("========================================");
  
  // Строка 3: "[N] WiFi🟢 [U] User: name"
  d.setCursor(0, 16);
  d.setTextColor(0xFFE0, 0x0000); // yellow
  d.print("[N]");
  d.setTextColor(0xFFFF, 0x0000); // white
  d.print(" WiFi");
  
  // Индикатор Wi-Fi (зеленый/красный кружок)
  drawWiFiIndicator(52, 20, wifiOK);
  
  d.setCursor(60, 16);
  d.setTextColor(0xFFE0, 0x0000); // yellow
  d.print(" [U]");
  d.setTextColor(0xFFFF, 0x0000); // white
  d.print(" User: ");
  d.setTextColor(0x07FF, 0x0000); // cyan для имени
  d.print(author);
  
  // Отображаем 2 поста начиная с scrollOffset
  int yPos = 28; // начальная позиция для постов
  int boxHeight = 42; // высота одного блока поста (увеличено для многострочного текста)
  int boxSpacing = 3; // отступ между блоками
  
  for (int i = 0; i < 2; i++) {
    int feedIdx = scrollOffset + i;
    if (feedIdx >= (int)feedCount) break;
    
    Post& p = FEED[feedIdx];
    int boxY = yPos + i * (boxHeight + boxSpacing);
    
    // Рамка (зеленая для выбранного, белая для остальных)
    uint16_t boxColor = (feedIdx == sel) ? 0x07E0 : 0x8410; // green / gray
    drawBox(2, boxY, 236, boxHeight, boxColor);
    
    // Автор (верхняя строка внутри блока)
    d.setCursor(6, boxY + 2);
    d.setTextColor(0xFFFF, 0x0000); // white
    d.print(p.author);
    
    // Текст поста (многострочный, до 3 строк)
    d.setTextColor(0xFFFF, 0x0000);
    String txt = p.text;
    int lineY = boxY + 10;
    int charsPerLine = 38; // characters per line
    for (int line = 0; line < 3 && txt.length() > 0; line++) {
      d.setCursor(6, lineY);
      String part = txt.substring(0, min((int)txt.length(), charsPerLine));
      d.print(part);
      txt = txt.substring(min((int)txt.length(), charsPerLine));
      lineY += 8;
    }
    
    // Heart + likes count (bottom left)
    // Black background under heart to cover border line
    d.fillRect(7, boxY + 32, 12, 10, 0x0000); // black rectangle
    drawHeart(11, boxY + 36, 0xF800); // red graphic heart
    d.setCursor(18, boxY + 34);
    d.setTextColor(0xF800, 0x0000);
    d.print(p.likes);
    
    // Post creation date (bottom right)
    // Format: "2024-01-23T15:30:45.123Z" -> "23.01.24 15:30"
    if (p.created_at.length() >= 19) {
      String date = p.created_at.substring(8, 10) + "." +  // day
                    p.created_at.substring(5, 7) + "." +   // month
                    p.created_at.substring(2, 4) + " " +   // year (last 2 digits)
                    p.created_at.substring(11, 16);        // time HH:MM
      d.setCursor(150, boxY + 34);
      d.setTextColor(0x8410, 0x0000); // gray color
      d.print(date);
    }
  }
  
  // Управление внизу
  d.setCursor(0, 118);
  d.setTextColor(0xFFE0, 0x0000); // yellow
  d.print("[W/A, Up/Dn] Scroll, [Enter] Like/Unlike");
  d.setCursor(0, 126);
  d.print("[Fn+Enter] Post, [R] Refresh Feed");
}

void setup() {
  auto cfg=M5.config(); M5Cardputer.begin(cfg,true);
  M5Cardputer.Display.setRotation(1); M5Cardputer.Display.setTextSize(1);
  
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
  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()){
    if (M5Cardputer.Keyboard.isKeyPressed('N') || M5Cardputer.Keyboard.isKeyPressed('n')) { 
      wifiForgetAndReconfig(); 
      if (wifiOK) { apiStats(); apiFeed(); }
      drawUI(); 
    }
    else if (keyUpPressed()) { 
      sel = max(0, sel-1); 
      // Обновляем scrollOffset, чтобы выбранный пост был виден
      if (sel < scrollOffset) scrollOffset = sel;
      drawUI(); 
    }
    else if (keyDownPressed()) { 
      sel = min((int)feedCount-1, sel+1); 
      // Обновляем scrollOffset, чтобы выбранный пост был виден
      if (sel >= scrollOffset + 2) scrollOffset = sel - 1;
      drawUI(); 
    }
    else if (M5Cardputer.Keyboard.isKeyPressed('R') || M5Cardputer.Keyboard.isKeyPressed('r')) { 
      if (wifiOK) { apiStats(); apiFeed(); } 
      drawUI(); 
    }
#ifdef KEY_FN
    else if (M5Cardputer.Keyboard.isKeyPressed(KEY_FN) && M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
#else
    else if (M5Cardputer.Keyboard.isKeyPressed(KEY_TAB) && M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
#endif
      String t = promptInput("New post (max 120)", 120);
      if (t.length()>0 && wifiOK) { apiPost(t); apiStats(); apiFeed(); }
      drawUI();
    }
    else if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
      if (feedCount>0 && wifiOK) { apiLike(FEED[sel].id); apiFeed(); }
      drawUI();
    }
    else if (M5Cardputer.Keyboard.isKeyPressed('U') || M5Cardputer.Keyboard.isKeyPressed('u')) {
      String newName = promptInput("New username (3-24)", 24);
      if (newName.length() >= 3 && wifiOK) {
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
      } else if (newName.length() > 0 && newName.length() < 3) {
        showMessage("Error", "Username too short!", "Minimum 3 characters", "", 2);
        while (!M5Cardputer.Keyboard.isChange()) { M5Cardputer.update(); delay(10); }
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