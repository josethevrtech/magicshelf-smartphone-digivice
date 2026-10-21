#pragma once
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XTest.h>
#include <chrono>
#include <string>
struct GameInput {
 Display*d=nullptr;bool pressed=false,continuous=false;int width=480,height=1046;std::chrono::steady_clock::time_point release;
 GameInput(std::string display,bool drag=false):continuous(drag){if(display.size()<2||display[0]!=':'||atoi(display.c_str()+1)<=0)throw std::runtime_error("Refusing input outside a private display");d=XOpenDisplay(display.c_str());if(!d)throw std::runtime_error("Private display not ready");width=DisplayWidth(d,DefaultScreen(d));height=DisplayHeight(d,DefaultScreen(d));}
 ~GameInput(){if(d){if(pressed)XTestFakeButtonEvent(d,1,False,CurrentTime);XFlush(d);XCloseDisplay(d);}}
 void move(int x,int y){if(pressed&&!continuous)return;XTestFakeMotionEvent(d,-1,std::clamp(x,0,width-1),std::clamp(y,0,height-1),CurrentTime);XFlush(d);if(pressed&&continuous)release=std::chrono::steady_clock::now()+std::chrono::seconds(5);}
 void tap(int x,int y){if(pressed)return;move(x,y);XTestFakeButtonEvent(d,1,True,CurrentTime);XFlush(d);pressed=true;release=std::chrono::steady_clock::now()+std::chrono::milliseconds(65);}
 void begin(int x,int y){if(pressed)return;move(x,y);XTestFakeButtonEvent(d,1,True,CurrentTime);XFlush(d);pressed=true;release=std::chrono::steady_clock::now()+std::chrono::seconds(5);}
 void end(){if(pressed){XTestFakeButtonEvent(d,1,False,CurrentTime);XFlush(d);pressed=false;}}
 void scroll(int horizontal,int vertical){
  end();
  auto wheel=[&](int count,int positive,int negative){for(int n=0;n<std::min(12,std::abs(count));n++){int button=count>0?positive:negative;XTestFakeButtonEvent(d,button,True,CurrentTime);XTestFakeButtonEvent(d,button,False,CurrentTime);}};
  wheel(vertical,4,5);wheel(horizontal,7,6);XFlush(d);
 }
 void tick(){if(pressed&&std::chrono::steady_clock::now()>=release){XTestFakeButtonEvent(d,1,False,CurrentTime);XFlush(d);pressed=false;}}
 void text(std::string text){for(unsigned char c:text){KeySym sym=c==8?XK_BackSpace:c==10||c==13?XK_Return:c;KeyCode code=XKeysymToKeycode(d,sym);if(!code||c>126)continue;bool shift=XkbKeycodeToKeysym(d,code,0,0)!=sym;auto shiftcode=XKeysymToKeycode(d,XK_Shift_L);if(shift)XTestFakeKeyEvent(d,shiftcode,True,CurrentTime);XTestFakeKeyEvent(d,code,True,CurrentTime);XTestFakeKeyEvent(d,code,False,CurrentTime);if(shift)XTestFakeKeyEvent(d,shiftcode,False,CurrentTime);}XFlush(d);}
};
