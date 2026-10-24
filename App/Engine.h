#pragma once
#include "Recording.h"
#include <Windows.h>
#include <memory>
#include <string>
namespace Replay {
constexpr UINT Changed=WM_APP+31;
struct Device {std::string token;std::wstring name;Kind kind=Kind::Keyboard;std::uint32_t id=0;};
struct Snapshot {
    bool connected=false,recording=false,replaying=false;
    std::wstring message=L"Choose Scan devices to explicitly open a capture session. Physical input is forwarded unchanged.";
    std::vector<Device> devices;
    std::shared_ptr<const Recording> document;
    std::uint64_t count=0,sent=0,max_late_us=0;
    std::wstring latest;
};
class Engine {
    struct Impl;std::unique_ptr<Impl> impl_;
public:
    Engine();~Engine();
    void Start(HWND window);
    void Scan();
    void Record(std::string token);
    void Replay(std::string token);
    void StopAction();
    void Load(Recording recording);
    void Close();
    Snapshot Read();
};
}
