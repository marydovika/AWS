#include "MEMORY.h"
#include <Arduino.h>
#include <SD.h>
#include <string>

File MemoryFile;
const int chipSelectPin = 4; 

Memory::Memory() {}

void Memory::setupMemory() {
  Serial.begin(9600);
  if (!SD.begin(chipSelectPin)) {
    Serial.println("Card failed, or not present");
    return;
  }
  Serial.println("SD card initialized.");

}

void Memory::createFile(const string& filename) {
      if (SD.exists(filename.c_str())) {
    Serial.print(filename.c_str());
    Serial.println(" exists.");
    return;
  }else{
    MemoryFile = SD.open(filename.c_str(), FILE_WRITE);
    if (MemoryFile) {
        Serial.print("File created: ");
        Serial.println(filename.c_str());
        MemoryFile.close();
    } else {
        Serial.print("Error creating file: ");
        Serial.println(filename.c_str());
    }
 }
}

void Memory::readData(const string& filename) {
  if (SD.exists(filename.c_str())) {
    Serial.print(filename.c_str());
    Serial.println(" exists.");
    File f = SD.open(filename.c_str());
    if (f) {
      while (f.available()) {
        Serial.write(f.read());
      }
      f.close();
    }
  } else {
    Serial.print(filename.c_str());
    Serial.println(" doesn't exist.");
    Serial.print("Creating ");
    createFile(filename);
    if (SD.exists(filename.c_str())) {
      Serial.print(filename.c_str());
      Serial.println(" exists.");
    } else {
      Serial.print(filename.c_str());
      Serial.println(" doesn't exist.");
    }
  }
}

void Memory::writeData(const string& filename, const string& data) {
  MemoryFile = SD.open(filename.c_str(), FILE_APPEND);
  if (MemoryFile) {
    MemoryFile.println(data.c_str());
    MemoryFile.close();
    Serial.print("Data written to ");
    Serial.println(filename.c_str());
  } else {
    Serial.print("Error opening ");
    Serial.println(filename.c_str());
  }
}

void Memory::clearFile(const string& filename) {
  if (SD.exists(filename.c_str())) {
    SD.remove(filename.c_str());
    Serial.print("File cleared: ");
    Serial.println(filename.c_str());
    createFile(filename);
  } else {
    Serial.print("File does not exist: ");
    Serial.println(filename.c_str());
  }
}