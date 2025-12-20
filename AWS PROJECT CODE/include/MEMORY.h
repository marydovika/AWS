#pragma once
#ifndef MEMORY_H
#define MEMORY_H
#include <string>
using namespace std;

class Memory {
    public:
        Memory();
        void setupMemory();
            void createFile(const string& filename);
            void readData(const string& filename);
            void writeData(const string& filename, const string& data);
            void clearFile(const string& filename);
   private:
        string filename_;
        string data_;

};
#endif