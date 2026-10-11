// Freestanding i386 checks of the real GameCube diagnostics implementation.
// Fixed MEM1 fixture data substitutes only the live engine state boundary.
#include "../../gamecube/menus/diagnostics_gc.cpp"
using namespace usm::gc;
namespace {
u32 checks;
u32 failures;
int system_call(u32 number,u32 a,u32 b=0,u32 c=0){int r;asm volatile("int $0x80":"=a"(r):"a"(number),"b"(a),"c"(b),"d"(c):"memory");return r;}
void print(const char* text){u32 n=0;while(text[n])++n;system_call(4,1,reinterpret_cast<uptr>(text),n);}
void check(bool value,const char* label){++checks;if(!value){++failures;print("FAIL: ");print(label);print("\n");}}
void put(uptr a,u32 v){*reinterpret_cast<u32*>(a)=v;}
bool same(const char* a,const char* b){while(*a&&*a==*b){++a;++b;}return *a==*b;}
bool contains(const char* text,const char* fragment){for(;*text;++text){const char* a=text;const char* b=fragment;while(*b&&*a==*b){++a;++b;}if(!*b)return true;}return !*fragment;}
char export_output[131072];
u32 export_length,export_calls;
bool export_result;
usm::gc::ai_export::Serializer exported_serializer;
u32 pushes,pops,resolves,compatibility_calls,play_calls,speed_calls,controller_creations;
uptr pushed_slot,resolver_als,resolver_actor,compatibility_controller,play_controller,speed_controller;
u32 resolver_hash,play_hash,play_flags,returned_layer=1u;
float play_blend,speed_value;
bool play_loop,compatible=true,native_lock_observed;
uptr resolver_result;
uptr returned_owner;
u32 expected_skeleton_type[8];
bool skeleton_type_matches;
void boundary(uptr address,uptr function){*reinterpret_cast<u8*>(address)=0xE9u;put(address+1u,function-address-5u);}
void* push_context(void* slot){++pushes;pushed_slot=reinterpret_cast<uptr>(slot);native_lock_observed=menu::interaction_is_locked();return slot;}
void* pop_context(){++pops;return nullptr;}
void create_controller(void*,u32,void*){++controller_creations;}
void* resolve_animation(const u32* hash,void* als,void* actor){++resolves;resolver_hash=*hash;resolver_als=reinterpret_cast<uptr>(als);resolver_actor=reinterpret_cast<uptr>(actor);return reinterpret_cast<void*>(resolver_result);}
bool skeleton_compatible(void* controller,const u32* types){++compatibility_calls;compatibility_controller=reinterpret_cast<uptr>(controller);skeleton_type_matches=true;for(u32 index=0;index<8;++index)if(types[index]!=expected_skeleton_type[index])skeleton_type_matches=false;return compatible;}
void native_play(void* handle,void* controller,const u32* hash,float blend,u32 flags,bool loop){++play_calls;play_controller=reinterpret_cast<uptr>(controller);play_hash=*hash;play_blend=blend;play_flags=flags;play_loop=loop;auto* result=static_cast<u32*>(handle);result[0]=returned_layer;result[1]=77u;result[2]=returned_owner!=0u?returned_owner:play_controller;}
void native_speed(void* controller,float speed){++speed_calls;speed_controller=reinterpret_cast<uptr>(controller);speed_value=speed;}
void reset_native_calls(){pushes=0;pops=0;resolves=0;compatibility_calls=0;play_calls=0;speed_calls=0;controller_creations=0;native_lock_observed=false;returned_layer=1u;returned_owner=0u;compatible=true;}
}
namespace usm::gc::engine {
void* world(){return reinterpret_cast<void*>(0x81000000u);}
bool game_ready(){return true;}
bool interactive_world_ready(){return true;}
bool gameplay_rel_ready(){return true;}
}
namespace usm::gc::ai_export {
bool request_export(Serializer serializer){++export_calls;exported_serializer=serializer;export_result=serializer(export_output,sizeof(export_output),export_length);return export_result;}
}
extern "C" void* memcpy(void* d,const void* s,unsigned int n){auto* o=static_cast<u8*>(d);auto* i=static_cast<const u8*>(s);for(u32 k=0;k<n;++k)o[k]=i[k];return d;}
extern "C" void* memset(void* d,int v,unsigned int n){auto* o=static_cast<u8*>(d);for(u32 k=0;k<n;++k)o[k]=static_cast<u8>(v);return d;}
extern "C" unsigned int strlen(const char* s){u32 n=0;while(s[n])++n;return n;}
extern "C" int run_tests(){
u32 mapping[6]={0x80000000u,0x1800000u,7u,0x32u,~0u,0u};
if(static_cast<uptr>(system_call(90,reinterpret_cast<uptr>(mapping)))!=mapping[0]){print("Cannot map MEM1 fixture\n");return 2;}
using namespace usm::gc::diagnostics;
const uptr list=0x81001000u,sentinel=0x81001020u,node=0x81001040u,core=0x81002000u,actor=0x81003000u;
put(ai_high_list_slot,list);put(list+4,sentinel);put(sentinel,node);put(sentinel+4,node);put(node,sentinel);put(node+4,sentinel);put(node+8,core);put(core+0x58,actor);
put(0x8045F7B4u,0u);put(0x8045F7B0u,list);
check(scan_ai().total==1u,"AI scans the native high-priority list rather than the adjacent low-priority global");
put(0x8045F7B0u,0u);put(0x8045F7B4u,list);
check(scan_ai().state==ScanState::ready&&scan_ai().total==0u,"populated native low-priority list cannot substitute for an empty high-priority list");
put(0x8045F7B4u,0u);put(0x8045F7B0u,list);
put(ai_high_list_slot,list);
selected_ai_core=core;selected_ai_actor=actor;selected_ai_world=0x81000000u;selected_ai_handle=42;put(actor+0x1c,42);
check(ai_owner_live(),"AI owner accepts live high priority core");
const uptr pb=core+0x44,array=0x81004000u,values=0x81004100u,param=0x81004200u;
put(pb+4,array);put(array+4,1);put(array+8,values);put(values,param);put(param+4,1);put(param,12);
selected_parameter_block=pb;check(parameter_live(param),"parameter belongs to selected core");
menu::Entry e{};e.target=reinterpret_cast<void*>(param);put(0x8100014c,91);
parameter_handler(e,menu::Command::increment);check(load_s32(param)==13,"integer increment");check(load_u32(pb)==91,"native frame stamp updated");
put(param,0x7fffffff);parameter_handler(e,menu::Command::increment);check(load_s32(param)==2147483647,"integer overflow blocked");
put(param,0x80000000u);parameter_handler(e,menu::Command::decrement);check(load_s32(param)==(-2147483647-1),"integer underflow blocked");
put(param+4,0);*reinterpret_cast<float*>(param)=1.0f;parameter_handler(e,menu::Command::increment);check(*reinterpret_cast<float*>(param)>1.29f&&*reinterpret_cast<float*>(param)<1.31f,"float adjustment");
put(param,0x7fc00000u);parameter_handler(e,menu::Command::increment);check(load_u32(param)==0x7fc00000u,"NaN ignored");
selected_parameter_block=0x81005000u;put(selected_parameter_block+4,array);check(!parameter_live(param),"foreign block rejected despite same array");
selected_parameter_block=pb;put(node+8,0);check(!parameter_live(param),"expired core blocks mutation");put(node+8,core);
const uptr slot=0x81006000u,dir=0x81007000u,locations=0x81008000u,file=0x81009000u,clip=0x8100A000u,clip2=0x8100A100u;
put(actor+0xbc,slot);put(slot+8,4);put(slot+0x18,dir);put(dir+0x78,slot);put(dir+0x50,locations);*reinterpret_cast<u16*>(dir+0x54)=2;
put(locations,123u);put(locations+8u,clip);put(locations+12u,456u);put(locations+20u,clip2);
put(clip,0x8035B420u);put(clip2,0x8035B420u);put(clip+4,clip2);put(clip2+4,0);put(clip+8,123);put(clip2+8,456);
uptr clips[2]{};u32 count=0;check(actor_animations(actor,0,clips,2,count)&&count==2&&clips[0]==clip&&clips[1]==clip2,"native type9 locations enumerate direct animation clips exactly once");
clips[0]=0;check(actor_animations(actor,1,clips,1,count)&&count==2u&&clips[0]==clip2,"animation pagination uses location clip index");
put(clip2+4,clip);check(actor_animations(actor,0,clips,2,count)&&count==2u,"animation location enumeration never follows overlapping or cyclic clip next links");put(clip2+4,0);
put(locations+20u,0u);check(actor_animations(actor,0,clips,2,count)&&count==1u,"unloaded null animation locations are skipped");put(locations+20u,clip2);
put(file,0x10101u);put(file+4u,8u);put(file+0x34u,clip);put(locations+8u,file);
check(!actor_animations(actor,0,clips,2,count),"animation location rejects an anim-file container instead of confusing it with a direct clip");put(locations+8u,clip);
put(locations,124u);check(!actor_animations(actor,0,clips,2,count),"animation location hash must match its direct clip hash");put(locations,123u);
put(clip,0x8035B421u);check(!actor_animations(actor,0,clips,2,count),"animation direct clip requires an aligned runtime vtable pointer");put(clip,0x8035B420u);
*reinterpret_cast<u16*>(dir+0x54)=4097u;check(!actor_animations(actor,0,clips,2,count),"animation location array rejects counts beyond the native safety bound");*reinterpret_cast<u16*>(dir+0x54)=2u;
const uptr vi=0x8100b000u,var=0x8100c000u;put(actor+0x12c,vi);put(vi,0x80344ce0);put(vi+4,actor);put(vi+0xc,var);*reinterpret_cast<u16*>(vi+0x10)=2;
uptr iface,out;check(variant_data(actor,iface,out,count)&&count==2&&out==var,"GC variant vector layout");put(vi+4,0);check(!variant_data(actor,iface,out,count),"foreign variant interface rejected");

put(actor+0x1c,43);check(!ai_owner_live(),"recycled actor handle rejects stale AI owner");
put(actor+0x1c,42);check(ai_owner_live(),"original actor handle restores AI ownership");
put(core+0x54,0x8100d000u);put(0x8100d004u,1);put(0x8100d008u,0x8100d100u);put(0x8100d100u,0x8100d200u);put(0x8100d208u,core);put(0x8100d214u,array);selected_parameter_block=0x8100d210u;
check(parameter_live(param),"live inode block belongs to selected core");put(0x8100d208u,0);check(!parameter_live(param),"foreign inode core rejected");

// The native entity search leaves this fixture list unchanged; unresolved
// string hashes use the production hexadecimal-label fallback.
auto* find_stub=reinterpret_cast<u8*>(find_entities_entry);
find_stub[0]=0x31u;find_stub[1]=0xC0u;find_stub[2]=0xC3u;
auto* hash_stub=reinterpret_cast<u8*>(hash_to_string_entry);
hash_stub[0]=0x31u;hash_stub[1]=0xC0u;hash_stub[2]=0xC3u;
constexpr uptr nodes_base=0x81100000u,cores_base=0x81110000u,actors_base=0x81130000u;
for(u32 index=0;index<70;++index){
    const uptr current=nodes_base+index*16u;
    put(current,index==69?sentinel:current+16u);
    put(current+4,index==0?sentinel:current-16u);
    put(current+8,cores_base+index*0x100u);
    put(cores_base+index*0x100u+0x58u,actors_base+index*0x200u);
    put(actors_base+index*0x200u+0x10u,2000u+index);
    put(actors_base+index*0x200u+0x1Cu,1000u+index);
}
put(sentinel,nodes_base);put(sentinel+4,nodes_base+69u*16u);
const AiSnapshot ai_first=scan_ai(),ai_second=scan_ai(30u),ai_last=scan_ai(60u);
check(ai_first.visible_total==70u&&ai_first.record_count==30u,"AI first page reports full population");
check(ai_second.records[0].core==cores_base+30u*0x100u&&ai_second.record_count==30u,"AI second page begins at record31");
check(ai_last.record_count==10u&&ai_last.records[9].core==cores_base+69u*0x100u,"AI final record reachable on third page");
selected_ai_core=cores_base+69u*0x100u;selected_ai_actor=actors_base+69u*0x200u;selected_ai_handle=1069u;
check(ai_owner_live(),"AI editor owner lookup reaches beyond first display page");
auto* ai_menu=menu::create("AI fixture",32u);configure_ai(*ai_menu);
check(ai_menu->count==32u&&ai_menu->entries[1].type==menu::EntryType::custom,"AI root reserves a page row");
ai_menu->entries[1].handler(ai_menu->entries[1],menu::Command::increment);refresh_ai(*ai_menu);
check(reinterpret_cast<uptr>(ai_menu->entries[2].target)==cores_base+30u*0x100u,"AI page control rebuilds next page");
ai_menu->entries[1].handler(ai_menu->entries[1],menu::Command::increment);refresh_ai(*ai_menu);
check(ai_menu->count==12u&&reinterpret_cast<uptr>(ai_menu->entries[11].target)==cores_base+69u*0x100u,"AI last-page root target retained");

constexpr uptr inode_vector=0x81150000u,inode_values=0x81151000u,inodes=0x81160000u;
const uptr last_core=cores_base+69u*0x100u;
put(last_core+0x54u,inode_vector);put(inode_vector+4u,70u);put(inode_vector+8u,inode_values);
for(u32 index=0;index<70;++index){put(inode_values+index*4u,inodes+index*0x40u);put(inodes+index*0x40u+4u,3000u+index);put(inodes+index*0x40u+8u,last_core);}
ai_menu->selected=11u;ai_detail_menu->parent=ai_menu;refresh_ai_details(*ai_detail_menu);
check(ai_detail_menu->count==64u&&ai_detail_menu->entries[1].type==menu::EntryType::custom,"inode list keeps core parameters plus page control");
ai_detail_menu->entries[1].handler(ai_detail_menu->entries[1],menu::Command::increment);refresh_ai_details(*ai_detail_menu);
check(ai_detail_menu->count==10u&&reinterpret_cast<uptr>(ai_detail_menu->entries[9].target)==inodes+69u*0x40u+0x10u,"inode final record reachable beyond former63-row limit");
inode_page=99u;refresh_ai_details(*ai_detail_menu);check(inode_page==1u,"inode page clamps to last valid page");
// Removing an inode must invalidate its open parameter list even when the
// core, actor handle and old parameter allocation are still live and readable.
const uptr last_inode=inodes+69u*0x40u;
put(last_inode+0x14u,array);ai_detail_menu->selected=9u;ai_parameter_menu->parent=ai_detail_menu;
refresh_ai_parameters(*ai_parameter_menu);
check(ai_parameter_menu->count==3u&&reinterpret_cast<uptr>(ai_parameter_menu->entries[2].target)==param,"live inode parameter list enumerates values after page and export rows");
put(inode_vector+4u,69u);
check(ai_owner_live(),"removing inode preserves live AI core ownership");
refresh_ai_parameters(*ai_parameter_menu);
check(ai_parameter_menu->count==1u&&ai_parameter_menu->entries[0].type==menu::EntryType::read_only&&ai_parameter_menu->entries[0].target==nullptr,"removed inode parameter list shows owner expired without stale values");
put(inode_vector+4u,70u);
put(nodes_base+4u*16u,sentinel);put(sentinel+4u,nodes_base+4u*16u);refresh_ai(*ai_menu);
check(ai_root_page==0u&&ai_menu->count==7u,"AI page clamps after live population shrinks");
put(nodes_base+4u*16u,nodes_base+5u*16u);put(sentinel+4u,nodes_base+69u*16u);

// Every actor has two loaded clips and two variants, so the root pages must
// expose all70 live actors rather than silently dropping the last39.
put(found_entities_slot,list);put(0x811900D4u,actor_is_actor_entry);
for(u32 index=0;index<70;++index){
    const uptr current_actor=actors_base+index*0x200u,current_vi=0x811A0000u+index*0x100u;
    put(nodes_base+index*16u+8u,current_actor);put(current_actor,0x81190000u);put(current_actor+0xBCu,slot);
    put(current_actor+0x12Cu,current_vi);put(current_vi,0x80344CE0u);put(current_vi+4u,current_actor);
    put(current_vi+0xCu,var);*reinterpret_cast<u16*>(current_vi+0x10u)=2u;
}
const EntitySnapshot actor_first=scan_entity_animations(),actor_second=scan_entity_animations(31u),actor_last=scan_entity_animations(62u);
check(actor_first.actor_count==70u&&actor_first.record_count==31u,"animation root scan reports full actor count");
check(actor_second.records[0].actor==actors_base+31u*0x200u,"animation second-page scan starts at record32");
check(actor_last.record_count==8u&&actor_last.records[7].actor==actors_base+69u*0x200u,"animation final actor remains reachable");
auto* animation_menu=menu::create("Animation fixture",32u);configure_entity_animations(*animation_menu);
check(animation_menu->count==32u&&animation_menu->entries[0].type==menu::EntryType::custom,"animation root reserves a page row");
animation_menu->entries[0].handler(animation_menu->entries[0],menu::Command::increment);refresh_entity_animations(*animation_menu);
check(reinterpret_cast<uptr>(animation_menu->entries[1].target)==actors_base+31u*0x200u,"animation page change bypasses refresh throttle");
animation_menu->entries[0].handler(animation_menu->entries[0],menu::Command::increment);refresh_entity_animations(*animation_menu);
check(animation_menu->count==9u&&reinterpret_cast<uptr>(animation_menu->entries[8].target)==actors_base+69u*0x200u,"animation final root page preserves actor target");
const EntitySnapshot variant_first=scan_entity_variants(),variant_last=scan_entity_variants(62u);
check(variant_first.actor_count==70u&&variant_first.record_count==31u,"variant scan reports full actor population");
check(variant_last.record_count==8u&&variant_last.records[7].actor_handle==1069u,"variant final-page scan preserves lifetime handle");
auto* variant_menu=menu::create("Variant fixture",32u);configure_entity_variants(*variant_menu);
check(variant_menu->count==32u&&variant_menu->entries[0].type==menu::EntryType::custom,"variant root reserves a page row");
variant_menu->entries[0].handler(variant_menu->entries[0],menu::Command::increment);refresh_entity_variants(*variant_menu);
check(reinterpret_cast<uptr>(variant_menu->entries[1].target)==actors_base+31u*0x200u,"variant root page control reaches next actor group");
variant_menu->entries[0].handler(variant_menu->entries[0],menu::Command::increment);refresh_entity_variants(*variant_menu);
check(variant_menu->count==9u&&static_cast<u32>(variant_menu->entries[8].minimum)==1069u,"variant final root page preserves actor identity");

// Authored data may be translated into the native F0 ARAM pager. Only the
// live pack's read-only extent is accepted; editor-owned state stays in MEM1.
u32 paged_mapping[6]={0xF0000000u,0x10000u,7u,0x32u,~0u,0u};
if(static_cast<uptr>(system_call(90,reinterpret_cast<uptr>(paged_mapping)))!=paged_mapping[0]){print("Cannot map paged resource fixture\n");return 2;}
constexpr uptr manager=0x81300000u,partitions=0x81300100u,partition=0x81300200u,slots=0x81300300u;
put(0x8045F214u,manager);put(manager,partitions);put(partitions,partition);put(partition+8u,slots);put(partition+12u,slots+4u);put(slots,slot);
put(slot+8u,4u);put(dir+0x78u,slot);put(dir+0x80u,0xF0000000u);put(dir+0x88u,0x1000u);
constexpr uptr paged_locations=0xF0000100u,paged_clip=0xF0000300u,paged_clip2=0xF0000400u,paged_variants=0xF0000500u,paged_label=0xF0000600u,paged_vector=0xF0000700u;
put(dir+0x50u,paged_locations);put(paged_locations,123u);put(paged_locations+8u,paged_clip);put(paged_locations+12u,456u);put(paged_locations+20u,paged_clip2);
put(paged_clip,0x8035B420u);put(paged_clip2,0x8035B420u);put(paged_clip+4u,paged_clip2);put(paged_clip2+4u,0u);put(paged_clip+8u,123u);put(paged_clip2+8u,456u);
check(actor_animations(actor,0u,clips,2u,count)&&count==2u&&clips[0]==paged_clip&&clips[1]==paged_clip2,"owned paged animation location array exposes each direct clip once");
put(paged_clip2+4u,paged_clip);check(actor_animations(actor,0u,clips,2u,count)&&count==2u,"owned paged direct clip next links cannot duplicate clips or create a scan cycle");put(paged_clip2+4u,0u);
put(paged_clip,0xF0000200u);check(!actor_animations(actor,0u,clips,2u,count),"readable paged resource data cannot substitute for a runtime animation vtable");put(paged_clip,0x8035B420u);
put(vi+4u,actor);put(vi+0xCu,paged_variants);put(paged_variants,987u);put(paged_variants+16u,654u);
check(variant_data(actor,iface,out,count)&&count==2u&&out==paged_variants,"owned paged authored variant array is readable");
memcpy(reinterpret_cast<void*>(paged_label),"paged label",12u);char rendered[64];
check(copy_runtime_string(rendered,sizeof(rendered),reinterpret_cast<const char*>(paged_label))&&same(rendered,"paged label"),"owned paged resource label is copied safely");
put(sentinel,node);put(sentinel+4u,node);put(node+8u,core);selected_ai_core=core;selected_ai_actor=actor;selected_ai_world=0x81000000u;selected_ai_handle=42u;selected_parameter_block=pb;
put(param,paged_label);put(param+4u,3u);e.target=reinterpret_cast<void*>(param);parameter_formatter(e,rendered,sizeof(rendered));
check(same(rendered,"paged label"),"AI string display accepts owned paged authored text");
put(param,paged_vector);put(param+4u,4u);*reinterpret_cast<float*>(paged_vector)=1.0f;*reinterpret_cast<float*>(paged_vector+4u)=2.0f;*reinterpret_cast<float*>(paged_vector+8u)=3.0f;
parameter_formatter(e,rendered,sizeof(rendered));
check(same(rendered,"1.0, 2.0, 3.0"),"AI vector display accepts owned paged authored components");
put(paged_vector+0x40u,17u);put(paged_vector+0x44u,1u);put(values,paged_vector+0x40u);
e.target=reinterpret_cast<void*>(paged_vector+0x40u);parameter_handler(e,menu::Command::increment);
check(!parameter_live(paged_vector+0x40u)&&load_u32(paged_vector+0x40u)==17u,"paged resource readability cannot authorize mutable AI parameter writes");put(values,param);
put(paged_locations+20u,0xF0000FF0u);check(!actor_animations(actor,0u,clips,2u,count),"direct paged animation clip cannot cross its pack extent");put(paged_locations+20u,paged_clip2);
put(paged_locations+20u,0xF0001100u);check(!actor_animations(actor,0u,clips,2u,count),"unowned direct paged animation location is rejected before reading its clip");put(paged_locations+20u,paged_clip2);
put(dir+0x88u,0x708u);e.target=reinterpret_cast<void*>(param);parameter_formatter(e,rendered,sizeof(rendered));check(same(rendered,"invalid value"),"AI vector spanning beyond its pack extent is rejected");put(dir+0x88u,0x1000u);
put(slot+8u,3u);check(!copy_runtime_string(rendered,sizeof(rendered),reinterpret_cast<const char*>(paged_label))&&!actor_animations(actor,0u,clips,2u,count)&&!variant_data(actor,iface,out,count),"native unload invalidates cached paged labels animations and variants");
put(slot+8u,4u);
put(dir+0x78u,slot+0x100u);
check(!actor_animations(actor,0u,clips,2u,count),"animation bank rejects a directory belonging to another resource slot");
put(dir+0x78u,slot);
check(actor_animations(actor,0u,clips,2u,count)&&count==2u,"animation bank recovers only after READY state and ownership back-reference match");

// The export action must serialize the entire live block, including rows
// outside the selected page, with typed/raw values and untruncated escaping.
constexpr uptr export_values=0x81410000u,export_parameters=0x81411000u,export_string=0xF0000800u;
put(array+4u,130u);put(array+8u,export_values);
for(u32 index=0;index<130u;++index){const uptr current=export_parameters+index*16u;put(export_values+index*4u,current);put(current,index);put(current+4u,1u);put(current+8u,0xA0000000u+index);}
for(u32 type=0u;type<8u;++type)put(export_parameters+type*16u+4u,type);
put(export_parameters,0x3FA00000u);put(export_parameters+16u,0xFFFFFFEFu);
put(export_parameters+32u,0xABCDEF01u);put(export_parameters+48u,export_string);
put(export_parameters+64u,paged_vector);put(export_parameters+80u,paged_vector+0x20u);
put(export_parameters+96u,0xDEADBEEFu);put(export_parameters+112u,0xCAFEBABEu);
put(paged_vector,0x3F800000u);put(paged_vector+4u,0xC0200000u);put(paged_vector+8u,0x40500000u);
put(paged_vector+0x20u,0x3F000000u);put(paged_vector+0x24u,0x3FA00000u);
for(u32 index=0u;index<80u;++index)*reinterpret_cast<char*>(export_string+index)=static_cast<char>('A'+index%26u);
const char escaped_tail[]={'"','\\','\n','\r','\t',1,static_cast<char>(0x80),'Z','\0'};
memcpy(reinterpret_cast<void*>(export_string+80u),escaped_tail,sizeof(escaped_tail));
auto* export_parent=menu::create("AI export fixture",1u);
auto* export_owner=menu::add(*export_parent,"Core parameters",menu::EntryType::submenu);
export_owner->target=reinterpret_cast<void*>(pb);ai_parameter_menu->parent=export_parent;
selected_parameter_block=pb;parameter_page=0u;refresh_ai_parameters(*ai_parameter_menu);
check(ai_parameter_menu->count==128u&&same(ai_parameter_menu->entries[1].label,"--Export this block--"),"AI parameter page reserves the export action and126 editable rows");
check(reinterpret_cast<uptr>(ai_parameter_menu->entries[127].target)==export_parameters+125u*16u,"AI first parameter page reaches row126 without losing reserved rows");
ai_parameter_menu->entries[0].handler(ai_parameter_menu->entries[0],menu::Command::increment);refresh_ai_parameters(*ai_parameter_menu);
check(parameter_page==1u&&ai_parameter_menu->count==6u&&reinterpret_cast<uptr>(ai_parameter_menu->entries[2].target)==export_parameters+126u*16u,"AI parameter second page starts at127 and keeps all trailing parameters");
ai_parameter_menu->entries[1].handler(ai_parameter_menu->entries[1],menu::Command::activate);
check(export_calls==1u&&exported_serializer==serialize_selected_ai_block&&export_result,"AI export action invokes the real complete-block serializer");
check(export_length==strlen(export_output)&&contains(export_output,"count=130\n")&&contains(export_output,"129 name_hash=0xA0000081 type=int"),"AI export includes all130 parameters regardless of the visible page");
const char* const type_names[]={" type=float "," type=int "," type=string_hash "," type=fixedstring "," type=vector3d "," type=float_variance "," type=entity "," type=pointer "};
bool all_types=true;for(u32 type=0u;type<8u;++type)all_types=all_types&&contains(export_output,type_names[type]);
check(all_types,"AI export retains all eight authored parameter types");
check(contains(export_output,"type=float raw=0x3FA00000 value=1.3")&&contains(export_output,"type=int raw=0xFFFFFFEF value=-17"),"AI export includes exact scalar words alongside display values");
check(contains(export_output,"1.0[0x3F800000], -2.5[0xC0200000], 3.3[0x40500000]")&&contains(export_output,"0.5[0x3F000000], 1.3[0x3FA00000]"),"AI export includes all vector and variance components and their exact words");
check(contains(export_output,"type=string_hash raw=0xABCDEF01 value=0xABCDEF01")&&contains(export_output,"type=entity raw=0xDEADBEEF value=0xDEADBEEF")&&contains(export_output,"type=pointer raw=0xCAFEBABE value=0xCAFEBABE"),"AI export preserves hash entity and pointer values without dereferencing them");
check(contains(export_output,"ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZAB")&&contains(export_output,"\\\"\\\\\\n\\r\\t\\x01\\x80Z\""),"AI export retains strings longer than the UI label and escapes quotes slashes controls and high bytes");
char short_export[64];u32 short_length=99u;
check(!serialize_selected_ai_block(short_export,sizeof(short_export),short_length)&&short_length==0u&&short_export[0]=='\0',"AI export rejects insufficient output space without publishing a partial block");
put(actor+0x1Cu,43u);export_length=99u;
check(!serialize_selected_ai_block(export_output,sizeof(export_output),export_length)&&export_length==0u&&export_output[0]=='\0',"AI export rejects a recycled actor owner before reading parameters");put(actor+0x1Cu,42u);
put(export_parameters+48u,0xF0001100u);
check(!serialize_selected_ai_block(export_output,sizeof(export_output),export_length)&&export_length==0u,"AI export rejects an unowned paged string");put(export_parameters+48u,export_string);
put(dir+0x88u,0x850u);
check(!serialize_selected_ai_block(export_output,sizeof(export_output),export_length)&&export_output[0]=='\0',"AI export rejects a string with no terminator inside its owner extent");put(dir+0x88u,0x1000u);
put(export_parameters+8u*16u+4u,8u);
check(!serialize_selected_ai_block(export_output,sizeof(export_output),export_length)&&export_length==0u,"AI export refuses unknown types rather than silently dropping a parameter");put(export_parameters+8u*16u+4u,1u);
put(export_values+129u*4u,0xF0000A00u);
check(!serialize_selected_ai_block(export_output,sizeof(export_output),export_length),"AI export cannot treat read-only paged data as a live mutable parameter");put(export_values+129u*4u,export_parameters+129u*16u);
put(array+4u,8u);refresh_ai_parameters(*ai_parameter_menu);
check(parameter_page==0u&&ai_parameter_menu->count==10u,"AI parameter page clamps when the live block shrinks");
put(array+4u,4097u);
check(!serialize_selected_ai_block(export_output,sizeof(export_output),export_length)&&export_length==0u,"AI export rejects an excessive live parameter count before traversing it");put(array+4u,8u);
constexpr uptr export_inode=0x81403000u,export_inode_vector=0x81403100u,export_inode_values=0x81403200u;
put(core+0x54u,export_inode_vector);put(export_inode_vector+4u,1u);put(export_inode_vector+8u,export_inode_values);put(export_inode_values,export_inode);
put(export_inode+8u,core);put(export_inode+0x14u,array);export_owner->target=reinterpret_cast<void*>(export_inode+0x10u);refresh_ai_parameters(*ai_parameter_menu);
check(ai_parameter_menu->count==10u&&selected_parameter_block==export_inode+0x10u,"AI export entry can select a live inode-owned parameter block");
put(export_inode_vector+4u,0u);ai_parameter_menu->entries[1].handler(ai_parameter_menu->entries[1],menu::Command::activate);
check(ai_owner_live()&&!export_result&&export_length==0u,"AI export revalidates a removed inode block even while the AI core remains live");
export_owner->target=reinterpret_cast<void*>(pb);selected_parameter_block=pb;

// A find_entities result may mix a legal static ARAM object and a mutable
// actor. The static entry must not suppress the actor, but an unowned pager
// pointer must invalidate the complete search result.
constexpr uptr extra_node=0x81001060u,static_entity=0xF0000B00u;
put(sentinel,node);put(sentinel+4u,extra_node);put(node,sentinel+0x40u);put(node+4u,sentinel);put(node+8u,static_entity);
put(extra_node,sentinel);put(extra_node+4u,node);put(extra_node+8u,actor);put(actor,0x81190000u);
const EntitySnapshot mixed=scan_entity_animations();
check(mixed.state==ScanState::ready&&mixed.actor_count==1u&&mixed.records[0].actor==actor,"owned static paged entity does not hide a valid MEM1 animation actor");
put(node+8u,0xF0001100u);
check(scan_entity_animations().state==ScanState::invalid,"unowned paged entity invalidates a native entity search instead of being dereferenced");put(node+8u,static_entity);

// Fixed-address native boundary stubs preserve the production play path:
// resource context, resolver, skeleton check, returned handle and speed setter.
constexpr uptr controller=0x81400000u,controller_vtable=0x81400100u,skeleton=0xF0000900u,actor_skeleton=0xF0000980u;
put(actor+0x74u,controller);put(controller,actor);put(controller+4u,actor_skeleton);put(controller+8u,0x81400200u);put(controller+0x10u,controller_vtable);
put(controller_vtable+0xA8u,0u);put(controller_vtable+0xACu,0x800A7FA8u);put(paged_clip+0x30u,skeleton);
for(u32 index=0u;index<8u;++index){expected_skeleton_type[index]=0xB000u+index;put(skeleton+0x28u+index*4u,expected_skeleton_type[index]);}
boundary(0x8015CCE8u,reinterpret_cast<uptr>(push_context));boundary(0x8015CD78u,reinterpret_cast<uptr>(pop_context));
boundary(0x8010523Cu,reinterpret_cast<uptr>(create_controller));boundary(0x800A6BB4u,reinterpret_cast<uptr>(resolve_animation));
boundary(0x800A72B8u,reinterpret_cast<uptr>(skeleton_compatible));boundary(0x800A6EA0u,reinterpret_cast<uptr>(native_play));boundary(0x800A7FA8u,reinterpret_cast<uptr>(native_speed));
menu::Entry animation_entry{};animation_entry.target=reinterpret_cast<void*>(actor);animation_entry.minimum=42;animation_entry.id=123u;animation_entry.reserved=0u;
selected_animation_world=0x81000000u;resolver_result=paged_clip;reset_native_calls();
play_animation(animation_entry,menu::Command::activate);
check(pushes==1u&&pops==1u&&pushed_slot==slot&&native_lock_observed&&!menu::interaction_is_locked(),"animation playback balances its native pack context and interaction lock");
check(resolves==1u&&resolver_hash==123u&&resolver_als==0x81400200u&&resolver_actor==actor,"animation playback resolves the selected hash in the actor's actual ALS resource context");
check(compatibility_calls==1u&&compatibility_controller==controller&&skeleton_type_matches,"animation playback validates all eight skeleton signature words before native play");
check(play_calls==1u&&play_controller==controller&&play_hash==123u&&play_blend==0.0f&&play_flags==32u&&play_loop,"animation playback passes native base-layer blend flags and loop parameters");
check(speed_calls==1u&&speed_controller==controller&&speed_value==1.0f,"successful native base-layer handle resets playback to normal speed");
reset_native_calls();resolver_result=0u;play_animation(animation_entry,menu::Command::activate);
check(pushes==1u&&pops==1u&&resolves==1u&&compatibility_calls==0u&&play_calls==0u&&!menu::interaction_is_locked(),"missing native resolved clip exits before play and balances context and lock");resolver_result=paged_clip;
reset_native_calls();put(paged_clip+0x30u,0u);play_animation(animation_entry,menu::Command::activate);
check(pushes==1u&&pops==1u&&resolves==1u&&compatibility_calls==0u&&play_calls==0u&&!menu::interaction_is_locked(),"missing animation skeleton releases native resource context before compatibility or play");put(paged_clip+0x30u,skeleton);
reset_native_calls();compatible=false;play_animation(animation_entry,menu::Command::activate);
check(pushes==1u&&pops==1u&&compatibility_calls==1u&&play_calls==0u&&speed_calls==0u&&!menu::interaction_is_locked(),"incompatible skeleton exits without calling the native fatal playback path");
reset_native_calls();returned_layer=0u;play_animation(animation_entry,menu::Command::activate);
check(pushes==1u&&pops==1u&&play_calls==1u&&speed_calls==0u&&!menu::interaction_is_locked(),"native playback failure never sets speed and still releases its context");
reset_native_calls();returned_owner=controller+4u;play_animation(animation_entry,menu::Command::activate);
check(pushes==1u&&pops==1u&&play_calls==1u&&speed_calls==0u&&!menu::interaction_is_locked(),"base-layer handle for a foreign controller cannot authorize the speed setter");
reset_native_calls();put(controller_vtable+0xA8u,0xFFF0u);play_animation(animation_entry,menu::Command::activate);
check(play_calls==1u&&speed_calls==1u&&speed_controller==controller-16u&&speed_value==1.0f,"native speed setter honors the signed CodeWarrior this adjustment");put(controller_vtable+0xA8u,0u);
reset_native_calls();put(controller_vtable+0xACu,0x800A7FA4u);play_animation(animation_entry,menu::Command::activate);
check(pushes==1u&&pops==1u&&play_calls==0u&&!menu::interaction_is_locked(),"unsupported controller speed vtable blocks playback without an arbitrary native call");put(controller_vtable+0xACu,0x800A7FA8u);
reset_native_calls();put(controller,actor+4u);play_animation(animation_entry,menu::Command::activate);
check(pushes==1u&&pops==1u&&resolves==0u&&play_calls==0u&&!menu::interaction_is_locked(),"controller with a foreign owner balances context without resolving or playing");put(controller,actor);
reset_native_calls();put(actor+0x74u,0u);play_animation(animation_entry,menu::Command::activate);
check(controller_creations==1u&&pushes==1u&&pops==1u&&play_calls==0u&&!menu::interaction_is_locked(),"failed lazy controller creation balances the pack context and unlocks the menu");put(actor+0x74u,controller);
reset_native_calls();put(actor+0x1Cu,43u);play_animation(animation_entry,menu::Command::activate);
check(pushes==0u&&resolves==0u&&play_calls==0u,"stale animation actor handle is rejected before entering native resource context");put(actor+0x1Cu,42u);
reset_native_calls();animation_entry.id=124u;play_animation(animation_entry,menu::Command::activate);
check(pushes==0u&&play_calls==0u,"changed bank hash is rejected before native animation calls");animation_entry.id=123u;
reset_native_calls();put(slot+8u,3u);play_animation(animation_entry,menu::Command::activate);
check(pushes==0u&&play_calls==0u&&!menu::interaction_is_locked(),"unloading animation bank blocks native context entry");put(slot+8u,4u);
reset_native_calls();menu::set_interaction_locked(true);play_animation(animation_entry,menu::Command::activate);
check(pushes==0u&&play_calls==0u&&menu::interaction_is_locked(),"animation action preserves another transaction's interaction lock");menu::set_interaction_locked(false);
char digits[16];u32 length=0,number=checks;
do{digits[length++]=static_cast<char>('0'+number%10u);number/=10u;}while(number!=0u);
print(failures==0?"PASS: ":"FAILED: ");
while(length!=0u){char digit[2]={digits[--length],'\0'};print(digit);}
print(" diagnostics behavior checks\n");return failures==0?0:1;
}
asm(".global _start\n_start:\n call run_tests\n mov %eax, %ebx\n mov $1, %eax\n int $0x80\n");
