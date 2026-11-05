#include <SD.h>
#include <SPI.h>
#include <MFRC522.h>
#include "AudioFileSourceSD.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

//Pins Used
#define SD_CS      5
#define SD_SCK    14
#define SD_MISO   32
#define SD_MOSI   13
#define RC522_SCK   18
#define RC522_MISO  19
#define RC522_MOSI  23
#define RC522_SS   4
#define RC522_RST 16
#define I2S_BCLK  26
#define I2S_LRC   25
#define I2S_DIN   27

// #define TEST_PLAY_ON_BOOT
#define GAIN 0.15  //Volume (0.0 - 4.0)
#define MISS_INTERVAL_CHECK 500   //In ms
#define MISSES_NEEDED 3


//NFC tag to song lists
//Structure
struct TagEntry {
  const char* uid;   // uppercase hex
  const char* file;  // SD card path
};

//Map object
//UID tags aren't constant, new ones need changed
TagEntry tagMap[] = {
  {"0493C1E2450289", "/Blocks.mp3"},
  {"04535B35450289", "/Cat.mp3"},
  {"xxxxxxxxxxxxxx", "/Chirp.mp3"},
  {"xxxxxxxxxxxxxx", "/Far.mp3"},
  {"xxxxxxxxxxxxxx", "/Mall.mp3"},
  {"xxxxxxxxxxxxxx", "/Mellohi.mp3"},
  {"xxxxxxxxxxxxxx", "/Stal.mp3"},
  {"xxxxxxxxxxxxxx", "/Strad.mp3"},
  {"xxxxxxxxxxxxxx", "/Wait.mp3"},
  {"xxxxxxxxxxxxxx", "/Ward.mp3"},
  {"xxxxxxxxxxxxxx", "/Pigstep.mp3"},
  {"xxxxxxxxxxxxxx", "/otherside.mp3"},
  {"xxxxxxxxxxxxxx", "/Relic.mp3"},
  {"xxxxxxxxxxxxxx", "/Creator.mp3"},
  {"xxxxxxxxxxxxxx", "/Precipice.mp3"},
  {"04F34BE2450289", "/Tears.mp3"},
  {"xxxxxxxxxxxxxx", "/Bounce.mp3"},
  {"0473B6E0450289", "/con lentitud poderosa.mp3"},
  {"046366DD450289", "/xxxxxx.mp3"},
  {"04D31CE2450289", "/xxxxxx.mp3"},
};
const int tagCount = sizeof(tagMap) / sizeof(tagMap[0]);

//Global Variables
MFRC522 rfid(RC522_SS, RC522_RST); // Instance of the RC522 class
SPIClass sdSPI(HSPI);              // Instance of SD class

AudioGeneratorMP3 *mp3 = nullptr;
AudioFileSourceSD *src = nullptr;
AudioOutputI2S *out = nullptr;

String currentUid = "";
int missCount = 0;
unsigned long lastCheck = 0;

//Helper Functions
String uidToString(MFRC522::Uid &uid) {
  String s = "";
  for (byte i = 0; i < uid.size; i++) {
    if (uid.uidByte[i] < 0x10) s += "0";
    s += String(uid.uidByte[i], HEX);
  }
  s.toUpperCase();
  return s;
}

const char* findSong(const String &uid) {
  for (int i = 0; i < tagCount; i++) {
    if (uid.equals(tagMap[i].uid)) return tagMap[i].file;
  }
  return nullptr;
}

void stopTrack() {
  if (mp3) {
    if (mp3->isRunning()) mp3->stop();
    delete mp3;
    mp3 = nullptr;
  }
  if (src) {
    delete src;
    src = nullptr;
  }
}

void playTrack(const char* path) {
  stopTrack();
  src = new AudioFileSourceSD(path);
  mp3 = new AudioGeneratorMP3();
  if (!mp3->begin(src, out)) {
    Serial.print("Could not play ");
    Serial.println(path);
    stopTrack();
  }
}

// Returns true if a tag is in range; fills uidOut with its UID
bool readTag(String &uidOut) {
  byte atqa[2];
  byte atqaSize = sizeof(atqa);
  MFRC522::StatusCode status = rfid.PICC_WakeupA(atqa, &atqaSize);
  if (status != MFRC522::STATUS_OK) return false;
  if (!rfid.PICC_ReadCardSerial()) return false;
  uidOut = uidToString(rfid.uid);
  rfid.PICC_HaltA();
  return true;
}

//----------------------------------------------------------------
void setup() {
  //Setup code, runs once
  Serial.begin(115200);

  pinMode(SD_CS, OUTPUT);     
  digitalWrite(SD_CS, HIGH);
  pinMode(RC522_SS, OUTPUT);  
  digitalWrite(RC522_SS, HIGH);

  SPI.begin(RC522_SCK, RC522_MISO, RC522_MOSI, RC522_SS);       // RC522
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);            // SD card

  if (!SD.begin(SD_CS, sdSPI)) {
    Serial.println("SD card mount FAILED (check wiring, FAT32 format)");
  } else {
    Serial.println("SD card OK");
  }

  out = new AudioOutputI2S();
  out->SetPinout(I2S_BCLK, I2S_LRC, I2S_DIN);
  out->SetGain(GAIN);

  #ifdef TEST_PLAY_ON_BOOT    //Old test case conditional start, skipped over in final version
    Serial.println("TEST MODE: playing Blocks");
    audio.connecttoFS(SD, "\blocks.mp3");
  #else
    rfid.PCD_Init();
    rfid.PCD_DumpVersionToSerial();
    Serial.println("RC522 ready: Insert disc");
  #endif  
}

//----------------------------------------------------------------
void loop() {
  //loop code, constantly running
  // Keep audio running constantly
  if (mp3 && mp3->isRunning()) {
    if (!mp3->loop()) {
      mp3->stop();
      Serial.println("Song finished");
    }
  }

  #ifndef TEST_PLAY_ON_BOOT
   if (millis() - lastCheck < MISS_INTERVAL_CHECK) return;
   lastCheck = millis();

    String uid;
    if (readTag(uid)) {
     missCount = 0;
      if (uid != currentUid) {            // New disc inserted
        currentUid = uid;
        Serial.print("Tag UID: ");
        Serial.println(uid);
        const char* file = findSong(uid);
        if (file) {                       //Check if uid tag is included
         Serial.print("Playing ");
         Serial.println(file);
         playTrack(file);
       } else {
          Serial.println("Unknown tag, add this UID");
        }
      }
    } else if (currentUid != "") {
     missCount++;
     if (missCount >= MISSES_NEEDED) {  // disc removed
        Serial.println("Disc removed");
        stopTrack();
        currentUid = "";
        missCount = 0;
     }
    }
  #endif
}
