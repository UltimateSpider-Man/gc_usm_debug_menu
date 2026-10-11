// Exercise the real dynamic-menu/VM bridge against native ABI boundaries.
// Freestanding i386 keeps GC-sized pointers and maps the actual MEM1 window.
#include "../../gamecube/menus/progression_gc.cpp"
using namespace usm::gc;
namespace {
u32 checks, failures, launches, callbacks;
bool world_ready = true, launch_succeeds = true, callback_saw_live;
bool hero_transition, mission_transition, spawn_resources, cleanup_completes, paused;
u32 cleanup_calls, unpause_calls;
uptr launched_instance, launched_executable, launched_argument, lookup_result, callback_record;
u32 looked_up_hash;
constexpr uptr vm = 0x81010000u, thread = 0x81011000u, instance = 0x81012000u;
constexpr uptr object = 0x81013000u, executable = 0x81014000u, functions = 0x81015000u;
constexpr uptr label = 0x81016000u, handler_name = 0x81017000u;
int syscall(u32 n,u32 a,u32 b=0,u32 c=0){int r;asm volatile("int $0x80":"=a"(r):"a"(n),"b"(a),"c"(b),"d"(c):"memory");return r;}
void print(const char* s){u32 n=0;while(s[n])++n;syscall(4,1,reinterpret_cast<uptr>(s),n);}
void check(bool value,const char* message){++checks;if(!value){++failures;print("FAIL: ");print(message);print("\n");}}
void put(uptr p,u32 value){*reinterpret_cast<u32*>(p)=value;}
u32 get(uptr p){return *reinterpret_cast<u32*>(p);}
void text_at(uptr p,const char* s){for(u32 i=0;;++i){*reinterpret_cast<char*>(p+i)=s[i];if(s[i]=='\0')return;}}
bool equal(const char* a,const char* b){for(u32 i=0;;++i){if(a[i]!=b[i])return false;if(a[i]=='\0')return true;}}
u32 hash(const char* s){u32 value=0;for(u32 i=0;s[i];++i){u8 c=static_cast<u8>(s[i]);if(c>='A'&&c<='Z')c+=32;value=value*33u+c;}return value;}
void hash_native(u32* result,const char* s){*result=hash(s);}
void* add_native(void* owner,void* code,const void* args){++launches;launched_instance=reinterpret_cast<uptr>(owner);launched_executable=reinterpret_cast<uptr>(code);launched_argument=args==nullptr?0u:*static_cast<const u32*>(args);return launch_succeeds?reinterpret_cast<void*>(0x81018000u):nullptr;}
const char* string_native(void*,const char* s){return s;}
void* find_native(const u32* query){looked_up_hash=*query;return reinterpret_cast<void*>(lookup_result);}
void callbacks_native(void* owner,u32 reason,void* active_thread){++callbacks;callback_saw_live=owner==reinterpret_cast<void*>(instance)&&reason==0&&active_thread==nullptr&&usm::gc::progression::from_handle(callback_record)!=nullptr;}
void boundary(uptr address,uptr function){auto* code=reinterpret_cast<u8*>(address);code[0]=0xe9;put(address+1u,function-address-5u);}
using Hook = bool (*)(const void*,void*,u32);
void frame(u32 a,u32 b=0,u32 bytes=4){put(vm,a);if(bytes==8)put(vm+4,b);put(vm+0x184,vm+bytes);}
void invoke(Hook hook,u32 a,u32 b=0,u32 bytes=4){frame(a,b,bytes);hook(nullptr,reinterpret_cast<void*>(vm),0);}
u32 float_bits(float value){union {float f;u32 bits;} data{value};return data.bits;}
float stack_float(){return *reinterpret_cast<float*>(vm);}
void add_entry(){invoke(usm_gc_progression_create,label,handler_name,8);}
void rebuild_fixture(menu::Menu& menu){menu.refresh(menu);}
}
namespace usm::gc::engine {
bool interactive_world_ready(){return world_ready;}
s32 game_state(){return paused?7:6;}
bool game_paused(){return paused;}
void pause_game(){paused=true;}
void unpause_game(){paused=false;++unpause_calls;}
bool dismiss_debug_pause_menu(){return false;}
}
namespace usm::gc::hero_select {
bool busy(){return hero_transition;}
}
namespace usm::gc::mission_level {
bool busy(){return mission_transition;}
}
namespace usm::gc::character_spawn {
bool has_resources(){return spawn_resources;}
bool prepare_transition(){++cleanup_calls;if(cleanup_completes)spawn_resources=false;return cleanup_completes;}
}
extern "C" void* memcpy(void* d,const void* s,unsigned int n){auto* o=static_cast<u8*>(d);auto* i=static_cast<const u8*>(s);for(u32 k=0;k<n;++k)o[k]=i[k];return d;}
extern "C" void* memset(void* d,int v,unsigned int n){auto* o=static_cast<u8*>(d);for(u32 k=0;k<n;++k)o[k]=static_cast<u8>(v);return d;}
extern "C" unsigned int strlen(const char* s){u32 n=0;while(s[n])++n;return n;}

