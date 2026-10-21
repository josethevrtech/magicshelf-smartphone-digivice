#pragma once
#include "openvr.h"
#include <string>
#include <iostream>
#include <cmath>
#include <algorithm>
struct ControllerNav {
 vr::VRActionSetHandle_t set=0;vr::VRActionHandle_t scroll=0,click=0,back=0,home=0;vr::VRInputValueHandle_t hand[2]{};
 bool available=false,wasFocused=false,wasGrabbing=false,waitForNeutral=false;
 float wheelX=0,wheelY=0;
 explicit ControllerNav(const std::string&base){
  auto*i=vr::VRInput();auto error=i->SetActionManifestPath((base+"/assets/input/actions.json").c_str());
  if(error){std::cerr<<"Phone action manifest error "<<error<<"\n";return;}
  error=i->GetActionSetHandle("/actions/phone",&set);
  error=(vr::EVRInputError)(error|i->GetActionHandle("/actions/phone/in/scroll",&scroll)|i->GetActionHandle("/actions/phone/in/click",&click)|i->GetActionHandle("/actions/phone/in/back",&back)|i->GetActionHandle("/actions/phone/in/home",&home));
  i->GetInputSourceHandle("/user/hand/left",&hand[0]);i->GetInputSourceHandle("/user/hand/right",&hand[1]);
  available=error==vr::VRInputError_None;std::cerr<<"Phone controller actions ready="<<available<<"\n";
 }
 struct Result{float scrollX=0,scrollY=0;int x=0,y=0;bool click=false,back=false,home=false,axisAvailable=false;};
 static float deflection(float value){return std::abs(value)<=.25f?0:std::copysign((std::min(std::abs(value),1.f)-.25f)/.75f,value);}
 Result update(bool focused,int role,float dt,bool grabbing=false){
  Result out;if(!available)return out;auto*i=vr::VRInput();vr::VRActiveActionSet_t active{};active.ulActionSet=set;
  if(wasGrabbing&&!grabbing)waitForNeutral=true;
  wasGrabbing=grabbing;
  focused=focused&&role>=1&&role<=2;
  active.ulRestrictedToDevice=focused?hand[role-1]:vr::k_ulInvalidInputValueHandle;
  active.nPriority=vr::k_nActionSetOverlayGlobalPriorityMin+1;
  auto error=i->UpdateActionState(focused?&active:nullptr,sizeof(active),focused?1:0);
  if(error||!focused){wasFocused=false;wheelX=wheelY=0;return out;}
  auto pressed=[&](vr::VRActionHandle_t action){vr::InputDigitalActionData_t data{};return i->GetDigitalActionData(action,&data,sizeof(data),active.ulRestrictedToDevice)==vr::VRInputError_None&&wasFocused&&data.bActive&&data.bChanged&&data.bState;};
  if(!grabbing){out.click=pressed(click);out.back=pressed(back);out.home=pressed(home);}
  vr::InputAnalogActionData_t axis{};
  if(i->GetAnalogActionData(scroll,&axis,sizeof(axis),active.ulRestrictedToDevice)==vr::VRInputError_None&&axis.bActive){
   out.axisAvailable=true;
   if(!grabbing&&waitForNeutral){if(std::abs(axis.x)<=.25f&&std::abs(axis.y)<=.25f)waitForNeutral=false;wasFocused=true;return out;}
   out.scrollX=deflection(axis.x);out.scrollY=deflection(axis.y);
   if(grabbing){wheelX=wheelY=0;wasFocused=true;return out;}
   wheelX+=out.scrollX*10*std::clamp(dt,0.f,.1f);wheelY+=out.scrollY*10*std::clamp(dt,0.f,.1f);
   out.x=(int)wheelX;out.y=(int)wheelY;wheelX-=out.x;wheelY-=out.y;
  }
  wasFocused=true;return out;
 }
};
