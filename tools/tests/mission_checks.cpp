// Freestanding checks of the production mission catalogue and deferred native
// transaction. Mapped MEM1 data and tiny native-call stubs exercise ownership,
// all-hero policy, authored variants, cross-hero stages and scene persistence.
#include "../../gamecube/menus/mission_level_gc.cpp"
using namespace usm::gc;
namespace {
u32 failures, checks;
constexpr uptr fixture_world=0x81000000u,fixture_hero=0x81001000u,controller=0x81002000u,
 manager=0x81003000u,table=0x81004000u,condition=0x81005000u,instance=0x81006000u,
 region=0x81007000u,text=0x81008000u,resource=0x81009000u,partition=0x81009200u,
 directory=0x81009500u,fixture_game=0x8100A000u,stack=0x8100B000u,descriptors=0x8100C000u;
u32 unload_calls,force_calls,camera_calls,load_calls,persist_calls,directory_calls,reboot_calls,
 scheduled_reboot_calls,close_calls;
s32 state=6,descriptor_count=2; bool character_resources,cleanup_ready=true;u32 cleanup_calls; bool paused,hero_busy,pack_ready=true,character_pack=true,persist_ok=true,locked;
s32 forced_district,forced_instance;bool forced_has_data;char forced_name[32],forced_data[32],loaded_name[32],persisted_name[32],last_hash[64];
u32 sequence,persist_sequence,load_sequence;
bool paged_descriptors,city_only;
constexpr uptr paged_base=0xF0000000u,paged_levels=paged_base+0x1000u,
 paged_table=paged_base+0x2000u,paged_conditions=paged_base+0x3000u,
 paged_instances=paged_base+0x4000u,paged_text=paged_base+0x6000u;
int call(u32 number,u32 first,u32 second=0,u32 third=0){int result;asm volatile("int $0x80":"=a"(result):"a"(number),"b"(first),"c"(second),"d"(third):"memory");return result;}
void out(const char* s){u32 n=0;while(s[n])++n;call(4,1,reinterpret_cast<uptr>(s),n);}
void check(bool ok,const char* name){++checks;if(!ok){++failures;out("FAIL ");out(name);out("\n");}}
void put(uptr b,u32 o,u32 v){memory<u32>(b+o)=v;}
void text_at(uptr destination,const char* source){u32 i=0;do{memory<char>(destination+i)=source[i];}while(source[i++]!='\0');}
void stub(uptr address,uptr function){auto* p=reinterpret_cast<u8*>(address);p[0]=0xB8;*reinterpret_cast<u32*>(p+1)=function;p[5]=0xFF;p[6]=0xE0;}
menu::Menu menu_store[12];menu::Entry entry_store[12][1025];u32 menu_used;
}
namespace usm::gc::menu {
void copy_text(char* out,u32 cap,const char* in){if(cap==0)return;u32 n=0;if(in)while(n+1<cap&&in[n]){out[n]=in[n];++n;}out[n]=0;}
void append_text(char* out,u32 cap,const char* in){u32 n=0;while(n<cap&&out[n])++n;if(n<cap)copy_text(out+n,cap-n,in);}
void format_s32(char* out,u32 cap,s32 v){char b[16];u32 n=0;u32 x=v<0?0u-static_cast<u32>(v):static_cast<u32>(v);do{b[n++]=static_cast<char>('0'+x%10);x/=10;}while(x);u32 k=0;if(v<0&&k+1<cap)out[k++]='-';while(n&&k+1<cap)out[k++]=b[--n];if(cap)out[k]=0;}
Menu* create(const char* title,u16 capacity,Refresh refresh){if(menu_used>=12||capacity>1025)return nullptr;u32 i=menu_used++;Menu& m=menu_store[i];m={};m.title=title;m.capacity=capacity;m.entries=entry_store[i];m.refresh=refresh;return &m;}
Entry* add(Menu& m,const char* label,EntryType type){if(m.count>=m.capacity)return nullptr;Entry& e=m.entries[m.count++];e={};e.label=label;e.type=type;return &e;}
Entry* add_submenu(Menu& m,Menu& child){Entry* e=add(m,child.title,EntryType::submenu);if(e){e->submenu=&child;if(!child.parent)child.parent=&m;}return e;}
void clear_entries(Menu& m){m.count=0;m.selected=0;m.scroll=0;}
void close(){++close_calls;}
void set_interaction_locked(bool value){locked=value;}
void set_status(const char*,u16){}
}
namespace usm::gc::engine {
void* world(){return reinterpret_cast<void*>(fixture_world);}
void* game(){return reinterpret_cast<void*>(fixture_game);}
bool gameplay_rel_ready(){return state==6||state==7;}
s32 game_state(){return state;}
bool game_paused(){return paused;}
const char* region_name(void*){return reinterpret_cast<const char*>(region+0x200);}
void reboot_game(){++reboot_calls;}
}
namespace usm::gc::hero_select {bool busy(){return hero_busy;}}
namespace usm::gc::character_spawn {bool has_resources(){return character_resources;}bool prepare_transition(){++cleanup_calls;if(cleanup_ready)character_resources=false;return !character_resources;}}
namespace usm::gc::scene_config {
bool select_scene(const char* name){++persist_calls;persist_sequence=++sequence;menu::copy_text(persisted_name,32,name);return persist_ok;}
bool select_scene_and_reboot(const char* name){if(!select_scene(name))return false;++scheduled_reboot_calls;return true;}
void format_persistence(char* out,u32 cap){menu::copy_text(out,cap,"Saved");}
}
extern "C" void* memcpy(void* d,const void* s,unsigned int n){auto* o=static_cast<u8*>(d);auto* i=static_cast<const u8*>(s);for(u32 k=0;k<n;++k)o[k]=i[k];return d;}
extern "C" void* memset(void* d,int v,unsigned int n){auto* p=static_cast<u8*>(d);for(u32 i=0;i<n;++i)p[i]=static_cast<u8>(v);return d;}
extern "C" unsigned int strlen(const char* s){u32 n=0;while(s[n])++n;return n;}
extern "C" void native_hash(u32* hash,const char* name){*hash=1;menu::copy_text(last_hash,64,name);}
extern "C" bool native_pack(const void*,void*,void*,void*){return pack_ready&&(!city_only||mission_level::equal_text(last_hash,"city_arena"))&&(character_pack||!mission_level::equal_text(last_hash,"characterb_arena"));}
extern "C" void native_unload(void*){++unload_calls;if(memory<u32>(manager+0x40)){put(manager,0x4c,1);put(manager,0x7c,0);}}
extern "C" bool native_wait(void*){return false;}
extern "C" void native_camera(void*,bool enabled,bool,float,float){++camera_calls;put(fixture_world,0x70,enabled?1u:0u);}
extern "C" void native_force(void*,s32 district,const char* name,s32 index,const char* data){++force_calls;forced_district=district;forced_instance=index;forced_has_data=data!=nullptr;menu::copy_text(forced_name,32,name);menu::copy_text(forced_data,32,data);}
extern "C" void* native_partition(s32){return reinterpret_cast<void*>(partition);}
extern "C" void* native_directory(void*,const void* key,s32* size,void**){
 ++directory_calls;
 if(!mission_level::equal_text(last_hash,"level")||static_cast<const u32*>(key)[1]!=9u){*size=0;return nullptr;}
 *size=descriptor_count*0x90;return reinterpret_cast<void*>(paged_descriptors?paged_levels:descriptors);
}
extern "C" void native_load(void*,engine::MString* name){++load_calls;load_sequence=++sequence;menu::copy_text(loaded_name,32,name->guts);}
extern "C" int run_tests(){
 u32 mapping[6]={0x80000000u,0x1800000u,7u,0x32u,~0u,0u};
 if(static_cast<uptr>(call(90,reinterpret_cast<uptr>(mapping)))!=mapping[0])return 2;
 u32 paged_mapping[6]={paged_base,0x10000u,7u,0x32u,~0u,0u};
 if(static_cast<uptr>(call(90,reinterpret_cast<uptr>(paged_mapping)))!=paged_base)return 2;
 using namespace usm::gc::mission_level;
 stub(string_hash_set,reinterpret_cast<uptr>(native_hash));stub(resource_get_pack_stats,reinterpret_cast<uptr>(native_pack));
 stub(mission_prepare_unload,reinterpret_cast<uptr>(native_unload));stub(mission_stack_waiting,reinterpret_cast<uptr>(native_wait));
 stub(game_enable_marky_camera,reinterpret_cast<uptr>(native_camera));stub(mission_force,reinterpret_cast<uptr>(native_force));
 stub(resource_get_partition,reinterpret_cast<uptr>(native_partition));stub(resource_directory_get,reinterpret_cast<uptr>(native_directory));stub(game_load_new_level,reinterpret_cast<uptr>(native_load));
 put(fixture_world,0x21c,fixture_hero);put(fixture_world,0x224,1);put(fixture_hero,0x8c,controller);put(controller,0x420,1);
 put(mission_manager_pointer,0,manager);put(mission_stack_pointer,0,stack);put(resource_manager_pointer,0,resource);put(resource,0,resource+0x100);
 put(partition,8,partition+0x100);put(partition+0x100,0,partition+0x200);put(partition+0x200,0x18,directory);
 put(resource+0x100,0,partition);put(partition,0xc,partition+0x104);put(partition+0x200,8,4);
 put(directory,0x78,partition+0x200);put(directory,0x80,paged_base);put(directory,0x84,0x20000);put(directory,0x88,0x10000);
 check(resource_memory::readable_object(reinterpret_cast<void*>(paged_levels),0x120),"READY slot owned paged byte count authorizes read-only data");
 check(!resource_memory::readable_range(reinterpret_cast<void*>(paged_base+0xfff0),32),"read-only data cannot cross paged pool end");
 check(!resource_memory::readable_range(reinterpret_cast<void*>(paged_base+0x10000),1),"unowned address beyond paged extent is rejected");
 put(partition+0x200,8,3);check(!resource_memory::readable_range(reinterpret_cast<void*>(paged_levels),1),"cached paged pool is rejected while slot unloads");put(partition+0x200,8,4);
 put(directory,0x78,0);check(!resource_memory::readable_range(reinterpret_cast<void*>(paged_levels),1),"foreign directory owner invalidates cached pool");put(directory,0x78,partition+0x200);
 put(partition,0xc,partition+0x100);check(!resource_memory::readable_range(reinterpret_cast<void*>(paged_levels),1),"removed live slot invalidates cached paged pool");put(partition,0xc,partition+0x104);
 put(directory,0x80,paged_base+0x8000);put(directory,0x88,0x8000);check(!resource_memory::readable_range(reinterpret_cast<void*>(paged_levels),1),"relocated owned pool invalidates cached address");put(directory,0x80,paged_base);put(directory,0x88,0x10000);
 check(!readable_object(reinterpret_cast<void*>(paged_table),0x48),"mutable-object validation remains restricted to MEM1");
 put(fixture_game,0x1a0,1);put(fixture_game,0x5c,1);put(fixture_game,0x68,fixture_world);put(fixture_game,0x74,fixture_game+0x400);put(fixture_world,0x6c,fixture_world+0xd000);put(fixture_world,0x220,fixture_world+0xf000);
 put(fixture_world,0x3e0,18);put(fixture_world,0x3e4,text+0x800);text_at(text+0x800,"ultimate_spiderman");
 put(manager,0xf0,~0u);put(manager,0x14,table);put(table,0x38,condition);memory<u16>(table+0x3c)=1;
 put(condition,8,instance);memory<u16>(condition+0xc)=1;put(condition,0x28,0x8000);
 MissionRecord rec{};rec.source_table=reinterpret_cast<void*>(table);rec.source_condition=reinterpret_cast<void*>(condition);rec.source_instance=reinterpret_cast<void*>(instance);
 mission_cache_manager=reinterpret_cast<void*>(manager);
 check(mission_record_is_current(reinterpret_cast<void*>(manager),rec),"live global mission");
 check(!mission_record_is_current(reinterpret_cast<void*>(manager+0x100),rec),"manager identity rejected");
 put(manager,0x14,0);check(!mission_record_is_current(reinterpret_cast<void*>(manager),rec),"removed table rejected");put(manager,0x14,table);
 put(table,0x38,condition+0x100);check(!mission_record_is_current(reinterpret_cast<void*>(manager),rec),"replaced condition allocation rejected");put(table,0x38,condition);
 memory<u16>(table+0x3c)=513;check(!mission_record_is_current(reinterpret_cast<void*>(manager),rec),"oversized condition count rejected");memory<u16>(table+0x3c)=1;
 put(condition,8,instance+0x100);check(!mission_record_is_current(reinterpret_cast<void*>(manager),rec),"replaced instance allocation rejected");put(condition,8,instance);
 memory<u16>(condition+0xc)=0;check(!mission_record_is_current(reinterpret_cast<void*>(manager),rec),"deleted instance rejected");memory<u16>(condition+0xc)=1;
 put(controller,0x420,2);check(mission_record_is_current(reinterpret_cast<void*>(manager),rec),"other hero authored mission remains selectable");put(controller,0x420,1);
 rec.district=1;check(!mission_record_is_current(reinterpret_cast<void*>(manager),rec),"global district mismatch rejected");
 put(manager,0x14,0);put(manager,0x38,1);put(manager,0x18,table);put(table,0x44,region);put(region,0xc0,1);
 check(mission_record_is_current(reinterpret_cast<void*>(manager),rec),"live district mission");
 put(region,0xc0,2);check(!mission_record_is_current(reinterpret_cast<void*>(manager),rec),"changed district rejected");put(region,0xc0,1);
 check(!bounded_string(nullptr,32),"null name rejected");check(!bounded_string(reinterpret_cast<char*>(0x81800000u),32),"out of MEM1 name rejected");
 auto* text_buffer=reinterpret_cast<char*>(text);for(u32 i=0;i<31;++i)text_buffer[i]='a';text_buffer[31]=0;check(bounded_string(text_buffer,32),"31 byte native name accepted");text_buffer[31]='a';text_buffer[32]=0;check(!bounded_string(text_buffer,32),"32 byte native name rejected");
 text_at(text,"x");check(same_bounded_text(text_buffer,"x",32),"snapshot match accepted");check(!same_bounded_text(text_buffer,"y",32),"changed name rejected");
 put(condition,0x18,text);u32 before=mission_table_signature(reinterpret_cast<void*>(table));put(instance,0x54,8);check(before!=mission_table_signature(reinterpret_cast<void*>(table)),"instance flag invalidates cache");
 const u32 flagsig=mission_table_signature(reinterpret_cast<void*>(table));put(instance,0x18,text);check(flagsig!=mission_table_signature(reinterpret_cast<void*>(table)),"instance data invalidates cache");
 check(requested_hero("PK_V02_WOLVERINE_IGC.MIS",0)==Hero::venom,"Venom family normalizes pack prefix and extension");
 check(requested_hero("dir\\S13_FINALE.mis",0x4000)==Hero::spiderman,"authored Spider-Man family overrides hero mask");
 check(requested_hero("v070",0)==Hero::current,"family token requires boundary");
 check(requested_hero("race",0x4000)==Hero::venom&&requested_hero("race",0x8000)==Hero::spiderman,"exclusive authored hero masks classify missions");
 check(requested_hero("race",0xc000)==Hero::current&&requested_hero("race",0x10000)==Hero::current,"shared and Parker flags retain current hero");
 check(cinematic_text("v09_IGC1")&&cinematic_text("cut_scene_intro")&&cinematic_text("CINEMATIC"),"authored cinematic token boundaries accepted");
 check(!cinematic_text("bigcity")&&!cinematic_text("cutscenery"),"incidental cinematic substrings ignored");
 // Both heroes are catalogued; authored variants survive with their exact IDs.
 put(manager,0x14,table);put(manager,0x38,0);text_at(text,"v09_test");put(condition,0x28,0x4000);memory<u16>(condition+0xc)=2;
 text_at(text+0x100,"IGC_intro");put(instance,0x18,text+0x100);put(instance,0x54,8);put(instance+0x68,0x54,0);
 mission_record_count=0;mission_catalogue_truncated=false;mission_cache_generation=7;
 check(add_mission_table(reinterpret_cast<void*>(table),0,0)&&mission_record_count==2,"catalogue includes Venom variants while Spider-Man is active");
 check(mission_records[0].target==Hero::venom&&mission_records[0].cinematic&&mission_records[1].instance==1&&!mission_records[1].cinematic,"authored data and instance numbering retained");
 add_mission_table(reinterpret_cast<void*>(table),0,1);check(mission_record_count==2,"duplicate district script instance data records excluded");
 auto* root=menu::create("Missions",8);attach_missions(*root);
 check(root->count==5&&mission_groups[0]->count==1&&mission_groups[3]->count==1,"Missions exposes four groups and cinematic links");
 mission_leaf->parent=mission_groups[0];mission_groups[0]->selected=0;refresh_mission_folder(*mission_leaf);
 check(mission_leaf->count==2&&equal_text(mission_leaf->title,"global"),"district folder lists all authored variants");
 mission_leaf->selected=1;refresh_mission_folder(*mission_leaf);check(mission_leaf->selected==1,"unchanged refresh preserves mission selection");
 check(transition_cameras_ready(reinterpret_cast<void*>(fixture_game),reinterpret_cast<void*>(fixture_world)),"loader-completed word1 and native game+74 camera permit hero transition");
 put(fixture_game,0x74,0);check(!transition_cameras_ready(reinterpret_cast<void*>(fixture_game),reinterpret_cast<void*>(fixture_world)),"missing native sync camera rejects hero transition");put(fixture_game,0x74,fixture_game+0x400);
 put(fixture_game,0x5c,0);check(!transition_cameras_ready(reinterpret_cast<void*>(fixture_game),reinterpret_cast<void*>(fixture_world)),"incomplete native loader rejects hero transition");put(fixture_game,0x5c,1);
 put(fixture_game,0x68,fixture_world+0x100);check(!transition_cameras_ready(reinterpret_cast<void*>(fixture_game),reinterpret_cast<void*>(fixture_world)),"foreign game world rejects camera transition");put(fixture_game,0x68,fixture_world);
 // Real staged operation: unload must finish before native hero switch, and
 // the original authored data is copied before source tables disappear.
 rec=mission_records[0];put(manager,0x40,condition);put(manager,0xf0,~0u);
 check(request_mission(rec,reinterpret_cast<void*>(manager))&&locked,"cross-hero request queues and owns menu interaction");
 frame_advance();check(unload_calls==1&&memory<u32>(manager+0xcc)==1&&launch.phase==LaunchPhase::unloading,"first running tick locks automatic selection and unloads");
 frame_advance();check(camera_calls==0&&force_calls==0,"live script prevents hero removal and mission force");
 put(manager,0x40,0);put(manager,0x4c,0);put(stack,0,1);frame_advance();check(camera_calls==0,"pending pack operations block hero switch");put(stack,0,0);
 frame_advance();check(launch.phase==LaunchPhase::switching&&memory<u32>(manager+0xf0)==0&&equal_text(reinterpret_cast<char*>(manager+0xd0),"venom"),"native switch gets verified fixed hero name and stage zero");
 check(camera_calls==1&&force_calls==0,"Marky camera precedes cross-hero replacement");
 put(fixture_world,0x224,0);frame_advance();check(force_calls==0,"native zero-player switch interval is preserved");
 put(fixture_world,0x224,1);put(fixture_world,0x3e0,5);text_at(text+0x800,"venom");put(manager,0xf0,~0u);frame_advance();
 check(force_calls==1&&forced_district==0&&forced_instance==0&&forced_has_data&&equal_text(forced_name,"v09_test")&&equal_text(forced_data,"IGC_intro"),"matching hero dispatches exact authored district name instance and data");
 check(launch.phase==LaunchPhase::idle&&!locked&&memory<u32>(manager+0xcc)==0&&camera_calls==2,"successful launch releases only owned selection lock and camera");
 hero_busy=true;check(!request_mission(rec,reinterpret_cast<void*>(manager)),"Hero Select transaction blocks overlapping mission");hero_busy=false;
 put(manager,0xcc,1);check(!request_mission(rec,reinterpret_cast<void*>(manager)),"native selection lock is respected");put(manager,0xcc,0);
 check(request_mission(rec,reinterpret_cast<void*>(manager)),"second mission can queue after completed transaction");put(mission_manager_pointer,0,manager+0x1000);frame_advance();
 check(launch.phase==LaunchPhase::idle&&!locked&&memory<u32>(manager+0xcc)==0,"manager replacement cancels without touching old lock");put(mission_manager_pointer,0,manager);
 put(fixture_game,0x1a8,1);check(!request_mission(rec,reinterpret_cast<void*>(manager))&&!locked,"disabled physics rejects native mission state machine before locking");
 menu::Entry frozen_unload{};unload_mission(frozen_unload,menu::Command::activate);check(launch.phase==LaunchPhase::idle&&!locked,"disabled physics rejects unload that cannot advance");put(fixture_game,0x1a8,0);
 character_resources=true;cleanup_ready=false;const u32 old_unloads=unload_calls;
 check(request_mission(rec,reinterpret_cast<void*>(manager))&&locked,"active lineup cleanup owns interaction while native worker drains");frame_advance();
 check(unload_calls==old_unloads&&launch.phase==LaunchPhase::queued&&locked,"mission waits for character packs with navigation locked");
 cleanup_ready=true;frame_advance();check(unload_calls==old_unloads+1&&locked,"drained lineup permits native mission unload");frame_advance();check(!locked&&launch.phase==LaunchPhase::idle,"post-cleanup mission completes normally");
 character_resources=true;cleanup_ready=false;menu::Entry unload_entry{};const u32 before_unload=unload_calls,before_force=force_calls;
 unload_mission(unload_entry,menu::Command::activate);frame_advance();check(launch.unload_only&&unload_calls==before_unload&&locked,"explicit Unload waits automatic lineup cleanup");
 cleanup_ready=true;frame_advance();frame_advance();check(launch.phase==LaunchPhase::idle&&unload_calls==before_unload+1&&force_calls==before_force&&!locked,"explicit Unload resumes automatically without dispatching a mission");
 // Native mashed mission/descriptor data may be ARAM-paged. Keep mutable
 // manager/player validation in MEM1, but read resources owned by READY slots.
 text_at(paged_text,"v09_paged");text_at(paged_text+0x100,"IGC_paged");
 put(paged_table,0x38,paged_conditions);memory<u16>(paged_table+0x3c)=1;
 put(paged_conditions,8,paged_instances);memory<u16>(paged_conditions+0xc)=1;
 put(paged_conditions,0x18,paged_text);put(paged_conditions,0x28,0x4000);
 put(paged_instances,0x18,paged_text+0x100);put(paged_instances,0x54,8);
 put(manager,0x14,paged_table);mission_record_count=0;
 check(add_mission_table(reinterpret_cast<void*>(paged_table),0,0)&&mission_record_count==1,"ARAM-paged mission table conditions instances and names populate catalogue");
 check(mission_record_count==1&&mission_record_is_current(reinterpret_cast<void*>(manager),mission_records[0]),"ARAM-paged mission record proves live manager ownership");
 put(manager,0x14,table);check(mission_record_count==0||!mission_record_is_current(reinterpret_cast<void*>(manager),mission_records[0]),"replaced paged mission table cannot activate cached record");
 text_at(paged_levels,"CITY_ARENA");text_at(paged_levels+0x60,"City");text_at(paged_levels+0x90,"CHARACTERB_ARENA");text_at(paged_levels+0xf0,"Characterb");
 paged_descriptors=true;void* level_data=nullptr;s32 level_bytes=0;const void* found_level=nullptr;
 check(get_level_descriptors(level_data,level_bytes)&&level_data==reinterpret_cast<void*>(paged_levels)&&level_bytes==0x120,"native level type9 key retrieves owned ARAM-paged descriptors");
 check(find_level_descriptor(level_data,level_bytes,"city_arena",found_level)&&found_level==reinterpret_cast<void*>(paged_levels),"explicit lowercase alias matches uppercase authored paged descriptor");
 const u32 before_directory=directory_calls;
 put(partition,0xc,partition+0x100);check(!get_level_descriptors(level_data,level_bytes)&&directory_calls==before_directory,"empty game partition prevents native directory lookup");put(partition,0xc,partition+0x104);
 put(partition+0x200,8,3);check(!get_level_descriptors(level_data,level_bytes)&&directory_calls==before_directory,"unloading game slot prevents native directory lookup");put(partition+0x200,8,4);
 put(directory,0x78,0);check(!get_level_descriptors(level_data,level_bytes)&&directory_calls==before_directory,"foreign game directory prevents native lookup");put(directory,0x78,partition+0x200);
 auto* paged_level_menu=menu::create("Paged Level Select",64);refresh_level_select(*paged_level_menu);
 check(paged_level_menu->count>2&&level_cache_data==reinterpret_cast<void*>(paged_levels),"live Level Select refresh builds menu from ARAM-paged descriptor table");
 paged_descriptors=false;
 // Explicit aliases remain visible without catalogue assets. Missing assets
 // cannot mutate SCENE_NAME or pretend that a native load succeeded.
 text_at(descriptors,"city_arena");text_at(descriptors+0x60,"city");text_at(descriptors+0x90,"characterb_arena");text_at(descriptors+0xf0,"characterb");
 auto* level=menu::create("Level Select",64);auto* heroes=menu::create("Hero Select",10);attach_level_select(*level,*heroes);
 check(equal_text(level->entries[0].label,"city")&&equal_text(level->entries[1].label,"characterb"),"both requested explicit scene aliases are visible");
 check(equal_text(level->entries[4].label,"-- REBOOT --")&&level->entries[4].handler==reboot_action,"generic manual reboot remains after authored level rows");
 level->entries[4].handler(level->entries[4],menu::Command::increment);
 check(reboot_calls==0,"reboot ignores directional commands");
 level->entries[4].handler(level->entries[4],menu::Command::activate);
 check(reboot_calls==1&&persist_calls==0&&load_calls==0,"reboot activates native reset without selecting a scene");
 level->entries[1].handler(level->entries[1],menu::Command::activate);
 check(persist_calls==1&&load_calls==1&&equal_text(persisted_name,"characterb_arena")&&equal_text(loaded_name,"characterb_arena")&&persist_sequence<load_sequence,"characterb writes native SCENE_NAME request before actual level load");
 character_pack=false;level->entries[1].handler(level->entries[1],menu::Command::activate);check(persist_calls==1&&load_calls==1,"missing characterb pack prevents persistence and load");character_pack=true;
 descriptor_count=1;level->entries[1].handler(level->entries[1],menu::Command::activate);check(persist_calls==1&&load_calls==1,"missing characterb descriptor prevents unsafe load");
 const u32 city_closes_before=close_calls;
 pack_ready=false;level->entries[0].handler(level->entries[0],menu::Command::activate);check(persist_calls==1&&scheduled_reboot_calls==0&&load_calls==1,"missing City pack prevents persistence and automatic reboot");pack_ready=true;
 descriptor_count=0;level->entries[0].handler(level->entries[0],menu::Command::activate);check(persist_calls==1&&scheduled_reboot_calls==0&&load_calls==1,"missing City descriptor prevents persistence and automatic reboot");descriptor_count=1;
 persist_ok=false;level->entries[0].handler(level->entries[0],menu::Command::activate);check(persist_calls==2&&scheduled_reboot_calls==0&&load_calls==1&&close_calls==city_closes_before,"failed City persistence request keeps menu open without native reload or reset");persist_ok=true;
 level->entries[0].handler(level->entries[0],menu::Command::activate);check(persist_calls==3&&scheduled_reboot_calls==1&&load_calls==1&&close_calls==city_closes_before&&equal_text(persisted_name,"city_arena"),"City saves scene and schedules acknowledged reboot without native level reload or menu close");
 descriptor_count=3;text_at(descriptors+0x120,"alternate_arena");load_scene("alternate_arena");check(persist_calls==3&&load_calls==2&&equal_text(loaded_name,"alternate_arena"),"other authored level loads preserve behavior without restricted alias persistence");
 character_resources=true;cleanup_ready=false;const u32 before_load=load_calls,before_persist=persist_calls;
 const u32 before_scheduled=scheduled_reboot_calls;
 load_scene("city_arena");check(pending_level.queued&&locked&&load_calls==before_load&&persist_calls==before_persist&&scheduled_reboot_calls==before_scheduled,"City waits cleanup before persistence and automatic reboot");
 frame_advance();check(load_calls==before_load,"pending lineup blocks level transition");cleanup_ready=true;frame_advance();
 check(!pending_level.queued&&load_calls==before_load&&persist_calls==before_persist+1&&scheduled_reboot_calls==before_scheduled+1,"drained lineup resumes City save and acknowledged reboot without native reload");
 descriptor_count=0;refresh_level_select(*level);check(equal_text(level->entries[0].label,"city")&&equal_text(level->entries[1].label,"characterb")&&equal_text(level->entries[4].label,"-- REBOOT --"),"aliases and manual reboot remain visible before descriptors are ready");
 descriptor_count=2;city_only=true;auto* city_menu=menu::create("Retail Level Select",64);attach_level_select(*city_menu,*heroes);
 check(city_menu->count==6&&equal_text(city_menu->entries[3].label,"No additional level packs found"),"City-only retail disc reports no additional levels while retaining aliases");
 const u32 city_load_before=load_calls,city_persist_before=persist_calls,city_scheduled_before=scheduled_reboot_calls;city_menu->entries[0].handler(city_menu->entries[0],menu::Command::activate);
 check(load_calls==city_load_before&&persist_calls==city_persist_before+1&&scheduled_reboot_calls==city_scheduled_before+1&&equal_text(persisted_name,"city_arena"),"City-only retail alias persists scene and schedules reboot without native reload");city_only=false;
 constexpr uptr capacity_conditions=0x81020000u,capacity_names=0x81020400u,capacity_instances=0x81021000u;
 for(u32 i=0;i<9;++i){
  const uptr c=capacity_conditions+i*0x34;char n[32];menu::copy_text(n,32,"s10_capacity_");char digit[2]={static_cast<char>('0'+i),0};menu::append_text(n,32,digit);
  text_at(capacity_names+i*32,n);put(c,0x18,capacity_names+i*32);put(c,8,capacity_instances+i*128*0x68);memory<u16>(c+0xc)=128;put(c,0x28,0x8000);
 }
 put(table,0x38,capacity_conditions);memory<u16>(table+0x3c)=5;mission_record_count=0;mission_catalogue_truncated=false;
 check(add_mission_table(reinterpret_cast<void*>(table),0,0)&&mission_record_count==640&&!mission_catalogue_truncated,"complete authored catalogue extends beyond former512 record limit");
 memory<u16>(table+0x3c)=9;mission_record_count=0;mission_catalogue_truncated=false;
 check(add_mission_table(reinterpret_cast<void*>(table),0,0)&&mission_record_count==1024&&mission_catalogue_truncated,"excess authored catalogue is bounded and explicitly reports truncation");
 char number[16];menu::format_s32(number,16,static_cast<s32>(checks));out(number);out(failures==0?" mission, hero transaction and scene alias checks passed\n":" mission checks failed\n");return failures?1:0;
}
asm(".global _start\n_start:\ncall run_tests\nmov %eax,%ebx\nmov $1,%eax\nint $0x80\n");
