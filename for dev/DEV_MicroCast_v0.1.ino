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
  auto& ks = M5Cardputer.Keyboard.keysState();
  
  // Стрелка вверх: ; (символ на клавише с оранжевой стрелкой)
  if (M5Cardputer.Keyboard.isKeyPressed(';')) return true;
  
  // Альтернативно — W/w для левшей
  return M5Cardputer.Keyboard.isKeyPressed('W') || M5Cardputer.Keyboard.isKeyPressed('w');
}

bool keyDownPressed(){
  auto& ks = M5Cardputer.Keyboard.keysState();
  
  // Стрелка вниз: . (символ на клавише с оранжевой стрелкой)
  if (M5Cardputer.Keyboard.isKeyPressed('.')) return true;
  
  // Альтернативно — A/a для левшей  
  return M5Cardputer.Keyboard.isKeyPressed('A') || M5Cardputer.Keyboard.isKeyPressed('a');
}

String promptInput(const String& title, size_t maxLen, bool mask=false) {
  auto& d=M5Cardputer.Display; header(title.c_str());
  printLine("(ASCII) Enter=OK  Esc=Cancel  Del=Erase", 0xFFE0);
  String s="";
  bool cancelled = false;
  while (true) {
    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
      // ВАЖНО: ESC проверяем первым, до KeysState
      // На M5Cardputer ESC - это клавиша с символом `
      if (M5Cardputer.Keyboard.isKeyPressed(KEY_ESC) || M5Cardputer.Keyboard.isKeyPressed('`')) { 
        cancelled = true;
        break; 
      }
      
      Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
      
      // Enter - подтвердить
      if (status.enter) break;
      
      // Del/Backspace - удалить последний символ
      if (status.del && s.length() > 0) {
        s.remove(s.length() - 1);
      }
      
      // Добавление символов из word (фильтруем ESC если он попал в word)
      for (char c : status.word) {
        // Игнорируем служебные символы и backtick (ESC на M5Cardputer)
        if (c != 0x1B && c != '`' && c >= 32 && c <= 126 && s.length() < maxLen) {
          s += c;
        }
      }
      
      // отрисовка
      d.fillRect(0, 48, d.width(), d.height()-48, 0x0000);
      d.setCursor(0,48);
      printLine(mask ? maskStars(s.length()) : s, 0xFFFF);
    }
    delay(8);
  }
  if (cancelled) return "";
  return asciiOnly(s, maxLen);
}

