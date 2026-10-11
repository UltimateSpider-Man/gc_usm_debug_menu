// End-to-end recorder dataflow through the real GC engine, input and menu
// implementations. Only SDK/game entry points are replaced in mapped MEM1.
#include "../../gamecube/gc_engine.cpp"
#include "../../gamecube/gc_input.cpp"
#include "../../gamecube/debug_menu_core.cpp"
#include "../../gamecube/menus/replay_gc.cpp"
using namespace usm::gc;
namespace {
constexpr uptr test_game = 0x81000000u, test_settings = 0x81001000u;
constexpr uptr test_world = 0x81002000u, test_hero = 0x81003000u, test_terrain = 0x81004000u;
u32 checks, failures, pad_reads, pauses, unpauses, physics_calls;
bool hero_busy, mission_busy;
s32 native_state = 6;
PadStatus hardware[4], delivered[4];
menu::Menu* replay_menu;

int syscall(u32 n,u32 a,u32 b=0u,u32 c=0u){int r;asm volatile("int $0x80":"=a"(r):"a"(n),"b"(a),"c"(b),"d"(c):"memory");return r;}
void print(const char* s){syscall(4u,1u,reinterpret_cast<uptr>(s),strlen(s));}
void check(bool ok,const char* text){++checks;if(!ok){++failures;print("FAIL: ");print(text);print("\n");}}
bool equal(const char* a,const char* b){return strlen(a)==strlen(b)&&memcmp(a,b,strlen(a))==0;}
bool contains(const char* text,const char* part){for(u32 i=0;text[i];++i){u32 j=0;while(part[j]&&text[i+j]==part[j])++j;if(!part[j])return true;}return false;}
u32& test_word(uptr p){return *reinterpret_cast<u32*>(p);}
s32 state_native(void*){return native_state;}
void pause_native(void*){++pauses;native_state=7;test_word(test_game+0x1B0u)=1u;}
void unpause_native(void*){++unpauses;native_state=6;test_word(test_game+0x1B0u)=0u;}
void physics_native(void*,bool){++physics_calls;}
u32 pads_native(PadStatus* pads){++pad_reads;if(pads)memcpy(pads,hardware,sizeof(hardware));return 0x12345678u;}
void boundary(uptr p,uptr fn){*reinterpret_cast<u8*>(p)=0xE9u;test_word(p+1u)=fn-p-5u;}
void read_input(bool focus=false){input::read_for_game(delivered,focus);}
void configure_objects(){
 test_word(address::game_pointer)=test_game;test_word(address::world_pointer)=test_world;
 test_word(test_game+0xD8u)=test_settings;test_word(test_game+0x68u)=test_world;
 test_word(test_game+0x5Cu)=1u;test_word(test_game+0x1A0u)=1u;test_word(test_game+0x1A8u)=0u;test_word(test_game+0x1B0u)=0u;
 test_word(test_world+0x224u)=1u;test_word(test_world+0x21Cu)=test_hero;test_word(test_world+0x198u)=test_terrain;
 test_word(test_hero+0x1Cu)=0x10203040u;
}
void reset_fixture(){
 replay::clear_clip();menu::reset();configure_objects();native_state=6;hero_busy=false;mission_busy=false;
 memset(hardware,0,sizeof(hardware));for(u32 i=0;i<4u;++i)hardware[i].error=-1;
 read_input();input::poll();hardware[0].error=0;read_input();input::poll();
 replay_menu=menu::create("Replay",replay::recommended_capacity);replay::configure_replay(*replay_menu);menu::set_root(*replay_menu);
 pauses=unpauses=physics_calls=pad_reads=0u;
}
void open_menu(){input::Frame frame{};frame.toggle_menu=true;menu::update(frame);menu::dispatch_deferred();}
void select(u32 index){auto& entry=replay_menu->entries[index];entry.handler(entry,menu::Command::activate);}
void begin_record(){open_menu();select(0u);menu::dispatch_deferred();}
void begin_play(){open_menu();select(1u);menu::dispatch_deferred();}
void capture(s8 x,u16 buttons=0u,float dt=0.02f){hardware[0].stick_x=x;hardware[0].button=buttons;read_input();replay::frame_advance(dt);}
void make_clip(){reset_fixture();begin_record();capture(47,0x0200u,0.02f);capture(-63,0x0400u,0.03f);select(2u);}
void format(u32 index,char* out,u32 size){replay_menu->entries[index].formatter(replay_menu->entries[index],out,size);}
bool stopped(){return replay::mode==replay::Mode::idle;}
} // namespace
namespace usm::gc::hero_select {bool busy(){return hero_busy;}}
namespace usm::gc::mission_level {bool busy(){return mission_busy;}}
extern "C" int run_tests(){
 using namespace usm::gc::replay;
 u32 mapping[]={0x80000000u,0x1800000u,7u,0x32u,0xFFFFFFFFu,0u};
 if(static_cast<uptr>(syscall(90u,reinterpret_cast<uptr>(mapping)))!=mapping[0])return 2;
 boundary(address::game_get_state,reinterpret_cast<uptr>(state_native));
 boundary(address::game_pause,reinterpret_cast<uptr>(pause_native));
 boundary(address::game_unpause,reinterpret_cast<uptr>(unpause_native));
 boundary(address::game_enable_physics,reinterpret_cast<uptr>(physics_native));
 boundary(address::pad_read,reinterpret_cast<uptr>(pads_native));
 reset_fixture();
 check(recommended_capacity==6u&&replay_menu->count==6u,"Replay exposes Record, Play, Stop, Clear and live state/count controls");
 const char* const expected[]={"Record","Play","Stop","Clear","State","Samples / Duration"};
 bool names=true;for(u32 i=0u;i<6u;++i)names=names&&equal(replay_menu->entries[i].label,expected[i]);
 check(names,"Replay menu row order matches its user controls");
 char text[128];format(1u,text,sizeof(text));
 check(equal(text,"current position; world not rewound"),"Play explains current-world semantics before activation");
 format(4u,text,sizeof(text));check(equal(text,"idle / empty"),"new menu reports an empty recorder");
 select(1u);check(stopped()&&!menu::visible()&&stored_samples==0u,"Play rejects an absent recording without starting a stream");
 open_menu();const u32 before_empty_close=unpauses;select(1u);
 check(menu::visible()&&unpauses==before_empty_close&&contains(menu::status_text,"select Record first"),"empty Play retains the menu with a useful diagnosis");
 menu::close();menu::dispatch_deferred();

 reset_fixture();open_menu();select(0u);
 check(mode==Mode::arm_record&&!menu::visible()&&menu::owns_input(),"Record closes the real menu and waits for deferred unpause");
 check(pauses==1u&&unpauses==0u,"arming does not call native unpause from the render/input phase");
 menu::dispatch_deferred();check(native_state==6&&!menu::owns_input()&&unpauses==1u,"stock deferred unpause releases recording input once");
 hardware[0].button=0x100u;hardware[0].stick_x=71;read_input();frame_advance(0.02f);
 check(mode==Mode::arm_record&&stored_samples==0u&&delivered[0].button==0u&&delivered[0].stick_x==0,"held menu Accept is suppressed until released rather than recorded as gameplay");
 hardware[0].button=0x200u;hardware[0].stick_x=47;hardware[0].stick_y=-35;
 hardware[0].substick_x=-12;hardware[0].substick_y=31;hardware[0].trigger_left=93u;
 hardware[0].trigger_right=177u;hardware[0].analog_a=17u;hardware[0].analog_b=29u;
 const PadStatus first=hardware[0];read_input();
 check(mode==Mode::recording&&stored_samples==1u&&memcmp(&delivered[0],&first,sizeof(first))==0,"first released sample records and delivers the complete native PadStatus");
 hardware[0].stick_x=-80;hardware[0].button=0x400u;read_input();read_input();
 check(stored_samples==1u&&memcmp(&delivered[0],&first,sizeof(first))==0,"multiple per-controller PADRead calls share exactly one recorded game-update sample");
 frame_advance(0.02f);check(duration_seconds>0.019f&&duration_seconds<0.021f,"duration counts the stock update once instead of once per controller poll");
 capture(-63,0x400u,0.03f);select(2u);
 check(stopped()&&stored_samples==2u&&clip_valid,"Stop retains a playable bounded recording");
 check(duration_seconds>0.049f&&duration_seconds<0.051f,"recorded duration uses real update time increments");
 format(5u,text,sizeof(text));check(contains(text,"2 / 3600, 0.")&&text[strlen(text)-1u]=='s',"samples and measured duration are displayed together");
 check(physics_calls==0u&&test_word(test_game+0x1A8u)==0u,"recording never toggles physics or changes native actor state");

 hardware[0]={};hardware[0].error=0;begin_play();read_input();
 check(mode==Mode::playing&&playback_cursor==0u&&delivered[0].stick_x==47&&delivered[0].button==0x200u,"Play supplies recorded input at current position while physical input is neutral");
 check(memcmp(&delivered[0],&first,sizeof(first))==0,"playback preserves sticks, triggers, analog buttons and controller status");
 read_input();read_input();check(playback_cursor==0u&&delivered[0].stick_x==47,"all native controller polls receive the same playback frame");
 frame_advance(0.02f);check(playback_cursor==1u&&mode==Mode::playing,"playback advances one sample at the stock update boundary");
 read_input();check(delivered[0].stick_x==-63&&delivered[0].button==0x400u,"next update delivers the next authored input sample");
 frame_advance(0.02f);check(stopped()&&playback_cursor==2u&&clip_valid,"playback automatically completes and preserves the recording");
 hardware[0].stick_x=19;read_input();check(delivered[0].stick_x==19,"live controller input resumes after playback completes");
 check(stored_samples==2u&&duration_seconds>0.049f&&duration_seconds<0.051f,"playback does not alter recording length or duration");

 make_clip();hardware[0]={};hardware[0].error=0;begin_play();read_input();
 hardware[0].button=0x10u;read_input();
 check(stopped()&&playback_cursor==0u&&stored_samples==2u,"physical Z stops playback before another sample is injected or consumed");
 check(delivered[0].button==0u&&delivered[0].stick_x==0,"the Z stop poll neutralizes gameplay output");
 const auto z_frame=input::poll();
 check(z_frame.toggle_menu&&!z_frame.accept&&!z_frame.cancel,"raw physical Z reaches menu navigation even while recorded buttons are injected");
 check(input::latest_pads[0].button==0x10u&&input::latest_pads[0].stick_x==0,"raw input observations retain physical state rather than playback data");

 reset_fixture();begin_record();capture(25);hardware[0].button=0x10u;read_input();frame_advance(0.02f);
 check(stopped()&&stored_samples==1u&&clip_valid,"Z stops recording without recording the menu-opening input");
 check((samples[0].pads[0].button&0x10u)==0u,"stored gameplay samples never include Z");
 reset_fixture();begin_record();hardware[0].button=0;read_input(true);frame_advance(0.02f);
 check(mode==Mode::arm_record&&stored_samples==0u,"menu-owned focus produces no recording samples");
 read_input();frame_advance(0.02f);const u32 count_before_pause=stored_samples;
 native_state=7;read_input();frame_advance(0.02f);
 check(stored_samples==count_before_pause&&clip_valid&&mode==Mode::recording,"native paused state holds the recorder without expiring it");
 native_state=14;read_input();frame_advance(0.02f);
 check(stored_samples==count_before_pause&&clip_valid&&mode==Mode::recording,"native pause-return state 14 consumes no samples and retains the recording");
 native_state=6;capture(31);
 check(stored_samples==count_before_pause+1u,"recording resumes when native gameplay resumes");

 make_clip();hardware[0]={};hardware[0].error=0;begin_play();
 hardware[0].error=-2;read_input();frame_advance(0.02f);
 check(mode==Mode::arm_play&&playback_cursor==0u&&delivered[0].error==-2,"transient SDK status during arming is preserved without consuming input");
 hardware[0].error=0;read_input();frame_advance(0.02f);
 hardware[0].error=-3;read_input();frame_advance(0.02f);
 check(mode==Mode::playing&&playback_cursor==1u&&delivered[0].error==-3&&delivered[0].button==0u,"transient SDK transfer failure holds playback and does not forge a connected controller");
 hardware[0].error=0;read_input();frame_advance(0.02f);
 check(stopped()&&playback_cursor==2u,"playback resumes at its held sample after transient input recovers");
 make_clip();hardware[0]={};hardware[0].error=0;begin_play();read_input();frame_advance(0.02f);
 hardware[0].error=-1;read_input();frame_advance(0.02f);
 check(stopped()&&playback_cursor==1u&&clip_valid,"real controller disconnection stops playback while retaining its clip");
 reset_fixture();begin_record();capture(22);hardware[0].error=-2;read_input();frame_advance(0.02f);
 check(mode==Mode::recording&&stored_samples==1u,"transient controller failure does not add an invalid recording sample");
 hardware[0].error=0;capture(23);hardware[0].error=-1;read_input();
 check(stopped()&&stored_samples==2u,"real disconnection stops capture safely");

 // All four ports are stored, while the native PAD reset mask is preserved.
 reset_fixture();hardware[1].error=0;hardware[1].stick_x=32;hardware[1].button=0x400u;
 begin_record();capture(12);select(2u);hardware[0]={};hardware[0].error=0;hardware[1]={};hardware[1].error=0;
 begin_play();const u32 reset_mask=input::read_for_game(delivered,false);
 check(reset_mask==0x12345678u&&delivered[1].stick_x==32&&delivered[1].button==0x400u,"playback restores additional controller ports while preserving the SDK reset mask");
 frame_advance(0.02f);

 make_clip();const float previous_duration=duration_seconds;
 frame_advance(0.25f);check(duration_seconds==previous_duration,"stock updates with no captured input do not extend duration");
 begin_record();capture(7,0u,-1.0f);capture(8,0u,1.0f);
 float nan_value;u32 nan_bits=0x7FC00000u;memcpy(&nan_value,&nan_bits,4u);capture(9,0u,nan_value);
 check(stored_samples==3u&&duration_seconds==0.0f,"invalid, outlier and NaN update durations cannot poison the recording clock");
 capture(10,0u,0.125f);check(duration_seconds==0.125f,"valid timing resumes after invalid input without accumulating invented time");
 format(5u,text,sizeof(text));check(contains(text,"4 / 3600, 0.1s"),"duration is rendered using the menu's native tenth-second display precision");

 // Pointer reuse must not let a recording inject into a different hero.
 make_clip();test_word(test_hero+0x1Cu)=0x10203041u;frame_advance(0.02f);
 check(!clip_valid&&stopped()&&stored_samples==2u,"hero handle replacement expires the clip even at a reused actor address");
 begin_play();check(stopped()&&playback_cursor==0u,"expired clip cannot inject into a replacement hero");
 make_clip();test_word(test_world+0x21Cu)=test_hero+0x200u;test_word(test_hero+0x200u+0x1Cu)=0x9988u;frame_advance(0.02f);
 check(!clip_valid&&stopped(),"hero pointer changes invalidate the recorded owner");
 make_clip();test_word(test_world+0x198u)=test_terrain+0x100u;frame_advance(0.02f);
 check(!clip_valid,"terrain owner changes invalidate the recorded world");
 make_clip();test_word(test_game+0x1A0u)=0u;frame_advance(0.02f);
 check(!clip_valid,"native level unload invalidates input playback before stale world use");
 make_clip();test_word(test_game+0x68u)=test_world+0x100u;frame_advance(0.02f);
 check(!clip_valid,"game/world ownership mismatch invalidates the clip");
 make_clip();test_word(address::world_pointer)=0u;frame_advance(0.02f);
 check(!clip_valid,"missing world cannot retain an injectable clip");
 make_clip();test_word(address::game_pointer)=0u;frame_advance(0.02f);
 check(!clip_valid,"missing game invalidates without dereferencing stale owners");

 reset_fixture();open_menu();menu::set_interaction_locked(true);select(0u);
 check(stopped()&&menu::visible()&&menu::interaction_is_locked(),"Record respects an existing native operation lock");
 menu::set_interaction_locked(false);hero_busy=true;select(0u);
 check(stopped()&&menu::visible(),"Record is blocked during hero changes");
 hero_busy=false;mission_busy=true;select(0u);
 check(stopped()&&menu::visible(),"Record is blocked during mission changes");
 mission_busy=false;test_word(test_game+0x1A8u)=1u;select(0u);
 check(stopped()&&contains(menu::status_text,"Enable Physics"),"Record reports disabled physics instead of silently recording an idle world");
 reset_fixture();begin_record();capture(20);hero_busy=true;read_input();
 check(stopped()&&clip_valid&&stored_samples==1u,"an active hero operation stops capture immediately without unsafe native calls");
 reset_fixture();begin_record();capture(20);menu::set_interaction_locked(true);read_input();
 check(stopped()&&menu::interaction_is_locked(),"recording never unlocks another transaction");

 reset_fixture();begin_record();for(u32 i=0u;i<maximum_samples;++i)capture(static_cast<s8>(i%100u));
 check(stored_samples==3600u&&stopped()&&clip_valid,"fixed recording capacity stops cleanly at all 3600 playable samples");
 const Sample last=samples[maximum_samples-1u];capture(-99);capture(-98);
 check(stored_samples==maximum_samples&&memcmp(&last,&samples[maximum_samples-1u],sizeof(last))==0,"extra input cannot overwrite a full retained recording");
 select(3u);check(stopped()&&stored_samples==0u&&duration_seconds==0.0f&&!clip_valid&&playback_cursor==0u,"Clear resets recording, playback and timing state");
 process_game_pads(nullptr,false);check(stopped()&&stored_samples==0u,"null PAD destination is harmless");
 replay::configure_replay(*replay_menu);check(replay_menu->count==6u,"reconfiguring the menu does not duplicate controls or reset a clip implicitly");
 menu::Menu empty{"Replay",nullptr,nullptr,nullptr,0u,0u,0u,0u};configure_replay(empty);
 check(empty.count==0u,"missing menu storage is handled without invalid entry writes");
 reset_fixture();auto& rec=replay_menu->entries[0];rec.handler(rec,menu::Command::increment);rec.handler(rec,menu::Command::decrement);rec.handler(rec,menu::Command::frame);
 check(stopped()&&stored_samples==0u,"left/right/frame callbacks cannot start a recording");

 char number[16];menu::format_s32(number,sizeof(number),static_cast<s32>(checks));
 print(failures==0u?"PASS: ":"FAILED: ");print(number);print(" Replay recording/playback checks\n");return failures==0u?0:1;
}
asm(".global _start\n_start:\n call run_tests\n mov %eax, %ebx\n mov $1, %eax\n int $0x80\n");
