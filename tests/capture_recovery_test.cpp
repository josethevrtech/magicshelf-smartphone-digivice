#include "capture_recovery.h"
#include <cassert>
#include <cstdint>
#include <set>

struct FakeStream {
 static inline std::set<FakeStream*> alive;
 static inline bool dropLast=false;
 uint32_t targetNode,expectedWidth,expectedHeight;
 uint64_t targetSerial,frames=0;
 bool failed=false;
 FakeStream(uint32_t node,uint32_t width,uint32_t height,uint64_t serial):
  targetNode(node),expectedWidth(width),expectedHeight(height),targetSerial(serial){alive.insert(this);}
 ~FakeStream(){alive.erase(this);if(alive.empty())dropLast=true;}
 void poll(){}
};

int main(){
 using Recovery=CaptureRecovery<FakeStream>;
 auto time=Recovery::Time{};
 auto active=std::make_unique<FakeStream>(188,480,1046,3657);
 Recovery recovery;
 recovery.poll(active,time);
 recovery.poll(active,time+std::chrono::seconds(11));
 assert(!recovery.candidate&&FakeStream::alive.size()==1);
 recovery.poll(active,time+std::chrono::seconds(12));
 auto* original=active.get();auto* candidate=recovery.candidate.get();
 assert(candidate&&FakeStream::alive.size()==2&&active.get()==original);
 assert(candidate->targetSerial==3657&&candidate->expectedWidth==480&&candidate->expectedHeight==1046);
 candidate->frames=1;
 recovery.poll(active,time+std::chrono::seconds(13));
 assert(active.get()==candidate&&!recovery.candidate&&!FakeStream::dropLast);
 assert(FakeStream::alive.size()==1);
 recovery.poll(active,time+std::chrono::seconds(90));
 assert(!recovery.candidate); // A healthy stream is never replaced.

 recovery.reset();active->frames=0;
 recovery.poll(active,time);
 recovery.poll(active,time+std::chrono::seconds(12));
 recovery.candidate->failed=true;
 recovery.poll(active,time+std::chrono::seconds(13));
 assert(!recovery.candidate&&!FakeStream::dropLast);
 recovery.poll(active,time+std::chrono::seconds(24));
 assert(!recovery.candidate);
 recovery.poll(active,time+std::chrono::seconds(25));
 assert(recovery.candidate);
 active->frames=1; // Original starts producing while replacement is warming.
 recovery.poll(active,time+std::chrono::seconds(26));
 assert(!recovery.candidate&&!FakeStream::dropLast);
 recovery.reset();active.reset();
 assert(FakeStream::alive.empty());
 std::cout<<"Initial capture recovery retains a live consumer and retries safely\n";
}
