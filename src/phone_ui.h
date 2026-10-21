#pragma once
#include "render.h"

enum class PhoneAction { NoAction, Back, Room, Hand, Recenter, Smaller, Larger,
                         TurnLeft, TurnRight, Keyboard, Hide, Reset, Dimmer, Brighter, Home, BrightnessSlider, Dock };
struct PhoneButton {
 int x,y,w,h; PhoneAction action; const char* label;
 bool contains(int px,int py)const{return px>=x&&px<x+w&&py>=y&&py<y+h;}
};
// These rectangles use the same 480 x 1046 coordinates as the live game.
const std::array<PhoneButton,14> phoneButtons{{
 {24,112,AndroidOnly?208:432,72,PhoneAction::Back,AndroidOnly?"Resume app":"Back to pet"},
#ifdef ANDROID_PHONE
 {248,112,208,72,PhoneAction::Home,"Home screen"},
#endif
 {24,310,208,80,PhoneAction::Room,"In the room"},
 {248,310,208,80,PhoneAction::Hand,"Left hand"},
 {24,406,208,72,PhoneAction::Dock,"Return to dock"},
 {248,406,208,72,PhoneAction::Recenter,"Bring closer"},
 {24,536,208,76,PhoneAction::Smaller,"−  Smaller"},
 {248,536,208,76,PhoneAction::Larger,"+  Larger"},
 {24,632,208,72,PhoneAction::TurnLeft,"Turn left"},
 {248,632,208,72,PhoneAction::TurnRight,"Turn right"},
#ifdef ANDROID_PHONE
 {24,750,432,64,PhoneAction::BrightnessSlider,"Brightness"},
#else
 {24,750,208,64,PhoneAction::Dimmer,"−  Dimmer"},
 {248,750,208,64,PhoneAction::Brighter,"+  Brighter"},
#endif
 {24,832,208,64,PhoneAction::Keyboard,"VR keyboard"},
 {248,832,208,64,PhoneAction::Hide,"Hide phone"},
 {24,914,432,58,PhoneAction::Reset,"Reset position & size"}
}};
float phoneBrightnessAt(int x){return .25f+.75f*std::clamp((x-48)/384.f,0.f,1.f);}
PhoneAction phoneActionAt(int x,int y){
 for(const auto&b:phoneButtons)if(b.contains(x,y))return b.action;
 return PhoneAction::NoAction;
}
void rounded(Canvas&c,double x,double y,double w,double h,double radius,
             double r,double g,double b){
 auto*cr=c.c;double pi=3.141592653589793;
 cairo_new_sub_path(cr);cairo_arc(cr,x+w-radius,y+radius,radius,-pi/2,0);
 cairo_arc(cr,x+w-radius,y+h-radius,radius,0,pi/2);
 cairo_arc(cr,x+radius,y+h-radius,radius,pi/2,pi);
 cairo_arc(cr,x+radius,y+radius,radius,pi,3*pi/2);cairo_close_path(cr);
 cairo_set_source_rgb(cr,r,g,b);cairo_fill(cr);
}
void centered(Canvas&c,const PhoneButton&b,double size,double r,double g,double blue){
 cairo_select_font_face(c.c,"sans",CAIRO_FONT_SLANT_NORMAL,CAIRO_FONT_WEIGHT_NORMAL);
 cairo_set_font_size(c.c,size);cairo_text_extents_t ext;cairo_text_extents(c.c,b.label,&ext);
 c.text(b.x+(b.w-ext.width)/2-ext.x_bearing,b.y+(b.h-ext.height)/2-ext.y_bearing,b.label,size,r,g,blue);
}
void drawPhoneSettings(Canvas&c,bool wrist,float size,bool connected,bool protectedCare,float brightness=1.f,bool docked=false){
 c.fill(.035,.048,.065);
 c.text(24,33,AndroidOnly?"ANDROID PHONE":"DIGITAL PARTNER",19,.96,.57,.19);
 c.text(24,85,"Device settings",43);
 rounded(c,24,202,432,66,12,.075,.105,.13);
 c.text(40,229,connected?(AndroidOnly?"Android keeps running":"Your pet keeps running"):(AndroidOnly?"Connecting to Android...":"Connecting to your pet..."),23,.81,.9,.94);
 c.text(40,253,AndroidOnly?"Point + stick: scroll  ·  press: click":(protectedCare?"No-death on  ·  Music off":"Music off"),19,.58,.71,.76);
 c.text(24,296,"PLACEMENT",19,.58,.68,.73);
 c.text(24,520,"PHONE SIZE",19,.58,.68,.73);
 c.text(356,520,std::to_string((int)std::round(size*100))+"%",23,.96,.62,.29);
 c.text(24,735,"SCREEN BRIGHTNESS",19,.58,.68,.73);
 c.text(375,735,std::to_string((int)std::round(brightness*100))+"%",23,.96,.62,.29);
 for(const auto&b:phoneButtons){
  if(b.action==PhoneAction::NoAction)continue;
  if(b.action==PhoneAction::BrightnessSlider){
   float progress=std::clamp((brightness-.25f)/.75f,0.f,1.f);
   rounded(c,24,750,432,64,14,.09,.13,.17);
   rounded(c,48,778,384,8,4,.24,.30,.34);
   if(progress>.01f)rounded(c,48,778,384*progress,8,4,.96,.56,.20);
   cairo_arc(c.c,48+384*progress,782,17,0,6.283185307);
   cairo_set_source_rgb(c.c,.98,.68,.34);cairo_fill(c.c);continue;
  }
  bool selected=(b.action==PhoneAction::Room&&!wrist&&!docked)||(b.action==PhoneAction::Hand&&wrist)||(b.action==PhoneAction::Dock&&docked);
  bool back=b.action==PhoneAction::Back||b.action==PhoneAction::Home,reset=b.action==PhoneAction::Reset;
  if(back)rounded(c,b.x,b.y,b.w,b.h,14,.96,.56,.20);
  else if(selected)rounded(c,b.x,b.y,b.w,b.h,14,.19,.25,.29);
  else if(!reset)rounded(c,b.x,b.y,b.w,b.h,14,.09,.13,.17);
  if(selected)rounded(c,b.x+14,b.y+31,5,18,2,.98,.60,.22);
  centered(c,b,reset?22:(b.action==PhoneAction::Dock?24:26),back?.07:.91,back?.08:.95,back?.09:.97);
 }
 c.text(31,1017,AndroidOnly?"Bottom bezel button: settings":"Center button: settings",23,.58,.68,.73);
}
