#pragma once
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <vector>
namespace Replay {
enum class Kind {Keyboard,Mouse};
struct Event {std::uint64_t at_us=0;Kind kind=Kind::Keyboard;std::uint16_t code=0,flags=0,buttons=0,data=0;std::int32_t x=0,y=0;};
struct Recording {std::string name="Untitled recording";Kind kind=Kind::Keyboard;std::vector<Event> events;};
void Validate(const Recording& recording);
Recording Parse(std::string_view json);
std::string Serialize(const Recording& recording);
std::string Csv(const Recording& recording);
std::string Describe(const Event& event);
class Held {
    std::set<std::uint32_t> keys_;std::uint16_t buttons_=0;
public:
    void Accept(const Event& event);
    std::vector<Event> Release();
};
}
