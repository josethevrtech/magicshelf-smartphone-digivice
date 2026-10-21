#pragma once
#include "input.h"
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctime>
#include <vector>
#include <iostream>
// Lepton exposes a touch FIFO inside this Android container. Touch gestures
// reach Android directly, without being interpreted as desktop mouse drags.
struct AndroidTouch {
 GameInput keyboard;int fd=-1,pid=0,width=720,height=1600,serial=100;
 bool pressed=false,scrolling=false,autoRelease=false;float sx=0,sy=0;
 std::chrono::steady_clock::time_point deadline;
 explicit AndroidTouch(const std::string&display):keyboard(display,true){width=keyboard.width;height=keyboard.height;}
 ~AndroidTouch(){end();if(fd>=0)close(fd);}
 void configure(int newPid){
  if(newPid<=1)return;if(newPid!=pid){end();if(fd>=0)close(fd);fd=-1;pid=newPid;}
  if(fd>=0)return;
  std::string path="/proc/"+std::to_string(pid)+"/root/dev/input/wl_touch_events";
  fd=open(path.c_str(),O_WRONLY|O_NONBLOCK|O_CLOEXEC);if(fd<0)return;
  struct stat st{};if(fstat(fd,&st)||!S_ISFIFO(st.st_mode)){close(fd);fd=-1;return;}
  send({{EV_ABS,ABS_MT_SLOT,0},{EV_ABS,ABS_MT_PRESSURE,0},{EV_ABS,ABS_MT_TRACKING_ID,-1}});
  std::cerr<<"Android touchscreen connected to container "<<pid<<"\n";
 }
 struct Event{unsigned short type,code;int value;};
 bool send(std::initializer_list<Event> values){
  if(fd<0)return false;timespec now{};clock_gettime(CLOCK_MONOTONIC,&now);std::vector<input_event> events;
  for(auto value:values){input_event e{};e.time.tv_sec=now.tv_sec;e.time.tv_usec=now.tv_nsec/1000;e.type=value.type;e.code=value.code;e.value=value.value;events.push_back(e);}
  input_event sync{};sync.time=events.back().time;sync.type=EV_SYN;sync.code=SYN_REPORT;events.push_back(sync);
  bool ok=write(fd,events.data(),events.size()*sizeof(input_event))==(ssize_t)(events.size()*sizeof(input_event));
  if(!ok){close(fd);fd=-1;pressed=scrolling=autoRelease=false;}return ok;
 }
 void begin(int x,int y){
  if(pressed)return;x=std::clamp(x,0,width-1);y=std::clamp(y,0,height-1);sx=x;sy=y;
  pressed=send({{EV_ABS,ABS_MT_SLOT,0},{EV_ABS,ABS_MT_TRACKING_ID,++serial},{EV_ABS,ABS_MT_POSITION_X,x},{EV_ABS,ABS_MT_POSITION_Y,y},{EV_ABS,ABS_MT_PRESSURE,255}});
  deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);autoRelease=false;
 }
 void move(int x,int y){if(!pressed||scrolling)return;position(x,y);}
 void position(int x,int y){
  x=std::clamp(x,0,width-1);y=std::clamp(y,0,height-1);
  send({{EV_ABS,ABS_MT_SLOT,0},{EV_ABS,ABS_MT_POSITION_X,x},{EV_ABS,ABS_MT_POSITION_Y,y}});
  deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
 }
 // Android 11 marks a slot active on any pressure event. Tracking ID -1 must
 // be last, or the released finger becomes a lingering hover pointer.
 void end(){if(pressed)send({{EV_ABS,ABS_MT_SLOT,0},{EV_ABS,ABS_MT_PRESSURE,0},{EV_ABS,ABS_MT_TRACKING_ID,-1}});pressed=scrolling=autoRelease=false;}
 void tap(int x,int y){if(pressed)return;begin(x,y);autoRelease=true;deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(65);}
 void tick(){if(pressed&&std::chrono::steady_clock::now()>=deadline)end();}
 void text(std::string value){keyboard.text(value);}
 void stopScroll(){if(scrolling)end();}
 void stickScroll(float x,float y,float dt,int anchorX,int anchorY){
  if(x==0&&y==0){stopScroll();return;}
  if(pressed&&!scrolling)return;
  if(!pressed){begin(std::clamp(anchorX,100,width-100),std::clamp(anchorY,height/3,2*height/3));scrolling=pressed;}
  if(!scrolling)return;
  sx-=x*900*std::clamp(dt,0.f,.05f);sy+=y*1100*std::clamp(dt,0.f,.05f);
  position((int)sx,(int)sy);
  // Re-anchor a long scroll before touching Android's status/navigation bars.
  if(sx<65||sx>width-65||sy<120||sy>height-120)end();
 }
};
