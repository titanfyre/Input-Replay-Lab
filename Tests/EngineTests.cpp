#include "Engine.h"
#include "Interceptor.h"
#include <chrono>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
namespace {
std::mutex mutex;
std::vector<Interceptor::Device> devices;
std::vector<Replay::Event> sent;
unsigned opens=0,closes=0;
void Require(bool ok){if(!ok)throw std::runtime_error("Fake-transport engine assertion failed.");}
void Reset(){std::lock_guard<std::mutex> lock(mutex);devices={{}};devices[0].id=1;devices[0].type=IC_TYPE_KEYBOARD;devices[0].token="capture-A:keyboard:1";sent.clear();opens=closes=0;}
template<class F>void Until(F ready){const auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(2);while(!ready()){if(std::chrono::steady_clock::now()>=limit)throw std::runtime_error("Fake-transport engine test timed out.");std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
std::size_t Count(){std::lock_guard<std::mutex> lock(mutex);return sent.size();}
Replay::Recording Two(bool delayed){Replay::Recording r;r.events={{0,Replay::Kind::Keyboard,30},{delayed?10000000ULL:1000ULL,Replay::Kind::Keyboard,30,1}};return r;}
}
namespace Interceptor {
Init_Result Init_AllDevices(){std::lock_guard<std::mutex> lock(mutex);++opens;return {};}
std::vector<Device> Get_Devices(){std::lock_guard<std::mutex> lock(mutex);return devices;}
Input_Batch Get_Input_With_Timeout(DWORD timeout,HANDLE stop){WaitForSingleObject(stop,timeout);return {};}
void Destroy_Context(){std::lock_guard<std::mutex> lock(mutex);++closes;}
Send_Result Send_Input(Input_Batch& b){return {b.size(),0};}
Send_Result Send_Input(const Keyboard_Input& k){std::lock_guard<std::mutex> lock(mutex);Replay::Event e;e.code=k.stroke.make_code;e.flags=k.stroke.flags;sent.push_back(e);return {1,0};}
Send_Result Send_Input(const Mouse_Input& m){std::lock_guard<std::mutex> lock(mutex);Replay::Event e;e.kind=Replay::Kind::Mouse;e.x=m.stroke.x;e.y=m.stroke.y;e.buttons=m.stroke.button_flags;e.data=m.stroke.button_data;e.flags=m.stroke.flags;sent.push_back(e);return {1,0};}
}
int main(){try{
 Reset();{Replay::Engine e;e.Start(nullptr);std::this_thread::sleep_for(std::chrono::milliseconds(10));{std::lock_guard<std::mutex> lock(mutex);Require(opens==0);}e.Scan();Until([&]{return e.Read().connected;});e.Load(Two(false));Until([&]{return e.Read().document!=nullptr;});e.Replay("capture-A:keyboard:1");Until([&]{return Count()==2&&!e.Read().replaying;});{std::lock_guard<std::mutex> lock(mutex);Require(sent[0].code==30&&!sent[0].flags&&sent[1].flags==1);}e.Close();}
 Reset();{Replay::Engine e;e.Start(nullptr);e.Scan();Until([&]{return e.Read().connected;});e.Load(Two(true));Until([&]{return e.Read().document!=nullptr;});e.Replay("capture-A:keyboard:1");Until([&]{return Count()==1;});e.StopAction();Until([&]{return Count()==2&&!e.Read().replaying;});{std::lock_guard<std::mutex> lock(mutex);Require(sent.back().flags==1);}e.Close();}
 Reset();{Replay::Engine e;e.Start(nullptr);e.Scan();Until([&]{return e.Read().connected;});e.Load(Two(true));Until([&]{return e.Read().document!=nullptr;});e.Replay("capture-A:keyboard:1");Until([&]{return Count()==1;});{std::lock_guard<std::mutex> lock(mutex);devices[0].token="capture-A:keyboard:replacement";}e.StopAction();Until([&]{return !e.Read().connected;});Require(Count()==1);const auto old=e.Read().document;e.Load(Two(false));Until([&]{return e.Read().document!=old;});e.Scan();Until([&]{return e.Read().connected;});e.Replay("capture-A:keyboard:replacement");Until([&]{return Count()==3&&!e.Read().replaying;});e.Close();}
 std::cout<<"3 fake-transport scenarios passed: explicit start, timed replay, cancel/release and fail-closed token replacement. No driver or host input used.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
