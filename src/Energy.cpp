#include <PZEM004Tv30.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

// ==========================================
// 1. SETTINGS & CREDENTIALS
// ==========================================
const char* ssid = "AUSTUDENT";         
const char* password = "4592cdef0912"; 
#define COST_PER_KWH 100000 
#define RELAY_PIN 4
#define EEPROM_SIZE 128
#define BALANCE_ADDR 0

// ==========================================
// 2. GLOBAL VARIABLES
// ==========================================
float balance = 0.0;
bool relayState = true;
unsigned long lastMillis = 0;
float consumptionRateDay = 0.0;
float startBalanceForRate = 0;
unsigned long startTimeForRate = 0;

WebServer server(80);
PZEM004Tv30 pzem(Serial2, 16, 17); // RX=16, TX=17
// CS=5, DC=21, MOSI=23, SCLK=18, RST=22
Adafruit_ST7735 tft = Adafruit_ST7735(5, 21, 23, 18, 22);

// ==========================================
// 3. HTML DASHBOARD PAGE
// ==========================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><title>EnergyPro</title>
<meta charset='utf-8'>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<link href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0/css/all.min.css' rel='stylesheet'>
<style>
  :root { --primary: #1a237e; --bg: #f4f7f9; }
  body { font-family: 'Segoe UI', sans-serif; margin:0; display: flex; background: var(--bg); }
  .sidebar { width: 220px; background: var(--primary); height: 100vh; color: white; padding: 20px 0; position: fixed; }
  .sidebar h2 { padding: 0 20px; font-size: 1.2rem; margin-bottom: 30px; }
  .nav-item { padding: 15px 25px; cursor: pointer; display: flex; align-items: center; text-decoration: none; color: white; transition: 0.3s; }
  .nav-item:hover, .active { background: #ffffff33; border-left: 4px solid #4fc3f7; }
  .nav-item i { margin-right: 15px; width: 20px; }
  .main { margin-left: 220px; padding: 30px; width: 100%; }
  .header { display: flex; justify-content: space-between; margin-bottom: 30px; }
  .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 20px; }
  .card { background: white; padding: 20px; border-radius: 12px; box-shadow: 0 4px 6px rgba(0,0,0,0.05); position: relative; }
  .card h3 { font-size: 0.8rem; color: #666; margin: 0; text-transform: uppercase; }
  .card .val { font-size: 1.8rem; font-weight: bold; margin: 10px 0; color: #1a237e; }
  .icon-box { position: absolute; right: 20px; top: 20px; width: 35px; height: 35px; border-radius: 50%; display: flex; align-items: center; justify-content: center; color: white; }
  .status-row { display: flex; justify-content: space-between; padding: 10px 0; border-bottom: 1px solid #eee; }
  .btn { padding: 12px 25px; border: none; border-radius: 8px; color: white; cursor: pointer; font-weight: 600; width: 48%; }
  .btn-on { background: #66bb6a; } .btn-off { background: #ef5350; } .btn-recharge { background: #42a5f5; width: 100%; margin-top: 10px; }
  input { width: 94%; padding: 10px; border: 1px solid #ddd; border-radius: 5px; margin-top: 10px; }
</style></head><body>
  <div class='sidebar'>
    <h2>⚡ EnergyPro</h2>
    <a href='/' class='nav-item active'><i class='fas fa-th-large'></i> Dashboard</a>
    <a href='/account' class='nav-item'><i class='fas fa-user'></i> Account</a>
    <a href='#' class='nav-item'><i class='fas fa-gamepad'></i> Control</a>
    <a href='#' class='nav-item'><i class='fas fa-cog'></i> Settings</a>
    <a href='#' class='nav-item'><i class='fas fa-question-circle'></i> Help</a>
  </div>
  <div class='main'>
    <div class='header'>
      <div><h1 style='margin:0'>Energy Dashboard</h1><small>Real-time monitoring</small></div>
      <div id='time'>Last updated: --:--</div>
    </div>
    <div class='grid'>
      <div class='card'><div class='icon-box' style='background:#0288d1'><i class='fas fa-indian-rupee-sign'></i></div><h3>Balance</h3><div class='val'>&#8377;<span id='bal'>0.00</span></div><small>Available credit</small></div>
      <div class='card'><div class='icon-box' style='background:#1976d2'><i class='fas fa-bolt'></i></div><h3>Voltage</h3><div class='val'><span id='v'>0</span>V</div><small>Line voltage</small></div>
      <div class='card'><div class='icon-box' style='background:#0277bd'><i class='fas fa-wave-square'></i></div><h3>Current</h3><div class='val'><span id='i'>0</span>A</div><small>Line current</small></div>
      <div class='card'><div class='icon-box' style='background:#01579b'><i class='fas fa-plug'></i></div><h3>Power</h3><div class='val'><span id='p'>0</span>W</div><small>Active power</small></div>
    </div>
    <div style='display:flex; gap:20px; margin-top:20px;'>
      <div class='card' style='flex:1'><h3>System Status</h3>
        <div class='status-row'><span>Power</span><b id='pwr-stat' style='color:green'>ON</b></div>
        <div class='status-row'><span>Fault Status</span><b style='color:green'>OK</b></div>
        <div class='status-row'><span>Theft Detection</span><b style='color:green'>OK</b></div>
        <div class='status-row'><span>Total Energy</span><span><span id='e'>0</span> kWh</span></div>
      </div>
      <div class='card' style='flex:1'><h3>Quick Actions</h3>
        <p><small>Control power supply to your premises</small></p>
        <div style='display:flex; justify-content:space-between'>
          <button class='btn btn-on' onclick='setRelay(1)'>Power ON</button>
          <button class='btn btn-off' onclick='setRelay(0)'>Power OFF</button>
        </div>
        <input type='number' id='amt' placeholder='100'>
        <button class='btn btn-recharge' onclick='recharge()'>Recharge Now</button>
      </div>
    </div>
  </div>
<script>
function update(){
  fetch('/data').then(r=>r.json()).then(d=>{
    document.getElementById('bal').innerText=d.balance.toFixed(2);
    document.getElementById('v').innerText=d.voltage.toFixed(1);
    document.getElementById('i').innerText=d.current.toFixed(3);
    document.getElementById('p').innerText=d.power.toFixed(1);
    document.getElementById('e').innerText=d.energy.toFixed(3);
    document.getElementById('pwr-stat').innerText = d.relay ? 'ON' : 'OFF';
    document.getElementById('pwr-stat').style.color = d.relay ? 'green' : 'red';
    document.getElementById('time').innerText = 'Last updated: ' + new Date().toLocaleTimeString();
  });
}
function setRelay(s){ fetch('/setRelay?state='+s); }
function recharge(){ 
  let a = document.getElementById('amt').value;
  if(a) fetch('/recharge?amount='+a).then(()=>alert('Recharge Successful!'));
}
setInterval(update, 2000);
update();
</script></body></html>)rawliteral";

// ==========================================
// 4. HTML ACCOUNT PAGE
// ==========================================
const char account_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><title>Account - EnergyPro</title>
<meta charset='utf-8'>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<link href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0/css/all.min.css' rel='stylesheet'>
<style>
  :root { --primary: #1a237e; --bg: #f4f7f9; }
  body { font-family: 'Segoe UI', sans-serif; margin:0; display: flex; background: var(--bg); }
  .sidebar { width: 220px; background: var(--primary); height: 100vh; color: white; padding: 20px 0; position: fixed; }
  .sidebar h2 { padding: 0 20px; font-size: 1.2rem; margin-bottom: 30px; }
  .nav-item { padding: 15px 25px; cursor: pointer; display: flex; align-items: center; text-decoration: none; color: white; transition: 0.3s; }
  .nav-item:hover, .active { background: #ffffff33; border-left: 4px solid #4fc3f7; }
  .nav-item i { margin-right: 15px; width: 20px; }
  .main { margin-left: 220px; padding: 30px; width: 100%; }
  .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 20px; }
  .card { background: white; padding: 25px; border-radius: 12px; box-shadow: 0 4px 6px rgba(0,0,0,0.05); }
  .card h3 { font-size: 0.85rem; color: #666; margin: 0 0 15px 0; text-transform: uppercase; letter-spacing: 1px; }
  .val { font-size: 2.2rem; font-weight: bold; color: #1a237e; }
  .unit { font-size: 1.2rem; color: #666; margin-left: 5px; }
  input { width: 100%; padding: 12px; border: 1px solid #ddd; border-radius: 8px; margin: 10px 0; font-size: 1rem; box-sizing: border-box; }
  .btn-blue { background: #42a5f5; color: white; border: none; padding: 12px; width: 100%; border-radius: 8px; cursor: pointer; font-weight: bold; }
</style></head><body>
  <div class='sidebar'>
    <h2>⚡ EnergyPro</h2>
    <a href='/' class='nav-item'><i class='fas fa-th-large'></i> Dashboard</a>
    <a href='/account' class='nav-item active'><i class='fas fa-user'></i> Account</a>
    <a href='#' class='nav-item'><i class='fas fa-gamepad'></i> Control</a>
    <a href='#' class='nav-item'><i class='fas fa-cog'></i> Settings</a>
    <a href='#' class='nav-item'><i class='fas fa-question-circle'></i> Help</a>
  </div>
  <div class='main'>
    <h1>Account Management</h1>
    <p style='color:#666'>Manage your prepaid account balance and transactions</p>
    <div class='grid'>
      <div class='card'><h3>Account Balance</h3><div class='val'>&#8377;<span id='bal'>0.00</span></div><small>Available credit</small></div>
      <div class='card'><h3>Consumption Rate</h3><div class='val'>&#8377;<span id='rate'>--</span><span class='unit'>/day</span></div><small>Average daily consumption</small></div>
    </div>
    <div class='card' style='margin-top:20px; max-width: 600px;'>
      <h3>Recharge Account</h3>
      <div style='margin-top:20px; border-top:1px solid #eee; padding-top:20px;'>
        <label>Recharge Amount</label>
        <input type='number' id='amt' placeholder='100'>
        <button class='btn-blue' onclick='recharge()'>Process Recharge</button>
      </div>
    </div>
  </div>
<script>
  function update(){
    fetch('/data').then(r=>r.json()).then(d=>{
      document.getElementById('bal').innerText = d.balance.toFixed(2);
      document.getElementById('rate').innerText = d.rate > 0 ? d.rate.toFixed(2) : '--';
    });
  }
  function recharge(){
    let a = document.getElementById('amt').value;
    if(a > 0) fetch('/recharge?amount='+a).then(()=> { alert('Recharged!'); location.reload(); });
  }
  setInterval(update, 3000);
  update();
</script></body></html>)rawliteral";

// ==========================================
// 5. SERVER HANDLERS
// ==========================================
void handleData() {
  StaticJsonDocument<256> doc;
  float v = pzem.voltage();
  float i = pzem.current();
  float p = pzem.power();
  float e = pzem.energy();
  
  doc["voltage"] = isnan(v) ? 0 : v;
  doc["current"] = isnan(i) ? 0 : i;
  doc["power"]   = isnan(p) ? 0 : p;
  doc["energy"]  = isnan(e) ? 0 : e;
  doc["balance"] = balance;
  doc["relay"]   = relayState;
  doc["rate"]    = consumptionRateDay;
  
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleRecharge() {
  if (server.hasArg("amount")) {
    balance += server.arg("amount").toFloat();
    EEPROM.put(BALANCE_ADDR, balance);
    EEPROM.commit();
    server.send(200, "text/plain", "OK");
  }
}

void handleSetRelay() {
  if (server.hasArg("state")) {
    relayState = server.arg("state").toInt();
    digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
    server.send(200, "text/plain", "OK");
  }
}

// ==========================================
// 6. TFT DISPLAY LOGIC (MATCHING IMAGE)
// ==========================================
void updateTFT() {
  float v = pzem.voltage();
  float i = pzem.current();
  float p = pzem.power();
  float e = pzem.energy();

  tft.setTextWrap(false);
  
  // Header
  tft.fillRect(0, 0, 160, 20, ST7735_BLUE);
  tft.setCursor(5, 5);
  tft.setTextColor(ST7735_WHITE);
  tft.setTextSize(1);
  tft.print("ESP32 PREPAID METER");

  // Grid Drawing
  tft.drawRect(0, 22, 160, 105, ST7735_WHITE); // Main outer box
  tft.drawFastHLine(0, 55, 160, ST7735_WHITE); // Middle horizontal
  tft.drawFastHLine(0, 88, 160, ST7735_WHITE); // Bottom horizontal
  tft.drawFastVLine(80, 22, 66, ST7735_WHITE); // Vertical divider

  // Row 1: Voltage & Current
  tft.setTextColor(ST7735_CYAN);
  tft.setCursor(5, 27); tft.print("VOLTAGE");
  tft.setCursor(85, 27); tft.print("CURRENT");
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(5, 40); tft.printf("%.1fV", isnan(v) ? 0 : v);
  tft.setCursor(85, 40); tft.printf("%.3fA", isnan(i) ? 0 : i);

  // Row 2: Power & Energy
  tft.setTextColor(ST7735_CYAN);
  tft.setCursor(5, 60); tft.print("POWER");
  tft.setCursor(85, 60); tft.print("ENERGY");
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(5, 73); tft.printf("%.1fW", isnan(p) ? 0 : p);
  tft.setCursor(85, 73); tft.printf("%.2fkWh", isnan(e) ? 0 : e);

  // Row 3: Balance & Status
  tft.setTextColor(ST7735_CYAN);
  tft.setCursor(5, 93); tft.print("BALANCE");
  tft.setCursor(85, 93); tft.print("STATUS");
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(5, 106); tft.printf("%.2f Rs", balance);
  
  if(relayState) {
    tft.setTextColor(ST7735_GREEN);
    tft.setCursor(85, 106); tft.print("ACTIVE");
  } else {
    tft.setTextColor(ST7735_RED);
    tft.setCursor(85, 106); tft.print("OFF");
  }

  // Footer: IP Address
  tft.fillRect(0, 129, 160, 10, ST7735_BLACK); 
  tft.setTextColor(ST7735_GREEN);
  tft.setCursor(5, 129);
  tft.print("IP: "); tft.print(WiFi.localIP());
}

// ==========================================
// 7. SETUP & LOOP
// ==========================================
void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(BALANCE_ADDR, balance);
  if (isnan(balance) || balance < 0) balance = 100.0;

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1); 
  tft.fillScreen(ST7735_BLACK);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }

  server.on("/", []() { server.send(200, "text/html", index_html); });
  server.on("/account", []() { server.send(200, "text/html", account_html); });
  server.on("/data", handleData);
  server.on("/recharge", handleRecharge);
  server.on("/setRelay", handleSetRelay);
  server.begin();

  startTimeForRate = millis();
  startBalanceForRate = balance;
  lastMillis = millis();
}

void loop() {
  server.handleClient();
  
  if (millis() - lastMillis > 2000) {
    float powerW = pzem.power();
    if (!isnan(powerW) && relayState) {
      float hours = (millis() - lastMillis) / 3600000.0; 
      float cost = (powerW / 1000.0) * hours * COST_PER_KWH;
      balance -= cost;

      float spent = startBalanceForRate - balance;
      float timeDays = (millis() - startTimeForRate) / 86400000.0;
      if (timeDays > 0.0001) consumptionRateDay = spent / timeDays;

      if (balance <= 0) {
        balance = 0;
        relayState = false;
        digitalWrite(RELAY_PIN, LOW);
      }
      
      static unsigned long lastSave = 0;
      if (millis() - lastSave > 60000) {
        EEPROM.put(BALANCE_ADDR, balance);
        EEPROM.commit();
        lastSave = millis();
      }
    }
    
    updateTFT();
    lastMillis = millis();
  }
}