// ====== Wi-Fi onboarding ======
int scanAndSelectNetwork(String& outSsid, bool& openNet) {
  header("WiFi: scanning...");
  printLine("Scanning networks...", 0xFFFF);
  int n = WiFi.scanNetworks();
  if (n<=0){ printLine("No networks found.", 0xF800); delay(900); return -1; }
  int idx=0;
  while (true) {
    header("WiFi: select SSID");
    int start = max(0, idx-6); int end=min(n, start+12);
    for(int i=start;i<end;++i){
      String line = String(i==idx?"> ":"  ") + WiFi.SSID(i);
      if (WiFi.encryptionType(i)==WIFI_AUTH_OPEN) line += " (open)";
      line += "  RSSI:" + String(WiFi.RSSI(i));
      printLine(line, i==idx?0x07FF:0xFFFF);
    }
    printLine("",0xFFFF);
    printLine("Up/Down (or W/A)  Enter select  R rescan  Esc cancel",0xFFE0);

    // input
    for(;;){
      M5Cardputer.update();
      if (!(M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed())) { delay(10); continue; }
      if (keyUpPressed())   { idx = (idx>0)?idx-1:0; break; }
      if (keyDownPressed()) { idx = (idx<n-1)?idx+1:n-1; break; }
      if (M5Cardputer.Keyboard.isKeyPressed('R') || M5Cardputer.Keyboard.isKeyPressed('r')) { return scanAndSelectNetwork(outSsid, openNet); }
      if (M5Cardputer.Keyboard.isKeyPressed(KEY_ESC)) { return -1; }
      if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)){
        outSsid = WiFi.SSID(idx);
        openNet = (WiFi.encryptionType(idx)==WIFI_AUTH_OPEN);
        return idx;
      }
    }
  }
}
bool tryConnectWiFi(const String& ssid, const String& pass, int tries=30){
  header("WiFi: connecting");
  printLine("SSID: "+ssid, 0xFFFF);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.length()?pass.c_str():nullptr);
  for(int i=0;i<tries;++i){
    if (WiFi.status()==WL_CONNECTED){ printLine("Connected.",0x07E0); return true; }
    delay(300); M5Cardputer.Display.print(".");
  }
  printLine("\nFailed.",0xF800);
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
    header("First run - Registration");
    printLine("Enter username (min 3 chars):", 0xFFFF);
    printLine("Press ESC to skip", 0x8410);
    String newAuthor = "";
    while (true) {
      newAuthor = asciiOnly(promptInput("Username (3-24 chars)", 24), 24);
      if (newAuthor.length() == 0) {
        // ESC pressed - use temporary name
        author = "user_" + deviceId.substring(0, 6);
        prefs.putString("author", author);
        header("Skipped Registration");
        printLine("Using temp name: " + author, 0xFFE0);
        printLine("Press [U] later to register", 0xFFE0);
        printLine("Press any key...", 0xFFE0);
        while (!M5Cardputer.Keyboard.isChange()) { M5Cardputer.update(); delay(10); }
        break;
      }
      if (newAuthor.length() < 3) {
        header("Error");
        printLine("Username too short (min 3)", 0xF800);
        printLine("Press ESC to skip", 0x8410);
        printLine("Press any key to retry...", 0xFFE0);
        while (!M5Cardputer.Keyboard.isChange()) { M5Cardputer.update(); delay(10); }
        continue;
      }
      // Try to register
      if (!wifiOK) {
        header("No WiFi");
        printLine("WiFi not connected!", 0xF800);
        printLine("Using temp name: user_" + deviceId.substring(0,6), 0xFFE0);
        printLine("Connect WiFi and press [U]", 0xFFE0);
        printLine("to register later", 0xFFE0);
        author = "user_" + deviceId.substring(0, 6);
        prefs.putString("author", author);
        printLine("Press any key...", 0xFFE0);
        while (!M5Cardputer.Keyboard.isChange()) { M5Cardputer.update(); delay(10); }
        break;
      }
      header("Registering...");
      printLine("Connecting to server...", 0xFFFF);
      if (apiRegister(newAuthor)) {
        header("Success!");
        printLine("Username: " + author, 0x07E0);
        printLine("Press any key to continue...", 0xFFE0);
        while (!M5Cardputer.Keyboard.isChange()) { M5Cardputer.update(); delay(10); }
        break;
      } else {
        header("Registration Failed");
        printLine("Username may be taken or", 0xF800);
        printLine("server error", 0xF800);
        printLine("Press ESC to skip", 0x8410);
        printLine("Press any key to retry...", 0xFFE0);
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
  d.setCursor(210, 0);
  d.setTextColor(0xF800, 0x0000); // red
  d.println("v0.1");
  
  // Строка 2: подчеркивание
  d.setCursor(0, 8);
  d.setTextColor(0xFFFF, 0x0000); // white
  d.println("========================================");
  
  // Строка 3: "[N] WiFi🟢 [U] User: name"
  d.setCursor(0, 16);
  d.setTextColor(0xFFFF, 0x0000); // white
  d.print("[N] WiFi");
  
  // Индикатор Wi-Fi (зеленый/красный кружок)
  drawWiFiIndicator(52, 20, wifiOK);
  
  d.setCursor(60, 16);
  d.print(" [U] User: ");
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

// ====== Arduino ======
void setup() {
  auto cfg=M5.config(); M5Cardputer.begin(cfg,true);
  M5Cardputer.Display.setRotation(1); M5Cardputer.Display.setTextSize(1);
  ensureDevice();
  wifiLoadOrOnboard();
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
        header("Updating username...");
        printLine("Please wait...", 0xFFFF);
        String errorMsg = "";
        bool success = false;
        
        // Try to change name first
        if (apiChangeName(newName, errorMsg)) {
          success = true;
        } else if (errorMsg == "Device not found") {
          // Device not registered yet - register it
          header("Registering...");
          printLine("First time registration...", 0xFFFF);
          if (apiRegister(newName)) {
            success = true;
            errorMsg = "";
          } else {
            errorMsg = "Registration failed";
          }
        }
        
        if (success) {
          header("Success!");
          printLine("Username: " + author, 0x07E0);
          printLine("Press any key...", 0xFFE0);
          while (!M5Cardputer.Keyboard.isChange()) { M5Cardputer.update(); delay(10); }
        } else {
          header("Error");
          printLine(errorMsg, 0xF800);
          printLine("Press any key...", 0xFFE0);
          while (!M5Cardputer.Keyboard.isChange()) { M5Cardputer.update(); delay(10); }
        }
      } else if (newName.length() > 0 && newName.length() < 3) {
        header("Error");
        printLine("Username too short (min 3)", 0xF800);
        printLine("Press any key...", 0xFFE0);
        while (!M5Cardputer.Keyboard.isChange()) { M5Cardputer.update(); delay(10); }
      }
      drawUI();
    }
#ifdef KEY_FN
    else if (M5Cardputer.Keyboard.isKeyPressed(KEY_FN) && (M5Cardputer.Keyboard.isKeyPressed('C') || M5Cardputer.Keyboard.isKeyPressed('c'))) {
#else
    else if (M5Cardputer.Keyboard.isKeyPressed(KEY_TAB) && (M5Cardputer.Keyboard.isKeyPressed('C') || M5Cardputer.Keyboard.isKeyPressed('c'))) {
#endif
      header("Clear All Settings");
      printLine("This will erase:", 0xFFFF);
      printLine("- Device ID", 0xFFE0);
      printLine("- Username", 0xFFE0);
      printLine("- WiFi settings", 0xFFE0);
      printLine("", 0xFFFF);
      printLine("Press ENTER to confirm", 0xF800);
      printLine("Press ESC to cancel", 0x07E0);
      
      while (true) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
          if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
            header("Clearing...");
            printLine("Erasing all data...", 0xF800);
            prefs.begin("microcast", false);
            prefs.clear();
            prefs.end();
            printLine("Done!", 0x07E0);
            printLine("", 0xFFFF);
            printLine("Please restart device", 0xFFE0);
            printLine("(unplug and plug back)", 0xFFE0);
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