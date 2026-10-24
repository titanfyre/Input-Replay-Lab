#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <utility>
#define IC_TYPE_KEYBOARD 1u
#define IC_TYPE_MOUSE 2u
namespace Interceptor {
struct Device {std::uint32_t id=1,type=1;std::uint16_t name[128]{};std::string token;};
struct Keyboard_Input {std::uint32_t device=0;struct {std::uint16_t make_code=0,flags=0;}stroke;};
struct Mouse_Input {std::uint32_t device=0;struct {std::uint16_t flags=0,button_flags=0,button_data=0;std::int32_t x=0,y=0;}stroke;};
struct Send_Result {std::size_t accepted=0;DWORD error=0;explicit operator bool()const{return !error;}};
struct Init_Result {bool ready=true;DWORD error=0;};
struct Input_Event {const Device* device_info=nullptr;template<class T>T* Get(){return nullptr;}};
struct Input_Batch {std::vector<Input_Event> events;auto begin(){return events.begin();}auto end(){return events.end();}std::size_t size()const{return events.size();}};
Init_Result Init_AllDevices();
std::vector<Device> Get_Devices();
Input_Batch Get_Input_With_Timeout(DWORD timeout,HANDLE stop);
void Destroy_Context();
Send_Result Send_Input(Input_Batch&);
Send_Result Send_Input(const Keyboard_Input&);
Send_Result Send_Input(const Mouse_Input&);
}
