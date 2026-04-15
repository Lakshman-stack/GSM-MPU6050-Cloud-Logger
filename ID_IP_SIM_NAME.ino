#include <Wire.h>
#include <MPU6050.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>

#define FLAG_ADDR 160

SoftwareSerial gsm(7, 8);
MPU6050 mpu;

#define SAMPLE_COUNT 16

bool smsReceived = false;
bool smsProcessing = false;

char DEVICE_ID[20];
char IP_PORT[25];
char SIM_NAME[10] = "UNKNOWN";

long xSum = 0;
long zSum = 0;

int xAvg = 0;
int zAvg = 0;

bool gsmBusy = false;

unsigned long lastSendTime = 0;
const unsigned long sendInterval = 20000;

// ------------------------------
void setupGPRS()
{
  Serial.println(F("Setting up GPRS..."));
  while (gsm.available()) gsm.read(); 
  gsm.println("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"");
  waitWithSMS(1000);

  if (strcmp(SIM_NAME, "AIRTEL") == 0)
  gsm.println("AT+SAPBR=3,1,\"APN\",\"airtelgprs.com\"");
  else if (strcmp(SIM_NAME, "BSNL") == 0)
    gsm.println("AT+SAPBR=3,1,\"APN\",\"bsnlnet\"");
  else if (strcmp(SIM_NAME, "VI") == 0)
    gsm.println("AT+SAPBR=3,1,\"APN\",\"www\"");
  else
    gsm.println("AT+SAPBR=3,1,\"APN\",\"airtelgprs.com\"");
  waitWithSMS(2000);
  while (gsm.available()) gsm.read();
  gsm.println("AT+SAPBR=1,1");
  waitWithSMS(3000);
  while (gsm.available()) gsm.read();
  gsm.println("AT+SAPBR=2,1");
  waitWithSMS(2000);

  Serial.println(F("GPRS Ready"));
}
//SMS → saveConfig() → EEPROM --> Device restart → loadConfig() → RAM

// --IF DEVICE RESTARTS 
void loadConfig()
{
  memset(DEVICE_ID, 0, sizeof(DEVICE_ID));
  memset(IP_PORT, 0, sizeof(IP_PORT));
  // EEPROM.write(FLAG_ADDR, 1); BOX A OR EEPROM.write(FLAG_ADDR, 0); BOX B
  byte flag = EEPROM.read(FLAG_ADDR);

  if (flag == 1)
  {
    Serial.println(F("Using BOX A"));

    for (int i = 0; i < 20; i++)
    {
      char c = EEPROM.read(i);
      if (isPrintable(c)) DEVICE_ID[i] = c;
    }

    for (int i = 0; i < 20; i++)
    {
      char c = EEPROM.read(20 + i);
      if (isPrintable(c)) IP_PORT[i] = c;
    }
  }
  else
  {
    Serial.println(F("Recovery mode BOX B"));

    for (int i = 0; i < 20; i++)
    {
      char c = EEPROM.read(80 + i);
      if (isPrintable(c)) DEVICE_ID[i] = c;
    }

    for (int i = 0; i < 20; i++)
    {
      char c = EEPROM.read(100 + i);
      if (isPrintable(c)) IP_PORT[i] = c;
    }
  }

  Serial.print(F("ID: "));
  Serial.println(DEVICE_ID);
  Serial.print(F("IP: "));
  Serial.println(IP_PORT);
}

// BEFORE POWER OFF
void saveConfig()
{
  Serial.println(F("Saving config..."));
 // UPDATE IN PROGRESS 
  EEPROM.write(FLAG_ADDR, 0);
  // COPY TO BOX B ID AND IP PORT
  for (int i = 0; i < 20; i++)
    EEPROM.write(80 + i, (i < strlen(DEVICE_ID)) ? DEVICE_ID[i] : 0);

  for (int i = 0; i < 20; i++)
    EEPROM.write(100 + i, (i < strlen(IP_PORT)) ? IP_PORT[i] : 0);
  delay(50);

  // copy BOX B TO BOX A
  for (int i = 0; i < 40; i++)
    EEPROM.write(i, EEPROM.read(80 + i));

  delay(50);

  EEPROM.write(FLAG_ADDR, 1); // FLAG=1 TRANSFER TO BOX A

  Serial.println(F("Config saved"));
}

// ------------------------------
void initGSM()
{
  Serial.println(F("Initializing GSM"));

  gsm.println("AT"); delay(1000);
  gsm.println("ATE0"); delay(1000);
  gsm.println("AT+CMGF=1"); delay(1000);
  gsm.println("AT+CSCS=\"GSM\""); delay(1000);
  gsm.println("AT+CNMI=2,2,0,0,0"); delay(1000);

  gsm.println("AT+CPMS=\"SM\",\"SM\",\"SM\""); delay(1000);
  gsm.println("AT+CSQ"); delay(1000);
  gsm.println("AT+CREG?"); delay(2000);

  Serial.println(F("GSM Ready"));
}

// ------------------------------
bool isGSMConnected()
{
  for (int i = 0; i < 3; i++)
  {
    gsm.println("AT+CREG?");
    delay(500);

    char res[40] = "";
byte idx = 0;

while (gsm.available())
{
  char c = gsm.read();
  if (idx < sizeof(res) - 1)
    res[idx++] = c;
}
res[idx] = '\0';

   if (strstr(res, ",1") || strstr(res, ",5"))
{
  return true;
}

    delay(1000);
  }
  return false;
}

void detectSIM()
{
  // 🔥 Clear buffer
  while (gsm.available()) gsm.read();

  // 🔷 First try using operator name
  gsm.println("AT+COPS?");
  delay(3000);

  char res[80] = "";
  byte idx = 0;

  while (gsm.available())
  {
    char c = gsm.read();
    if (idx < sizeof(res) - 1)
      res[idx++] = c;
  }
  res[idx] = '\0';

  Serial.print("Operator RAW: ");
  Serial.println(res);

  if (strstr(res, "airtel") || strstr(res, "Airtel"))
  {
    strcpy(SIM_NAME, "AIRTEL");
  }
  else if (strstr(res, "BSNL"))
  {
    strcpy(SIM_NAME, "BSNL");
  }
  else if (strstr(res, "Vi") || strstr(res, "Vodafone"))
  {
    strcpy(SIM_NAME, "VI");
  }
  else
  {
    // 🔴 FALLBACK using CIMI (IMPORTANT)
    Serial.println(F("COPS failed → using CIMI"));

    while (gsm.available()) gsm.read();

    gsm.println("AT+CIMI");
    delay(2000);

    char imsi[20] = "";
    idx = 0;

    while (gsm.available())
    {
      char c = gsm.read();
      if (isDigit(c) && idx < sizeof(imsi) - 1)
        imsi[idx++] = c;
    }
    imsi[idx] = '\0';

    Serial.print("IMSI: ");
    Serial.println(imsi);

    // 🔥 Detect using IMSI prefix
    if (strncmp(imsi, "40445", 5) == 0 || strncmp(imsi, "40410", 5) == 0)
      strcpy(SIM_NAME, "AIRTEL");

    else if (strncmp(imsi, "40438", 5) == 0)
      strcpy(SIM_NAME, "BSNL");

    else if (strncmp(imsi, "40486", 5) == 0 || strncmp(imsi, "40484", 5) == 0)
      strcpy(SIM_NAME, "VI");

    else
      strcpy(SIM_NAME, "UNKNOWN");
  }

  Serial.print("Detected SIM: ");
  Serial.println(SIM_NAME);
}


// ------------------------------
void sendToCloud()
{
  gsmBusy = true;

  if (!isGSMConnected())
  {
    Serial.println(F("GSM not connected"));
    initGSM();
    delay(3000);
    return;
  }

 setupGPRS();

// 🔴 Check SMS BEFORE doing anything
checkSMS();
if (smsReceived || smsProcessing)
{
  Serial.println(F("⛔ SMS detected → stopping cloud"));
  return;
}

// 🔴 Set SMS mode
gsm.println("AT+CNMI=2,2,0,0,0");
delay(500);

// 🔥 FIX 2: Clear GSM buffer
while (gsm.available()) gsm.read();

// 🔥 FIX 3: Check again BEFORE HTTP
checkSMS();
if (smsReceived || smsProcessing)
{
  Serial.println(F("⛔ SMS priority → skipping cloud"));
  
  gsm.println("AT+HTTPTERM");  // stop any previous HTTP
  delay(500);

  gsmBusy = false;
  return;
}

// 🔴 Now build URL
char url[100];
sprintf(url, "http://%s/gate/api?field1=%s&field2=%d&field3=%d&field4=BATCH6",
        IP_PORT, DEVICE_ID, xAvg, zAvg);

url[strcspn(url, "\r\n")] = 0;

  Serial.println(url);
  checkSMS();
  if (smsReceived) return;

  gsm.println("AT+HTTPTERM"); delay(500);
  gsm.println("AT+HTTPINIT"); delay(500);
  while (gsm.available()) gsm.read();
  gsm.println("AT+HTTPPARA=\"CID\",1"); delay(500);

  gsm.print("AT+HTTPPARA=\"URL\",\"");
  gsm.print(url);
  gsm.println("\"");

  delay(1000);
  while (gsm.available()) gsm.read();
  gsm.println("AT+HTTPACTION=0");
  delay(2000);

unsigned long t = millis();
while (millis() - t < 5000)
{
  checkSMS();

  if (smsReceived || smsProcessing)
  {
    Serial.println(F("⛔ SMS detected → STOP HTTP"));

    gsm.println("AT+HTTPTERM");  // 🔥 STOP HTTP
    delay(500);

    gsmBusy = false;
    return;
  }
}

  Serial.println(F("Data Sent"));

  gsmBusy = false;
}

// ------------------------------
void checkSMS()
{
  static char line[50];
  static byte index = 0;
  static char sender[15];
  static bool waitForSMS = false;

  while (gsm.available())
  {
    char c = gsm.read();
    if (c == '\r') continue;

    if (c == '\n')
    {
      line[index] = '\0';
      index = 0;

      Serial.print("RAW: ");
      for (int i = 0; line[i] != '\0'; i++)
        {
          if (isPrintable(line[i]))
            Serial.print(line[i]);
        }
        Serial.println();

      // 🔴 Detect SMS header
      if (strstr(line, "+CMT:"))
      {
        sscanf(line, "+CMT: \"%[^\"]\"", sender);

        Serial.print("FROM: ");
        Serial.println(sender);

        waitForSMS = true;
        continue;
      }

      // 🔴 Wait until REAL SMS content comes
static char smsBuffer[30] = "";

if (waitForSMS)
{
  if (strlen(line) == 0) return;

  if (strcmp(line, "OK") == 0 || strcmp(line, "ERROR") == 0 || line[0] == '+')
    return;

  strncat(smsBuffer, line, sizeof(smsBuffer) - strlen(smsBuffer) - 1);

  if (strlen(smsBuffer) >= 5)
  {
    smsReceived = true;
    smsProcessing = true;
    gsmBusy = false;

    Serial.print("FULL SMS: ");
    Serial.println(smsBuffer);

    // ---------- ID ----------
    if (strncmp(smsBuffer, "ID:", 3) == 0)
    {
      strncpy(DEVICE_ID, smsBuffer + 3, sizeof(DEVICE_ID));
      DEVICE_ID[sizeof(DEVICE_ID) - 1] = '\0';

      Serial.println(F("UPDATED ID:"));
      Serial.println(DEVICE_ID);

      saveConfig();
    }

    // ---------- IP ----------
    else if (strncmp(smsBuffer, "IP:", 3) == 0)
    {
      strncpy(IP_PORT, smsBuffer + 3, sizeof(IP_PORT));
      IP_PORT[sizeof(IP_PORT) - 1] = '\0';

      Serial.println(F("UPDATED IP:"));
      Serial.println(IP_PORT);

      saveConfig();
    }

    // ---------- STATUS ----------
    else if (strncmp(smsBuffer, "STATUS", 6) == 0)
    {
      char msg[60];
      sprintf(msg, "ACK\nID:%s\nIP:%s\nSIM:%s",
              DEVICE_ID, IP_PORT, SIM_NAME);

      sendSMS(sender, msg);
    }

    // ACK
    char ack[60];
    sprintf(ack, "ACK\nID:%s\nIP:%s\nSIM:%s",
            DEVICE_ID, IP_PORT, SIM_NAME);

    sendSMS(sender, ack);

    smsBuffer[0] = '\0';   // clear
    waitForSMS = false;
    smsProcessing = false;
  }
}
    }
    else
    {
      if (index < sizeof(line) - 1)
        line[index++] = c;
    }
  }
}

// ------------------------------
void sendSMS(const char *number, const char *msg)
{
  gsm.println("AT+CMGF=1");
  delay(500);
  while (gsm.available()) gsm.read();
  gsm.print("AT+CMGS=\"");
  gsm.print(number);
  gsm.println("\"");
  delay(500);

  gsm.print(msg);
  delay(500);

  gsm.write(26);
  delay(3000);
}

// ------------------------------
int mpuTo1023(int val)
{
  val = constrain(val, -16384, 16384);
  return map(val, -16384, 16384, 0, 1023);
}

// ------------------------------
void readMPU()
{
  int16_t ax, ay, az, gx, gy, gz;

  xSum = 0;
  zSum = 0;

  for (int i = 0; i < SAMPLE_COUNT; i++)
  {
    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    xSum += ax;
    zSum += az;
    delay(20);
  }

  xAvg = mpuTo1023(xSum / SAMPLE_COUNT);
  zAvg = mpuTo1023(zSum / SAMPLE_COUNT);
}

// ------------------------------
void setup()
{
  Serial.begin(9600);
  gsm.begin(9600);

  loadConfig();

  Wire.begin();
  mpu.initialize();

  initGSM();
  delay(2000);

  // 🔴 Wait until network registers
  while (!isGSMConnected())
  {
    Serial.println(F("Waiting for network..."));
    delay(2000);
  }

  detectSIM();
  if (strcmp(SIM_NAME, "UNKNOWN") == 0)
{
  Serial.println(F("Retry SIM detection..."));
  delay(2000);
  detectSIM();
}
}

void waitWithSMS(unsigned long duration)
{
  unsigned long start = millis();

  while (millis() - start < duration)
  {

   checkSMS();
  if (smsReceived || smsProcessing)
  {
    Serial.println(F("⛔ SMS priority → skipping cloud"));
    gsmBusy = false;
    return;
  }
  }
}

// ------------------------------
void loop()
{
  // 🔴 ALWAYS CHECK SMS FIRST
  checkSMS();

  if (smsProcessing)
  {
    return; // 🔥 FULL STOP
  }

  if (smsReceived)
  {
    Serial.println(F("📩 SMS handled → skipping cycle"));
    smsReceived = false;
    return;
  }

  // 🔴 Normal operation
  if (millis() - lastSendTime >= sendInterval)
  {
    lastSendTime = millis();

    Serial.println(F("\n==== NEW CYCLE ===="));

    readMPU();

    // 🔴 Check SMS AGAIN before sending
    checkSMS();
    if (smsProcessing)
    {
      return;  // 🔥 STOP EVERYTHING
    }
    if (smsReceived)
    {
      smsReceived = false;
      delay(3000);
      return;
    }

    sendToCloud();
  }
}