extern "C" int run_tests(){
u32 mapping[6]={0x80000000u,0x1800000u,7u,0x32u,~0u,0u};
if(static_cast<uptr>(syscall(90,reinterpret_cast<uptr>(mapping)))!=mapping[0]){print("Cannot map MEM1 fixture\n");return 2;}
using namespace usm::gc::progression;
boundary(hash_setter,reinterpret_cast<uptr>(hash_native));
boundary(add_thread_with_parameters,reinterpret_cast<uptr>(add_native));
boundary(install_temp_string,reinterpret_cast<uptr>(string_native));
boundary(find_function_by_name,reinterpret_cast<uptr>(find_native));
boundary(run_instance_callbacks,reinterpret_cast<uptr>(callbacks_native));
put(vm+0x180,vm);put(vm+0x188,thread);put(thread+0xc,instance);put(thread+0x10,executable);
put(instance+0xc,123);put(instance+0x2c,object);put(object+0x20,functions);put(object+0x24,1);put(object+8,instance);
put(functions,executable);put(executable,object);put(executable+4,hash("advance_progression()"));
put(executable+0xc,0);put(executable+0x10,0x81019002u);put(executable+0x1c,2);
text_at(label,"Advance progression");text_at(handler_name,"advance_progression()");
auto* menu=menu::create("Progression",recommended_capacity);configure(*menu);
check(menu->count==1&&menu->entries[0].handler!=nullptr,"empty Progression exposes native script setup action");
add_entry();check(get(vm+0x184)==vm+4&&get(vm)==0,"creator consumes two strings and returns native zero result");
rebuild_fixture(*menu);check(menu->count==1&&equal(menu->entries[0].label,"Advance progression"),"VM creator produces a real menu action");
const uptr handle=menu->entries[0].id;
add_entry();rebuild_fixture(*menu);check(menu->count==1,"repeated script setup does not duplicate identical entries");
menu->entries[0].handler(menu->entries[0],menu::Command::activate);
check(launches==1&&launched_instance==instance&&launched_executable==executable&&launched_argument==0,"zero-argument action launches owning VM instance and executable");
check(handler_live(*from_handle(handle)),"two-byte-aligned bytecode is valid");
put(executable+0xc,4);menu->entries[0].handler(menu->entries[0],menu::Command::activate);
check(launches==2&&launched_argument==handle,"entry-argument handler receives stable managed entry handle");
hero_transition=true;menu->entries[0].handler(menu->entries[0],menu::Command::activate);
check(launches==2&&cleanup_calls==0,"hero transition blocks authored Progression handler without starting cleanup");hero_transition=false;
mission_transition=true;menu->entries[0].handler(menu->entries[0],menu::Command::activate);
check(launches==2&&cleanup_calls==0,"mission transition blocks authored Progression handler");mission_transition=false;
spawn_resources=true;menu::set_root(*menu);input::Frame opening{};opening.toggle_menu=true;menu::update(opening);menu::dispatch_deferred();
check(menu::visible()&&paused,"cleanup regression starts from a real menu-owned gameplay pause");
menu->entries[0].handler(menu->entries[0],menu::Command::activate);
check(launches==2&&cleanup_calls==1,"owned character resources queue cleanup before any authored handler launch");
check(!menu::visible()&&menu::owns_input(),"cleanup closes Progression and retains input until deferred unpause");
menu::dispatch_deferred();check(!paused&&!menu::owns_input()&&unpause_calls==1,"deferred unpause lets character cleanup advance safely");
menu->entries[0].handler(menu->entries[0],menu::Command::activate);
check(launches==2&&cleanup_calls==2,"reselection during character cleanup cannot start a script");
cleanup_completes=true;menu->entries[0].handler(menu->entries[0],menu::Command::activate);
check(launches==2&&!spawn_resources&&cleanup_calls==3,"even immediately completed cleanup requires a new explicit selection");
menu->entries[0].handler(menu->entries[0],menu::Command::activate);
check(launches==3&&cleanup_calls==3,"fresh selection after cleanup launches the authored handler");
invoke(usm_gc_progression_get_name,handle);check(equal(reinterpret_cast<const char*>(get(vm)),"Advance progression")&&get(vm+0x184)==vm+4,"get_name returns entry text with native VM result shape");
invoke(usm_gc_progression_get_handler,handle);check(equal(reinterpret_cast<const char*>(get(vm)),"advance_progression()"),"get_handler returns the bound signature");
text_at(label,"Renamed progression");invoke(usm_gc_progression_set_name,handle,label,8);rebuild_fixture(*menu);
check(equal(menu->entries[0].label,"Renamed progression")&&get(vm+0x184)==vm,"set_name updates visible entry and consumes both operands");
struct Pair {Hook setter;Hook getter;float value;};
const Pair properties[]={
 {usm_gc_progression_set_id,usm_gc_progression_get_id,37.0f},
 {usm_gc_progression_set_value,usm_gc_progression_get_value,0.5f},
 {usm_gc_progression_set_value_type,usm_gc_progression_get_value_type,2.0f},
 {usm_gc_progression_set_min_value,usm_gc_progression_get_min_value,-2.0f},
 {usm_gc_progression_set_max_value,usm_gc_progression_get_max_value,2.0f},
 {usm_gc_progression_set_step_size,usm_gc_progression_get_step_size,0.5f},
 {usm_gc_progression_set_step_scale,usm_gc_progression_get_step_scale,20.0f}
};
for(const Pair& pair:properties){invoke(pair.setter,handle,float_bits(pair.value),8);const bool consumed=get(vm+0x184)==vm;invoke(pair.getter,handle);check(consumed&&stack_float()==pair.value&&get(vm+0x184)==vm+4,"numeric entry property round-trip preserves VM float ABI");}
menu->entries[0].handler(menu->entries[0],menu::Command::increment);invoke(usm_gc_progression_get_value,handle);check(stack_float()==1.0f,"numeric entry applies script-defined step");
for(u32 i=0;i<10;++i)menu->entries[0].handler(menu->entries[0],menu::Command::increment);
invoke(usm_gc_progression_get_value,handle);check(stack_float()==2.0f,"numeric entry clamps at script-defined maximum");
invoke(usm_gc_progression_set_value,handle,0x7fc00000u,8);invoke(usm_gc_progression_get_value,handle);check(stack_float()==2.0f,"NaN cannot corrupt a menu value");
invoke(usm_gc_progression_set_step_size,handle,float_bits(-1.0f),8);invoke(usm_gc_progression_get_step_size,handle);check(stack_float()==0.5f,"negative step size is rejected");
invoke(usm_gc_progression_set_value_type,handle,float_bits(2.5f),8);invoke(usm_gc_progression_get_value_type,handle);check(stack_float()==2.0f,"fractional entry type is rejected");
put(executable+0xc,8);const u32 before_bad_signature=launches;menu->entries[0].handler(menu->entries[0],menu::Command::activate);
check(launches==before_bad_signature,"handler with unknown parameter layout does not run");put(executable+0xc,4);
world_ready=false;menu->entries[0].handler(menu->entries[0],menu::Command::activate);check(launches==before_bad_signature,"transitioning world blocks script activation");world_ready=true;
put(executable+0x10,0x81900000u);menu->entries[0].handler(menu->entries[0],menu::Command::activate);check(launches==before_bad_signature,"unmapped bytecode cannot be launched");put(executable+0x10,0x81019002u);
put(functions,executable+0x100u);menu->entries[0].handler(menu->entries[0],menu::Command::activate);check(launches==before_bad_signature,"replaced executable invalidates an old binding");put(functions,executable);
text_at(handler_name,"missing_handler()");add_entry();rebuild_fixture(*menu);char formatted[80];menu->entries[1].formatter(menu->entries[1],formatted,sizeof(formatted));
check(menu->count==2&&equal(formatted,"handler not found"),"missing handler remains visible with an accurate diagnosis");
menu->entries[1].handler(menu->entries[1],menu::Command::activate);check(launches==before_bad_signature,"missing handler does not launch another function");
text_at(handler_name,"advance_progression()");invoke(usm_gc_progression_set_handler,menu->entries[1].id,handler_name,8);
menu->entries[1].handler(menu->entries[1],menu::Command::activate);check(launches==before_bad_signature+1,"set_handler binds and executes a real function");
put(vm+0x184,vm+4);usm_gc_progression_create(nullptr,reinterpret_cast<void*>(vm),0);rebuild_fixture(*menu);
check(menu->count==2&&get(vm+0x184)==vm+4,"creator rejects stack underflow without touching stack state");
invoke(usm_gc_progression_set_value,0x8101f000u,float_bits(1.0f),8);check(get(vm+0x184)==vm,"foreign entry handle is safely consumed without a write");
const menu::Entry stale=menu->entries[0];callback_record=handle;
usm_gc_progression_instance_callbacks(reinterpret_cast<void*>(instance),0,nullptr);
check(callbacks==1&&callback_saw_live,"native destruction callbacks execute before managed entry invalidation");
rebuild_fixture(*menu);check(from_handle(handle)==nullptr&&menu->count==1&&menu->entries[0].target==nullptr,"destroyed owner removes all its Progression entries");
text_at(label,"Replacement entry");add_entry();rebuild_fixture(*menu);const u32 before_stale=launches;menu::Entry stale_copy=stale;stale_copy.handler(stale_copy,menu::Command::activate);
check(launches==before_stale&&menu->entries[0].id!=stale.id,"queued stale entry cannot target a reused record slot");
invoke(usm_gc_progression_get_name,handle);check(equal(reinterpret_cast<const char*>(get(vm)),""),"retained stale VM handle cannot read a reused record slot");
invoke(usm_gc_progression_set_value,handle,float_bits(1.0f),8);invoke(usm_gc_progression_get_value,menu->entries[0].id);check(stack_float()==0.0f,"retained stale VM handle cannot write a replacement entry");
invoke(usm_gc_progression_destroy,menu->entries[0].id);rebuild_fixture(*menu);check(menu->entries[0].target==nullptr&&get(vm+0x184)==vm,"destroy entry consumes handle and removes the row");
for(u32 index=0;index<recommended_capacity;++index){char value[16]="Step000";value[4]=static_cast<char>('0'+index/100u);value[5]=static_cast<char>('0'+index/10u%10u);value[6]=static_cast<char>('0'+index%10u);text_at(label,value);add_entry();}
rebuild_fixture(*menu);check(menu->count==recommended_capacity&&equal(menu->entries[511].label,"Step511"),"all512 dynamic records remain reachable without truncation");
const u32 first_serial=menu->entries[0].id;text_at(label,"Overflow");add_entry();rebuild_fixture(*menu);check(menu->count==recommended_capacity&&menu->entries[0].id==first_serial,"pool exhaustion does not overwrite live entries");
invalidate_instance(reinterpret_cast<void*>(instance));rebuild_fixture(*menu);
put(exec_map_slot,0x8101a000u);lookup_result=executable;put(executable+4,hash("setup_progression_debug_menu()"));put(executable+0xc,0);
const u32 before_setup=launches;
hero_transition=true;menu->entries[0].handler(menu->entries[0],menu::Command::activate);
check(launches==before_setup,"hero transition also blocks native Progression setup fallback");hero_transition=false;
mission_transition=true;menu->entries[0].handler(menu->entries[0],menu::Command::activate);
check(launches==before_setup,"mission transition also blocks Progression setup fallback");mission_transition=false;
spawn_resources=true;cleanup_completes=false;const u32 before_setup_cleanup=cleanup_calls;
menu->entries[0].handler(menu->entries[0],menu::Command::activate);
check(launches==before_setup&&cleanup_calls==before_setup_cleanup+1,"owned character packs block fallback and queue their cleanup");
cleanup_completes=true;menu->entries[0].handler(menu->entries[0],menu::Command::activate);
check(launches==before_setup&&!spawn_resources,"fallback requires a fresh selection after cleanup completes");
menu->entries[0].handler(menu->entries[0],menu::Command::activate);
check(launches==before_setup+1&&looked_up_hash==hash("setup_progression_debug_menu()")&&launched_argument==0,"fallback runs the actual native Progression setup function");
put(object+8,0);put(object+0x2c,0x8101b000u);put(0x8101b000u,instance);menu->entries[0].handler(menu->entries[0],menu::Command::activate);
check(launches==before_setup+2&&launched_instance==instance,"setup fallback locates a live non-global script instance");
lookup_result=0;menu->entries[0].handler(menu->entries[0],menu::Command::activate);check(launches==before_setup+2,"absent setup function does not fabricate progression flags");
// Script objects/strings/bytecode can be owned ARAM-paged pack resources.
u32 paged_mapping[6]={0xF0200000u,0x30000u,7u,0x32u,~0u,0u};
if(static_cast<uptr>(syscall(90,reinterpret_cast<uptr>(paged_mapping)))!=paged_mapping[0]){print("Cannot map paged-resource fixture\n");return 2;}
constexpr uptr manager=0x81100000u,partitions=0x81100100u,partition=0x81100200u;
constexpr uptr slots=0x81100300u,slot=0x81100400u,directory=0x81100500u;
constexpr uptr paged_object=0xF0201000u,paged_executable=0xF0202000u,paged_functions=0xF0203000u;
constexpr uptr paged_label=0xF0204000u,paged_handler=0xF0205000u,paged_bytecode=0xF0206002u;
put(0x8045F214u,manager);put(manager,partitions);put(partitions,partition);
put(partition+8u,slots);put(partition+12u,slots+4u);put(slots,slot);put(slot+8u,4u);put(slot+0x18u,directory);
put(directory+0x78u,slot);put(directory+0x80u,0xF0200000u);put(directory+0x84u,0x7000u);put(directory+0x88u,0x20000u);
put(thread+0x10u,paged_executable);put(instance+0x2Cu,paged_object);put(paged_object+8u,instance);
put(paged_object+0x20u,paged_functions);put(paged_object+0x24u,1u);put(paged_functions,paged_executable);
put(paged_executable,paged_object);put(paged_executable+4u,hash("advance_progression()"));
put(paged_executable+0xCu,0u);put(paged_executable+0x10u,paged_bytecode);put(paged_executable+0x1Cu,2u);
text_at(paged_label,"Paged progression");text_at(paged_handler,"advance_progression()");
invoke(usm_gc_progression_create,paged_label,paged_handler,8);rebuild_fixture(*menu);
check(menu->count==1u&&menu->entries[0].target!=nullptr&&equal(menu->entries[0].label,"Paged progression"),"owned paged strings, object and function array create an authored row");
const u32 before_paged=launches;menu->entries[0].handler(menu->entries[0],menu::Command::activate);
check(launches==before_paged+1u&&launched_executable==paged_executable,"owned paged executable and two-byte-aligned bytecode launch natively");
uptr bounded_args=0u;
put(0xF0208000u+0x180u,0xF0208000u);put(0xF0208000u+0x184u,0xF0208004u);
check(!arguments(reinterpret_cast<void*>(0xF0208000u),4u,bounded_args),"owned paged memory never authorizes a writable VM stack");
uptr context_instance=0u,context_object=0u;put(thread+0xCu,0xF0209000u);
check(!context(reinterpret_cast<void*>(vm),context_instance,context_object),"owned paged memory never authorizes mutable VM instances");put(thread+0xCu,instance);
invalidate_instance(reinterpret_cast<void*>(instance));rebuild_fixture(*menu);
lookup_result=paged_executable;put(paged_executable+4u,hash("setup_progression_debug_menu()"));
menu->entries[0].handler(menu->entries[0],menu::Command::activate);
check(launches==before_paged+2u&&launched_executable==paged_executable,"native lookup can start Progression setup from an owned paged executable");
char near_end[64]{};text_at(0xF021FFFEu,"A");
check(copy_string(near_end,sizeof(near_end),0xF021FFFEu)&&equal(near_end,"A"),"paged string can terminate at the final byte of its owned extent");
text_at(0xF021FFFEu,"AB");
check(!copy_string(near_end,sizeof(near_end),0xF021FFFEu,true),"paged string does not read beyond its owned extent");
text_at(0xF0220100u,"Unowned");
check(!copy_string(near_end,sizeof(near_end),0xF0220100u),"mapped but unowned F0 memory is rejected");
put(paged_object+0x20u,0xF021FFFCu);put(paged_object+0x24u,2u);uptr checked_functions=0u;u32 checked_count=0u;
check(!function_array(paged_object,checked_functions,checked_count),"paged function array cannot cross its ownership boundary");
put(paged_object+0x20u,paged_functions);put(paged_object+0x24u,1u);put(paged_executable+4u,hash("advance_progression()"));
invoke(usm_gc_progression_create,paged_label,paged_handler,8);rebuild_fixture(*menu);
put(slot+8u,3u);const u32 before_unload=launches;menu->entries[0].handler(menu->entries[0],menu::Command::activate);
check(launches==before_unload,"cached paged owner invalidates immediately when native slot leaves READY state");
rebuild_fixture(*menu);check(menu->entries[0].target==nullptr,"unloaded paged owner removes its authored row");
put(slot+8u,4u);invoke(usm_gc_progression_create,paged_label,paged_handler,8);rebuild_fixture(*menu);
put(partition+12u,slots);menu->entries[0].handler(menu->entries[0],menu::Command::activate);
check(launches==before_unload,"removed pack-slot membership invalidates a cached paged range");
char digits[16];u32 length=0,number=checks;do{digits[length++]=static_cast<char>('0'+number%10u);number/=10u;}while(number);
print(failures==0?"PASS: ":"FAILED: ");while(length){char digit[2]={digits[--length],'\0'};print(digit);}print(" Progression behavior checks\n");return failures==0?0:1;
}
asm(".global _start\n_start:\n call run_tests\n mov %eax, %ebx\n mov $1, %eax\n int $0x80\n");
