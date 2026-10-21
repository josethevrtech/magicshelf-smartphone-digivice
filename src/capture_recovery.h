#pragma once
#include <chrono>
#include <iostream>
#include <memory>

// Gamescope can leave an initial PipeWire connection streaming without frames.
// Keep that consumer alive until a replacement has a frame: removing the last
// consumer can stop some gamescope capture sources from producing altogether.
template<class Stream> struct CaptureRecovery {
 using Clock=std::chrono::steady_clock;
 using Time=Clock::time_point;
 static constexpr auto Timeout=std::chrono::seconds(12);
 Stream* watched=nullptr;
 Time nextAttempt{};
 std::unique_ptr<Stream> candidate;

 void reset(){candidate.reset();watched=nullptr;nextAttempt={};}
 void poll(std::unique_ptr<Stream>& active,Time now=Clock::now()){
  if(!active){reset();return;}
  if(watched!=active.get()){
   reset();watched=active.get();nextAttempt=now+Timeout;
  }
  active->poll();
  if(active->frames||active->failed){candidate.reset();return;}
  if(!candidate&&now>=nextAttempt){
   nextAttempt=now+Timeout;
   try{
    candidate=std::make_unique<Stream>(active->targetNode,active->expectedWidth,active->expectedHeight,active->targetSerial);
    std::cerr<<"Capture waiting for first frame; opening a replacement connection\n";
   }catch(const std::exception& e){std::cerr<<"Capture retry: "<<e.what()<<"\n";}
  }
  if(!candidate)return;
  candidate->poll();
  if(candidate->frames&&!candidate->failed){
   active.swap(candidate);candidate.reset();watched=active.get();
   std::cerr<<"Capture recovered its first frame\n";
  }else if(candidate->failed||now>=nextAttempt){
   candidate.reset();nextAttempt=now+Timeout;
  }
 }
};
