// Exercise the real immutable-snapshot mailbox without a game or companion.
#include "../../gamecube/ai_export_gc.cpp"
using namespace usm::gc;
namespace {
u32 checks, failures, calls, statuses, supplied_capacity, observed_request;
u32 wanted_length=5u;
bool serializer_ok=true, recursive_result;
char content='A';
char status[128];
int syscall(u32 n,u32 a,u32 b=0u,u32 c=0u){int r;asm volatile("int $0x80":"=a"(r):"a"(n),"b"(a),"c"(b),"d"(c):"memory");return r;}
void print(const char* text){u32 n=0u;while(text[n])++n;syscall(4u,1u,reinterpret_cast<uptr>(text),n);}
bool equal(const char* a,const char* b){for(u32 i=0u;;++i){if(a[i]!=b[i])return false;if(a[i]=='\0')return true;}}
void check(bool ok,const char* label){++checks;if(!ok){++failures;print("FAIL: ");print(label);print("\n");}}
bool serialize(char* output,u32 capacity,u32& length){
 ++calls;supplied_capacity=capacity;observed_request=usm_gc_ai_mailbox.request;
 const u32 bytes=wanted_length<capacity?wanted_length:capacity;
 for(u32 i=0u;i<bytes;++i)output[i]=content;
 length=wanted_length;return serializer_ok;
}
bool recursive(char* output,u32 capacity,u32& length){recursive_result=ai_export::request_export(serialize);return serialize(output,capacity,length);}
void heartbeat(){usm_gc_ai_mailbox.host_frame=usm_gc_ai_mailbox.frame;}
u32 snapshot_hash(){u32 value=2166136261u;for(u32 i=0;i<sizeof(usm_gc_ai_mailbox.data);++i)value=(value^static_cast<u8>(usm_gc_ai_mailbox.data[i]))*16777619u;return value;}
void reset(){
 usm_gc_ai_mailbox.frame=1u;usm_gc_ai_mailbox.host_frame=0u;usm_gc_ai_mailbox.request=0u;
 usm_gc_ai_mailbox.acknowledged=0u;usm_gc_ai_mailbox.result=0u;usm_gc_ai_mailbox.length=0u;
 for(char& value:usm_gc_ai_mailbox.data)value=0;
 ai_export::request_sequence=0u;ai_export::pending_request=0u;ai_export::pending_frames=0u;ai_export::serializing=false;
 calls=0u;statuses=0u;serializer_ok=true;wanted_length=5u;content='A';status[0]='\0';
}
}
namespace usm::gc::menu {
void set_status(const char* text,u16){++statuses;u32 i=0u;for(;i+1u<sizeof(status)&&text[i];++i)status[i]=text[i];status[i]='\0';}
}
extern "C" int run_tests(){
 using namespace ai_export;
 check(sizeof(UsmAiMailbox)==65568u&&__builtin_offsetof(UsmAiMailbox,data)==32u,"mailbox has exact host layout and 65536-byte snapshot capacity");
 check(usm_gc_ai_mailbox.magic==0x55534D41u&&usm_gc_ai_mailbox.version==1u&&usm_gc_ai_mailbox.frame==1u,"mailbox starts with USMA version1 and nonzero frame");
 reset();check(!request_export(serialize)&&calls==0u&&usm_gc_ai_mailbox.request==0u,"offline export never serializes or publishes");
 reset();usm_gc_ai_mailbox.frame=300u;usm_gc_ai_mailbox.host_frame=1u;
 check(request_export(serialize),"heartbeat299 frames old is connected");
 reset();usm_gc_ai_mailbox.frame=301u;usm_gc_ai_mailbox.host_frame=1u;
 check(!request_export(serialize)&&calls==0u,"heartbeat300 frames old is disconnected");
 reset();usm_gc_ai_mailbox.host_frame=2u;
 check(!request_export(serialize)&&calls==0u,"future heartbeat cannot impersonate a fresh host");
 reset();heartbeat();check(!request_export(nullptr)&&usm_gc_ai_mailbox.request==0u,"missing serializer never publishes a request");
 serializer_ok=false;check(!request_export(serialize)&&usm_gc_ai_mailbox.request==0u&&pending_request==0u,"failed serializer does not arm pending work");
 serializer_ok=true;wanted_length=0u;check(!request_export(serialize)&&usm_gc_ai_mailbox.request==0u,"empty serialized data is rejected");
 wanted_length=65537u;check(!request_export(serialize)&&usm_gc_ai_mailbox.request==0u,"oversized reported data is rejected without publication");
 reset();heartbeat();usm_gc_ai_mailbox.result=2u;
 check(request_export(serialize)&&supplied_capacity==65536u&&calls==1u,"serializer receives full buffer capacity synchronously");
 check(observed_request==0u&&usm_gc_ai_mailbox.request==1u&&usm_gc_ai_mailbox.result==0u&&usm_gc_ai_mailbox.length==5u,"request is published last with exact length and pending result");
 check(usm_gc_ai_mailbox.data[0]=='A'&&usm_gc_ai_mailbox.data[4]=='A',"published snapshot contains actual serialized bytes");
 const u32 initial_hash=snapshot_hash();content='B';wanted_length=17u;
 check(!request_export(serialize)&&calls==1u&&usm_gc_ai_mailbox.length==5u&&snapshot_hash()==initial_hash,"busy export never calls serializer or mutates pending snapshot");
 heartbeat();frame_advance();check(pending_request==1u&&snapshot_hash()==initial_hash,"frame polling leaves pending snapshot immutable");
 const u32 before_stale=statuses;usm_gc_ai_mailbox.acknowledged=77u;usm_gc_ai_mailbox.result=1u;heartbeat();frame_advance();
 check(pending_request==1u&&statuses==before_stale,"unrelated successful acknowledgment cannot complete request");
 usm_gc_ai_mailbox.acknowledged=1u;usm_gc_ai_mailbox.result=0u;heartbeat();frame_advance();
 check(pending_request==1u&&statuses==before_stale,"matching acknowledgment with pending result waits");
 usm_gc_ai_mailbox.result=9u;heartbeat();frame_advance();check(pending_request==1u,"unknown completion code never reports a successful export");
 usm_gc_ai_mailbox.result=1u;heartbeat();frame_advance();
 check(pending_request==0u&&equal(status,"AI export saved")&&usm_gc_ai_mailbox.request==1u,"matching successful acknowledgment completes saved snapshot");
 const u32 after_success=statuses;frame_advance();check(statuses==after_success,"successful acknowledgment is reported only once");
 heartbeat();check(request_export(serialize)&&usm_gc_ai_mailbox.request==2u&&usm_gc_ai_mailbox.length==17u&&usm_gc_ai_mailbox.result==0u,"new snapshot gets fresh sequence and resets old result");
 check(observed_request==0u,"successful serializer after saved request observes retirement before touching snapshot bytes");
 usm_gc_ai_mailbox.acknowledged=1u;usm_gc_ai_mailbox.result=1u;heartbeat();frame_advance();check(pending_request==2u,"old successful acknowledgment cannot complete a later snapshot");
 usm_gc_ai_mailbox.acknowledged=2u;usm_gc_ai_mailbox.result=2u;heartbeat();frame_advance();
 check(pending_request==0u&&equal(status,"AI export failed; companion could not save file"),"matching failure releases the snapshot and reports file error");
 const u32 after_failure=statuses;frame_advance();check(statuses==after_failure,"failed acknowledgment is reported only once");
 heartbeat();serializer_ok=false;observed_request=99u;
 check(!request_export(serialize)&&observed_request==0u&&usm_gc_ai_mailbox.request==0u&&pending_request==0u,"serializer failure after companion error retires terminal request before buffer mutation");
 serializer_ok=true;heartbeat();request_export(serialize);const u32 canceled_request=usm_gc_ai_mailbox.request,canceled_hash=snapshot_hash();
 usm_gc_ai_mailbox.host_frame=0u;frame_advance();
 check(pending_request==0u&&usm_gc_ai_mailbox.request==0u&&equal(status,"AI export canceled; companion disconnected"),"disconnect retires pending mailbox request");
 check(snapshot_hash()==canceled_hash,"disconnect preserves serialized snapshot bytes");
 const u32 after_disconnect=statuses;usm_gc_ai_mailbox.acknowledged=canceled_request;usm_gc_ai_mailbox.result=1u;frame_advance();
 check(statuses==after_disconnect,"late acknowledgment after cancellation cannot report saved");
 heartbeat();check(request_export(serialize)&&usm_gc_ai_mailbox.request==canceled_request+1u,"reconnect uses a new sequence despite request retirement");
 reset();heartbeat();request_export(serialize);const u32 timeout_hash=snapshot_hash();
 for(u32 i=0u;i<599u;++i){heartbeat();frame_advance();}
 check(pending_request!=0u&&usm_gc_ai_mailbox.request!=0u,"healthy companion keeps snapshot pending through599 frames");
 heartbeat();frame_advance();check(pending_request==0u&&usm_gc_ai_mailbox.request==0u&&equal(status,"AI export timed out; snapshot canceled"),"healthy but unresponsive companion times out exactly at600 frames");
 check(snapshot_hash()==timeout_hash,"timeout preserves pending serialized bytes");
 reset();usm_gc_ai_mailbox.frame=0xFFFFFFFFu;frame_advance();check(usm_gc_ai_mailbox.frame==1u,"frame counter wrap skips zero");
 heartbeat();request_sequence=0xFFFFFFFFu;usm_gc_ai_mailbox.acknowledged=1u;usm_gc_ai_mailbox.result=1u;
 check(request_export(serialize)&&usm_gc_ai_mailbox.request==1u&&usm_gc_ai_mailbox.result==0u,"private sequence wrap skips zero and clears stale matching result");
 heartbeat();frame_advance();check(pending_request==1u,"wrapped request cannot consume old matching acknowledgment without new result");
 reset();heartbeat();wanted_length=65536u;content='Z';
 check(request_export(serialize)&&usm_gc_ai_mailbox.length==65536u&&usm_gc_ai_mailbox.data[65535]=='Z',"exact-capacity snapshot is valid without writing a terminator beyond buffer");
 reset();heartbeat();recursive_result=true;check(request_export(recursive)&&!recursive_result&&calls==1u&&usm_gc_ai_mailbox.request==1u,"serializer cannot recursively replace its in-progress snapshot");
 char digits[16];u32 length=0u,number=checks;do{digits[length++]=static_cast<char>('0'+number%10u);number/=10u;}while(number);
 print(failures==0u?"PASS: ":"FAILED: ");while(length){char digit[2]={digits[--length],'\0'};print(digit);}print(" AI export mailbox checks\n");return failures==0u?0:1;
}
asm(".global _start\n_start:\n call run_tests\n mov %eax, %ebx\n mov $1, %eax\n int $0x80\n");
