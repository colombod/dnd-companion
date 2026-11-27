#pragma once
#include <string>
#include <cstdint>
#include <cstring>
#include <ctime>
class String : public std::string {
public:
    using std::string::string;
    String(const std::string &s) : std::string(s) {}
    explicit String(uint32_t n) : std::string(std::to_string(n)) {}
};
inline uint32_t fakeMillis;
inline uint32_t millis() { return fakeMillis; }
inline void configTime(int,int,const char *,const char *) {}
