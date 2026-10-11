// Exercise the production camera bridge against native call boundaries.
#include "../../gamecube/gc_engine.cpp"
#include "../../gamecube/menu_registry.cpp"
using namespace usm::gc;
namespace {
constexpr uptr game_object=0x81000000u,world_object=0x81001000u,devopts=0x81002000u;
constexpr uptr chase=0x81003000u,user=0x81004000u,analyzer=0x81005000u;
constexpr uptr analyzer_flag=0x804626F8u;
u32 checks,failures,camera_calls,analyzer_calls,last_mode;uptr last_owner;
char visible_camera[64];
s32 state=6;
int syscall(u32 n,u32 a,u32 b=0u,u32 c=0u){int r;asm volatile("int $0x80":"=a"(r):"a"(n),"b"(a),"c"(b),"d"(c):"memory");return r;}
void print(const char* s){u32 n=0;while(s[n])++n;syscall(4,1,reinterpret_cast<uptr>(s),n);}
void check(bool ok,const char* s){++checks;if(!ok){++failures;print("FAIL: ");print(s);print("\n");}}
bool equal(const char* a,const char* b){for(u32 i=0;;++i){if(a[i]!=b[i])return false;if(a[i]=='\0')return true;}}
u32& word(uptr p){return *reinterpret_cast<u32*>(p);}
s32 native_state(void*){return state;}
bool native_analyzer(){return word(analyzer_flag)!=0u;}
void enable_analyzer(bool value){++analyzer_calls;word(analyzer_flag)=value?1u:0u;}
void native_camera(void* owner,s32 mode){++camera_calls;last_owner=reinterpret_cast<uptr>(owner);last_mode=static_cast<u32>(mode);word(devopts+0x348u)=static_cast<u32>(mode);if(mode!=2)word(game_object+0x74u)=mode==1?user:chase;}
void boundary(uptr address,uptr target){*reinterpret_cast<u8*>(address)=0xE9u;word(address+1u)=target-address-5u;}
void native_graphic(){}
void native_dimensions(void*,const char* text,u32* width,u32* height,float,float){u32 n=0;while(text[n])++n;*width=n*8u;*height=18u;}
void native_text(void*,const char* text,float,float,float,u32,float,float){
 if(text[0]!='C'||text[1]!='a'||text[2]!='m'||text[3]!='e'||text[4]!='r'||text[5]!='a')return;
 for(u32 i=0;i<sizeof(visible_camera)-1u;++i){visible_camera[i]=text[i];if(text[i]=='\0')return;}
 visible_camera[sizeof(visible_camera)-1u]='\0';
}
void check_visible(const char* expected){visible_camera[0]='\0';menu::render();check(equal(visible_camera,expected),"real menu renderer composes the requested complete Camera : mode text");}
void reset(){
 state=6;camera_calls=0;analyzer_calls=0;word(address::game_pointer)=game_object;
 word(address::world_pointer)=world_object;word(address::developer_options_pointer)=devopts;
 word(game_object+0xD8u)=0x81006000u;word(game_object+0x68u)=world_object;
 word(game_object+0x5Cu)=1;word(game_object+0x1A0u)=1;word(game_object+0x1C8u)=0;
 word(game_object+0x74u)=chase;word(game_object+0x78u)=chase;word(game_object+0x80u)=0x81007000u;
 for(u32 offset=0x24u;offset<=0x40u;offset+=4u)word(world_object+0x28u+offset)=0x81008000u+offset*0x10u;
 word(world_object+0x58u)=user;word(world_object+0x68u)=analyzer;
 word(devopts+0x348u)=0;word(analyzer_flag)=0;
}
// Native handle_cameras8017DD8C..DDD8 corrects CAMERA_STATE from the enable
// word every frame. This catches a selector that changes only the Devopt.
void native_next_frame(){
 const u32 mode=word(devopts+0x348u);
 if(word(game_object+0x1C8u)!=0u&&mode!=1u)native_camera(reinterpret_cast<void*>(game_object),1);
 if(word(game_object+0x1C8u)==0u&&mode!=0u&&!native_analyzer())native_camera(reinterpret_cast<void*>(game_object),0);
}
}
extern "C" int run_tests(){
 u32 mapping[6]={0x80000000u,0x1800000u,7u,0x32u,~0u,0u};
 if(static_cast<uptr>(syscall(90,reinterpret_cast<uptr>(mapping)))!=mapping[0])return 2;
 boundary(address::game_get_state,reinterpret_cast<uptr>(native_state));
 boundary(address::game_set_camera,reinterpret_cast<uptr>(native_camera));
 boundary(address::scene_analyzer_enabled,reinterpret_cast<uptr>(native_analyzer));
 boundary(address::enable_scene_analyzer,reinterpret_cast<uptr>(enable_analyzer));
 boundary(address::ngl_init_quad,reinterpret_cast<uptr>(native_graphic));
 boundary(address::ngl_set_quad_color,reinterpret_cast<uptr>(native_graphic));
 boundary(address::ngl_set_quad_rect,reinterpret_cast<uptr>(native_graphic));
 boundary(address::ngl_set_quad_z,reinterpret_cast<uptr>(native_graphic));
 boundary(address::ngl_add_quad,reinterpret_cast<uptr>(native_graphic));
 boundary(address::ngl_get_string_dimensions,reinterpret_cast<uptr>(native_dimensions));
 boundary(address::ngl_add_string,reinterpret_cast<uptr>(native_text));
 word(address::ngl_system_font_pointer)=0x81010000u;
 reset();check(engine::camera_available(0)&&engine::camera_available(1)&&engine::camera_available(2),"all three native cameras are available in a loaded world");
 engine::set_camera(1);
 check(camera_calls==1&&last_owner==game_object&&last_mode==1,"User selection invokes native game camera setter with correct owner and mode");
 check(word(game_object+0x1C8u)==1u&&word(devopts+0x348u)==1u,"User selection sets the complete native enable word and CAMERA_STATE");
 native_next_frame();check(word(game_object+0x74u)==user&&engine::get_camera()==1,"User camera remains active after native frame correction");
 word(devopts+0x348u)=0;check(engine::get_camera()==1,"camera getter reports requested User enable while native frame catches up");native_next_frame();
 engine::set_camera(2);
 check(word(game_object+0x1C8u)==0u&&word(devopts+0x348u)==2u&&word(analyzer_flag)==1u,"Scene Analyzer disables User and enables the native analyzer flag");
 native_next_frame();check(engine::get_camera()==2&&word(game_object+0x74u)==chase,"Scene Analyzer keeps Chase as base camera and persists across native correction");
 engine::set_camera(0);native_next_frame();
 check(word(game_object+0x1C8u)==0u&&word(devopts+0x348u)==0u&&word(analyzer_flag)==0u&&word(game_object+0x74u)==chase,"Chase selection clears both debug camera enable states");
 reset();word(world_object+0x58u)=0;check(!engine::camera_available(1)&&engine::camera_available(0),"missing User camera blocks only User selection");
 engine::set_camera(1);check(camera_calls==0&&analyzer_calls==0&&word(game_object+0x1C8u)==0,"unavailable camera causes no native call or enable-word write");
 reset();word(world_object+0x64u)=0;check(!engine::camera_available(2)&&engine::camera_available(1),"missing analyzer controller blocks only Scene Analyzer");
 reset();word(game_object+0x1A0u)=0;check(!engine::camera_available(0),"level unload blocks camera selection even when process state is RUNNING");
 reset();word(game_object+0x5Cu)=0;check(!engine::camera_available(1),"incomplete native loader blocks User camera selection");
 reset();word(game_object+0x68u)=world_object+0x100u;check(!engine::camera_available(0),"camera selection requires game and world ownership to match");
 reset();word(address::developer_options_pointer)=0;check(!engine::camera_available(0),"missing native Devopts blocks camera setter dereference");
 reset();state=8;check(!engine::camera_available(0),"transitional game state blocks camera selection");
 reset();check(!engine::camera_available(-1)&&!engine::camera_available(3),"camera selectors reject out-of-range modes");
 reset();auto* root_menu=menu::create("Debug Menu",3);registry::populate_camera(*root_menu);
 check(root_menu->count==1&&equal(root_menu->entries[0].label,"Camera ")&&root_menu->entries[0].submenu==nullptr,"Camera is one root row whose label and renderer separator produce Camera : value");
 auto& entry=root_menu->entries[0];char display[32];
 menu::set_root(*root_menu);input::Frame opening{};opening.toggle_menu=true;menu::update(opening);
 check(entry.type==menu::EntryType::custom&&entry.target==nullptr,"Camera uses a live custom enum instead of a boolean or submenu");
 entry.formatter(entry,display,sizeof(display));check(equal(display,"chase"),"initial Camera value is lowercase chase");
 check_visible("Camera : chase");
 entry.handler(entry,menu::Command::activate);native_next_frame();entry.formatter(entry,display,sizeof(display));
 check(engine::get_camera()==1&&word(game_object+0x74u)==user&&equal(display,"user"),"A cycles chase to persistent User and displays lowercase user");
 check_visible("Camera : user");
 entry.handler(entry,menu::Command::activate);native_next_frame();entry.formatter(entry,display,sizeof(display));
 check(engine::get_camera()==2&&equal(display,"sceneanalyzer"),"A cycles User to Scene Analyzer and displays sceneanalyzer");
 check_visible("Camera : sceneanalyzer");
 entry.handler(entry,menu::Command::activate);native_next_frame();entry.formatter(entry,display,sizeof(display));
 check(engine::get_camera()==0&&word(game_object+0x74u)==chase&&equal(display,"chase"),"A wraps Scene Analyzer back to Chase");
 entry.handler(entry,menu::Command::decrement);native_next_frame();
 check(engine::get_camera()==2,"Left reverses Chase to Scene Analyzer");
 entry.handler(entry,menu::Command::increment);native_next_frame();
 check(engine::get_camera()==0,"Right advances Scene Analyzer to Chase");
 entry.handler(entry,menu::Command::increment);native_next_frame();
 check(engine::get_camera()==1,"Right advances Chase to User");
 entry.handler(entry,menu::Command::decrement);native_next_frame();
 check(engine::get_camera()==0,"Left reverses User to Chase");
 engine::set_camera(2);entry.formatter(entry,display,sizeof(display));
 check(equal(display,"sceneanalyzer"),"formatter reads live native mode changed outside this row");
 const u32 before_frame=camera_calls;entry.handler(entry,menu::Command::frame);
 check(camera_calls==before_frame,"Camera row frame callback never switches native cameras");
 state=8;entry.handler(entry,menu::Command::activate);
 check(camera_calls==before_frame&&engine::get_camera()==2,"unavailable Camera UI action preserves native camera state");
 reset();word(world_object+0x58u)=0;entry.handler(entry,menu::Command::increment);
 check(camera_calls==0&&engine::get_camera()==0,"missing next camera is rejected without advancing the live value");
 char digits[16];u32 length=0,number=checks;do{digits[length++]=static_cast<char>('0'+number%10u);number/=10u;}while(number);
 print(failures==0?"PASS: ":"FAILED: ");while(length){char digit[2]={digits[--length],'\0'};print(digit);}print(" camera checks\n");return failures==0?0:1;
}
asm(".global _start\n_start:\n call run_tests\n mov %eax, %ebx\n mov $1, %eax\n int $0x80\n");
