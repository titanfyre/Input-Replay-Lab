#include "Engine.h"
#include "Interceptor.h"
#include <algorithm>
#include <deque>
#include <mutex>
#include <thread>
#include <system_error>
namespace Replay {
namespace {
std::wstring Wide(const std::string& s){if(s.empty())return {};const int n=MultiByteToWideChar(CP_UTF8,0,s.data(),static_cast<int>(s.size()),nullptr,0);std::wstring w(static_cast<std::size_t>(n),0);MultiByteToWideChar(CP_UTF8,0,s.data(),static_cast<int>(s.size()),w.data(),n);return w;}
std::uint64_t Now(){LARGE_INTEGER n{};QueryPerformanceCounter(&n);return static_cast<std::uint64_t>(n.QuadPart);}
std::uint64_t Elapsed(std::uint64_t start,std::uint64_t frequency){const auto ticks=Now()-start;return (ticks/frequency)*1000000+(ticks%frequency)*1000000/frequency;}
void Accepted(const Interceptor::Send_Result& r,std::size_t expected){if(!r||r.accepted!=expected)throw std::system_error(static_cast<int>(r.error?r.error:ERROR_WRITE_FAULT),std::system_category(),"Input forwarding was incomplete");}
}
struct Engine::Impl {
    enum class Action{Scan,Record,Replay,Stop,Load};
    struct Command {Action action=Action::Stop;std::string token;std::shared_ptr<const Recording> document;};
    HWND window=nullptr;std::mutex mutex;HANDLE wake=CreateEventW(nullptr,FALSE,FALSE,nullptr);std::thread worker;bool closing=false,posted=false;std::deque<Command> commands;Snapshot snapshot;
    Impl(){if(!wake)throw std::runtime_error("Cannot create capture wake event.");}
    ~Impl(){CloseHandle(wake);}
    void Publish(const Snapshot& s){std::lock_guard<std::mutex> lock(mutex);snapshot=s;if(window&&!posted)posted=PostMessageW(window,Changed,0,0)!=FALSE;}
    void Add(Command c){std::lock_guard<std::mutex> lock(mutex);if(closing||commands.size()>=16)return;commands.push_back(std::move(c));SetEvent(wake);}
    void Run() noexcept {
        Snapshot state;Recording recording;Device selected;Held held;std::uint64_t start=0,frequency=0,published=0;std::size_t position=0;
        LARGE_INTEGER f{};QueryPerformanceFrequency(&f);frequency=static_cast<std::uint64_t>(f.QuadPart);
        const auto send=[&](const Event& e){
            const auto current=Interceptor::Get_Devices();
            const auto target=std::find_if(current.begin(),current.end(),[&](const auto& d){return d.token==selected.token&&d.id==selected.id&&d.type==(selected.kind==Kind::Keyboard?IC_TYPE_KEYBOARD:IC_TYPE_MOUSE);});
            if(target==current.end())throw std::runtime_error("Selected capture-scoped device disappeared or changed identity; replay will not retarget another source");
            if(e.kind==Kind::Keyboard){Interceptor::Keyboard_Input k{};k.device=selected.id;k.stroke.make_code=e.code;k.stroke.flags=e.flags;Accepted(Interceptor::Send_Input(k),1);}
            else{Interceptor::Mouse_Input m{};m.device=selected.id;m.stroke.flags=e.flags;m.stroke.button_flags=e.buttons;m.stroke.button_data=e.data;m.stroke.x=e.x;m.stroke.y=e.y;Accepted(Interceptor::Send_Input(m),1);}
        };
        const auto release=[&]{for(const auto& e:held.Release())send(e);state.replaying=false;};
        const auto finishRecord=[&]{if(state.recording){state.recording=false;state.document=std::make_shared<const Recording>(std::move(recording));recording={};}};
        for(;;){try{
            for(;;){
                std::deque<Command> pending;{std::lock_guard<std::mutex> lock(mutex);if(closing)break;pending.swap(commands);}
                for(auto& c:pending){
                    if(c.action==Action::Stop){finishRecord();release();state.message=L"Stopped. Replay-owned held keys/buttons were released; physical input continues unchanged.";Publish(state);continue;}
                    if(c.action==Action::Load){finishRecord();release();state.document=std::move(c.document);state.count=state.document->events.size();state.latest.clear();state.message=L"Recording loaded offline. Select a matching device before replay.";Publish(state);continue;}
                    if(c.action==Action::Scan){finishRecord();release();if(state.connected)Interceptor::Destroy_Context();state.connected=false;const auto init=Interceptor::Init_AllDevices();if(!init.ready)throw std::system_error(static_cast<int>(init.error?init.error:ERROR_SUCCESS_REBOOT_REQUIRED),std::system_category(),"Install/associate Interceptor Keyboard & Mouse and restart if requested");const auto devices=Interceptor::Get_Devices();state.devices.clear();for(const auto& d:devices){if(d.type!=IC_TYPE_KEYBOARD&&d.type!=IC_TYPE_MOUSE)continue;Device item;item.token=d.token;item.id=d.id;item.kind=d.type==IC_TYPE_KEYBOARD?Kind::Keyboard:Kind::Mouse;item.name=reinterpret_cast<const wchar_t*>(d.name);if(item.name.empty())item.name=item.kind==Kind::Keyboard?L"Keyboard":L"Mouse";state.devices.push_back(std::move(item));}state.connected=true;state.message=L"Session opened. Select one device; recording starts only when you choose Record.";Publish(state);continue;}
                    if(!state.connected)throw std::runtime_error("Scan devices before recording or replaying.");
                    const auto found=std::find_if(state.devices.begin(),state.devices.end(),[&](const auto& d){return d.token==c.token;});if(found==state.devices.end())throw std::runtime_error("Selected device is no longer in this capture session. Scan again.");finishRecord();release();selected=*found;state.sent=state.max_late_us=0;
                    if(c.action==Action::Record){recording={};recording.kind=selected.kind;recording.name=selected.kind==Kind::Keyboard?"Keyboard recording":"Mouse recording";recording.events.reserve(100000);start=Now();state.recording=true;state.count=0;state.message=L"RECORDING selected device. Stop before typing secrets; this is an explicit local input recording.";}
                    else{if(!state.document||state.document->events.empty())throw std::runtime_error("Load or record a non-empty timeline first.");Validate(*state.document);if(state.document->kind!=selected.kind)throw std::runtime_error("Selected target family differs from the recording family.");position=0;start=Now();state.replaying=true;state.message=L"REPLAYING to the selected driver device. Stop cancels and releases replay-owned held input.";}Publish(state);
                }
                if(!state.connected){WaitForSingleObject(wake,INFINITE);continue;}
                if(state.replaying){
                    auto elapsed=Elapsed(start,frequency);unsigned run=0;
                    while(position<state.document->events.size()&&state.document->events[position].at_us<=elapsed&&run++<128){const auto& e=state.document->events[position];send(e);held.Accept(e);++position;++state.sent;state.max_late_us=(std::max)(state.max_late_us,elapsed-e.at_us);state.latest=Wide(Describe(e));elapsed=Elapsed(start,frequency);}
                    if(position==state.document->events.size()){release();state.message=L"Replay completed. Held inputs released. Reported lateness measures submission scheduling, not application delivery.";Publish(state);}
                }
                DWORD timeout=INFINITE;
                if(state.replaying){const auto elapsed=Elapsed(start,frequency),due=state.document->events[position].at_us;timeout=due<=elapsed?0:static_cast<DWORD>((std::min)(std::uint64_t(1000),(due-elapsed+999)/1000));}
                {auto batch=Interceptor::Get_Input_With_Timeout(timeout,wake);
                    for(auto& event:batch){if(!state.recording||!event.device_info||event.device_info->token!=selected.token)continue;Event e;e.at_us=Elapsed(start,frequency);e.kind=selected.kind;
                        if(auto* k=event.Get<Interceptor::Keyboard_Input>()){e.code=k->stroke.make_code;e.flags=k->stroke.flags;}
                        else if(auto* m=event.Get<Interceptor::Mouse_Input>()){e.flags=m->stroke.flags;e.buttons=m->stroke.button_flags;e.data=m->stroke.button_data;e.x=m->stroke.x;e.y=m->stroke.y;}else continue;
                        if(recording.events.size()>=100000||e.at_us>3600000000ULL){finishRecord();state.message=L"Recording stopped at the safety limit (100000 events / one hour).";Publish(state);continue;}
                        if(e.kind==Kind::Keyboard&&(!e.code||e.code>255||(e.flags&~7u)))continue;
                        if(e.kind==Kind::Mouse&&((e.flags&~15u)||(e.buttons&~0x1FFFu)))continue;
                        recording.events.push_back(e);state.count=recording.events.size();state.latest=Wide(Describe(e));
                    }
                    Accepted(Interceptor::Send_Input(batch),batch.size());
                }
                const auto now=Now();if((now-published)>frequency/10){Publish(state);published=now;}
            }
            finishRecord();if(state.connected){release();Interceptor::Destroy_Context();}break;
        }catch(const std::exception& e){try{finishRecord();release();}catch(...){}try{Interceptor::Destroy_Context();}catch(...){}state.connected=state.recording=state.replaying=false;state.devices.clear();state.message=L"Stopped safely: "+Wide(e.what())+L". Resolve the error, then Scan devices to reconnect; offline file loading remains available.";Publish(state);}
        catch(...){try{Interceptor::Destroy_Context();}catch(...){}state.connected=state.recording=state.replaying=false;state.devices.clear();state.message=L"Capture stopped after an unexpected error; physical passthrough restored. Scan devices to reconnect.";Publish(state);}}
    }
};
Engine::Engine():impl_(std::make_unique<Impl>()){}
Engine::~Engine(){Close();}
void Engine::Start(HWND window){impl_->window=window;impl_->worker=std::thread([this]{impl_->Run();});}
void Engine::Scan(){impl_->Add({Impl::Action::Scan,{},{}});}
void Engine::Record(std::string token){impl_->Add({Impl::Action::Record,std::move(token),{}});}
void Engine::Replay(std::string token){impl_->Add({Impl::Action::Replay,std::move(token),{}});}
void Engine::StopAction(){impl_->Add({Impl::Action::Stop,{},{}});}
void Engine::Load(Recording r){Validate(r);impl_->Add({Impl::Action::Load,{},std::make_shared<const Recording>(std::move(r))});}
void Engine::Close(){{std::lock_guard<std::mutex> lock(impl_->mutex);impl_->closing=true;SetEvent(impl_->wake);}if(impl_->worker.joinable())impl_->worker.join();}
Snapshot Engine::Read(){std::lock_guard<std::mutex> lock(impl_->mutex);impl_->posted=false;return impl_->snapshot;}
}
