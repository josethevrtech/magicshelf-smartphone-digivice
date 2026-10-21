#include "magicshelf_sdk.h"
#include "render.h"
#include "openvr.h"
#include "capture.h"
#include "capture_recovery.h"
#include "input.h"
#include "placement.h"
#include "resume.h"
#include "pet_text_input.h"
#include "phone_ui.h"
#include "android_bridge.h"
#include "android_touch.h"
#include "controller_nav.h"
#include "object_dock.h"
#include <json-c/json.h>
#include <filesystem>
#include <chrono>
#include <thread>
#include <memory>
#include <csignal>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
namespace fs=std::filesystem;using namespace vr;
static volatile sig_atomic_t running=1,showRequested=0,dockRequested=0,hideRequested=0;
static const char*key=ApplicationKey;
void stop(int){running=0;}void requestShow(int){showRequested=1;}void requestDock(int){dockRequested=1;}void requestHide(int){hideRequested=1;}
void check(EVROverlayError e,const char*op){if(e)throw std::runtime_error(std::string(op)+": "+VROverlay()->GetOverlayErrorNameFromEnum(e));}
M fromvr(HmdMatrix34_t p){M r=identity();for(int i=0;i<3;i++)for(int j=0;j<4;j++)r[i+j*4]=p.m[i][j];return r;}
HmdMatrix34_t tovr(M p){HmdMatrix34_t r{};for(int i=0;i<3;i++)for(int j=0;j<4;j++)r.m[i][j]=p[i+j*4];return r;}
std::string readstr(json_object*j,const char*k,std::string def=""){json_object*v;return j&&json_object_object_get_ex(j,k,&v)?json_object_get_string(v):def;}
double readnum(json_object*j,const char*k,double def=0){json_object*v;return j&&json_object_object_get_ex(j,k,&v)?json_object_get_double(v):def;}
int main(int argc,char**argv){try{
 std::string base=std::getenv("MAGICSHELF_DATA_DIR")?std::getenv("MAGICSHELF_DATA_DIR"):fs::canonical("/proc/self/exe").parent_path().string();
 int lock=open((base+"/"+LockFile).c_str(),O_CREAT|O_RDWR,0600);if(lock<0)throw std::runtime_error("Cannot open lock");
 bool held=flock(lock,LOCK_EX|LOCK_NB)!=0;bool query=argc>1&&std::string(argv[1])=="--status";
 if(held){char b[40]={};pread(lock,b,39,0);pid_t pid=atoi(b);bool ok=pid>1&&kill(pid,0)==0;if(!query&&ok)kill(pid,argc>1&&std::string(argv[1])=="--hide"?SIGRTMIN:(argc>1&&std::string(argv[1])=="--dock"?SIGUSR2:SIGUSR1));close(lock);return ok?0:1;}
 if(argc>1){close(lock);return 1;}
 std::signal(SIGPIPE,SIG_IGN);std::signal(SIGUSR1,requestShow);std::signal(SIGUSR2,requestDock);std::signal(SIGRTMIN,requestHide);std::signal(SIGTERM,stop);std::signal(SIGINT,stop);
 EVRInitError err;VR_Init(&err,VRApplication_Background);if(err)throw std::runtime_error("Start SteamVR normally first.");VR_Shutdown();
 VR_Init(&err,VRApplication_Overlay);if(err)throw std::runtime_error(VR_GetVRInitErrorAsEnglishDescription(err));
 bool managed=std::getenv("MAGICSHELF_SHELF_OWNER")&&std::string(std::getenv("MAGICSHELF_SHELF_OWNER"))=="0";
 auto app=VRApplications();auto manifest=base+"/"+ManifestFile;
 if(managed){app->RemoveApplicationManifest(manifest.c_str());}else{
 auto ae=app->AddApplicationManifest(manifest.c_str(),false);if(ae)throw std::runtime_error(app->GetApplicationsErrorNameFromEnum(ae));app->SetApplicationAutoLaunch(key,false);ae=app->IdentifyApplication(getpid(),key);if(ae)throw std::runtime_error(app->GetApplicationsErrorNameFromEnum(ae));}
 auto ov=VROverlay();VROverlayHandle_t dash=0,thumb=0,device=0;
 if(!managed){check(ov->CreateDashboardOverlay(key,AppTitle,&dash,&thumb),"dashboard");check(ov->SetOverlayInputMethod(dash,VROverlayInputMethod_None),"launcher input");check(ov->SetOverlayFromFile(thumb,(base+"/"+IconFile).c_str()),"icon");}
 check(ov->CreateOverlay(std::getenv("MAGICSHELF_MODULE_KEY")?std::getenv("MAGICSHELF_MODULE_KEY"):DeviceKey,AppTitle,&device),"device");
 check(ov->SetOverlayFlag(device,VROverlayFlags_SideBySide_Parallel,true),"stereo");
 check(ov->SetOverlayFlag(device,VROverlayFlags_VisibleInDashboard,true),"visible in dashboard");
 check(ov->SetOverlayFlag(device,VROverlayFlags_MakeOverlaysInteractiveIfVisible,true),"interactive device");
 check(ov->SetOverlayFlag(device,VROverlayFlags_EnableClickStabilization,true),"device click stabilization");
 check(ov->SetOverlayInputMethod(device,VROverlayInputMethod_Mouse),"device input");HmdVector2_t ms{{512,512}};check(ov->SetOverlayMouseScale(device,&ms),"device mouse scale");
 auto navigation=std::make_unique<ControllerNav>(base);
 bool grabDebug=fs::exists(base+"/grab-debug");auto lastGrabLog=std::chrono::steady_clock::now();
 check(ov->SetOverlayFlag(device,VROverlayFlags_SendVRSmoothScrollEvents,true),"grab scroll");
 if(AndroidOnly&&(!navigation||!navigation->available))check(ov->SetOverlayFlag(device,VROverlayFlags_SendVRDiscreteScrollEvents,true),"scroll fallback");
 Renderer renderer(base);ObjectDock dock(ov,base);
 unsigned char transparent[4]={0,0,0,0};if(dash)check(ov->SetOverlayRaw(dash,transparent,1,1,4),"transparent launcher");
 Canvas ui(SettingsWidth,SettingsHeight),loading(256,256);loading.fill(.06,.1,.13);loading.text(20,115,AndroidOnly?"Starting Android...":"Starting game...",22);renderer.screen(loading);
 const bool androidOpen=AndroidOnly;bool androidHasFrame=false,settingsOpen=false,recallRequested=false,haveWristPose=false;M wristFallback=identity();
 bool visible=true,wrist=false,docked=false,placed=false,dirty=true,ready=false;float size=DefaultSize,yaw=0,brightness=.7f;int mode=0;M world=identity(),lastModel=identity(),lastPlane=identity();float lastSize=1,lastWidth=PhonePlaneWidth;RoomGrab grab;ShakeToDock shake;bool dockCandidate=false,returnedHome=false;
 auto*settings=json_object_from_file((base+"/"+SettingsFile).c_str());if(settings){brightness=std::clamp((float)readnum(settings,"brightness",.7),.25f,1.f);visible=readnum(settings,"visible",1);wrist=readnum(settings,"wrist",0);docked=readnum(settings,"docked",0);if(docked)wrist=false;size=std::clamp((float)readnum(settings,"size",DefaultSize),.6f,3.f);yaw=readnum(settings,"yaw",0);mode=std::clamp((int)readnum(settings,"mode",0),0,2);
  json_object* savedPose=nullptr;if(json_object_object_get_ex(settings,"room_pose",&savedPose)&&json_object_is_type(savedPose,json_type_array)&&json_object_array_length(savedPose)==16){M restored{};for(int i=0;i<16;i++)restored[i]=json_object_get_double(json_object_array_get_idx(savedPose,i));if(validRoomPose(restored)){world=restored;placed=true;std::cerr<<"Restored user room placement\n";}}
  json_object_put(settings);}
 auto save=[&](){auto*j=json_object_new_object();json_object_object_add(j,"app",json_object_new_string(androidOpen?"android":"pet"));json_object_object_add(j,"brightness",json_object_new_double(brightness));json_object_object_add(j,"visible",json_object_new_boolean(visible));json_object_object_add(j,"wrist",json_object_new_boolean(wrist));json_object_object_add(j,"docked",json_object_new_boolean(docked));json_object_object_add(j,"size",json_object_new_double(size));json_object_object_add(j,"yaw",json_object_new_double(yaw));json_object_object_add(j,"mode",json_object_new_int(mode));if(placed&&validRoomPose(world)){auto*a=json_object_new_array();for(float v:world)json_object_array_add(a,json_object_new_double(v));json_object_object_add(j,"room_pose",a);}json_object_to_file((base+"/"+SettingsFile+".tmp").c_str(),j);json_object_put(j);fs::rename(base+"/"+SettingsFile+".tmp",base+"/"+SettingsFile);};
 auto recenter=[&](M head){grab.end();docked=false;V p=point(head,{-.12f,-.10f,-.60f}),h=point(head,{0,0,0});V z=norm(sub(h,p)),x=norm(cross({0,1,0},z)),y=cross(z,x);world=pose(x,y,z,p);placed=true;};
 bool keyboardOwned=false;PetKeyboardRequest petKeyboard;
 auto closeKeyboard=[&](){petKeyboard.cancel();if(keyboardOwned){ov->HideKeyboard();keyboardOwned=false;}};
 auto keyboard=[&](){
  if(keyboardOwned)return;
  auto result=ov->ShowKeyboardForOverlay(device,k_EGamepadTextInputModeNormal,k_EGamepadTextInputLineModeSingleLine,KeyboardFlag_Minimal|KeyboardFlag_Modal,AppTitle,256,"",0);
  if(result!=VROverlayError_None){std::cerr<<"Keyboard unavailable: "<<ov->GetOverlayErrorNameFromEnum(result)<<"\n";return;}
  keyboardOwned=true;std::cerr<<"Headset keyboard opened\n";
 };
 CaptureRecovery<Capture> captureRecovery;AutoResume autoresume(base);std::unique_ptr<Capture>capture;std::unique_ptr<GameInput>input;uint32_t node=0;uint64_t uploaded=0;double started=0;bool hasFrame=false;auto lastSource=std::chrono::steady_clock::now()-std::chrono::seconds(5);
 std::unique_ptr<Capture>androidCapture;std::unique_ptr<AndroidTouch>androidInput;uint32_t androidNode=0;uint64_t androidUploaded=0;double androidStarted=0;
 auto lastUI=lastSource;auto lastSettingsTap=lastSource;
 auto lastControlsPoll=lastSource,lastBrightnessCommand=lastSource,lastNavigation=std::chrono::steady_clock::now();double controlsRevision=0;
 bool pointerOnScreen=false,pointerOnHandle=false,queuedClick=false,brightnessDragging=false;VREvent_t lastPointer{};
 uint32_t pointerController=k_unTrackedDeviceIndexInvalid;int pointerX=0,pointerY=0;
 auto finishBrightness=[&](){if(!brightnessDragging)return;brightnessDragging=false;lastBrightnessCommand=std::chrono::steady_clock::now();save();androidCommand("{\"op\":\"brightness\",\"value\":"+std::to_string(brightness)+"}");};
 auto toggleSettings=[&](bool open){closeKeyboard();finishBrightness();if(androidInput)androidInput->end();settingsOpen=open;dirty=true;if(!open){uploaded=0;androidUploaded=0;}};
 auto openFromLauncher=[&](){docked=false;visible=true;toggleSettings(true);recallRequested=true;};
 auto returnToDock=[&](const char*reason="command"){
  if(dock.slot<0)return;
  grab.end();shake.reset();dockCandidate=false;returnedHome=true;finishBrightness();
  if(androidInput)androidInput->end();if(input)input->end();
  docked=true;visible=true;wrist=false;placed=false;world=identity();recallRequested=false;
  pointerOnScreen=pointerOnHandle=false;pointerController=k_unTrackedDeviceIndexInvalid;queuedClick=false;
  toggleSettings(false);save();std::cerr<<"Returned to dock slot "<<dock.slot<<" reason="<<reason<<"\n";
 };
 auto releaseGrab=[&](){
  if(dock.canSnap(mm(world,yawPose(yaw)),size,dockCandidate)){ov->TriggerLaserMouseHapticVibration(device,.06f,100,.4f);returnToDock("drop");}
  else{grab.end();shake.reset();dockCandidate=false;save();std::cerr<<"Room placement saved\n";}
 };
 auto resetPlacement=[&](uint32_t type){if(visible&&dock.slot>=0){std::cerr<<"Placement reset event "<<type<<"\n";returnToDock("VR reset");}};
 while(running){auto now=std::chrono::steady_clock::now();returnedHome=false;if(hideRequested){hideRequested=0;visible=false;grab.end();recallRequested=false;pointerOnScreen=false;queuedClick=false;toggleSettings(false);save();std::cerr<<"Hidden by MagicShelf; capture kept alive\n";}if(showRequested){showRequested=0;openFromLauncher();}if(dockRequested){dockRequested=0;returnToDock();}
  if(now-lastSource>std::chrono::seconds(2)){auto*j=AndroidOnly?nullptr:json_object_from_file((base+"/private-source.json").c_str());if(j){uint32_t n=readnum(j,"node");double st=readnum(j,"started");if(n&&(n!=node||st!=started||!capture||capture->failed)){try{captureRecovery.reset();capture.reset();input.reset();capture=std::make_unique<Capture>(n,480,1046,(uint64_t)readnum(j,"serial"));input=std::make_unique<GameInput>(readstr(j,"display"));node=n;started=st;autoresume.reset(readnum(j,"autoresume",0),readnum(j,"resume_slot",1));uploaded=0;hasFrame=false;std::cerr<<"Connected private game node="<<node<<" display="<<readstr(j,"display")<<"\n";}catch(std::exception&e){std::cerr<<e.what()<<"\n";}}json_object_put(j);}auto*aj=AndroidOnly?json_object_from_file((base+"/android-source.json").c_str()):nullptr;
   if(aj){uint32_t n=readnum(aj,"node");double st=readnum(aj,"started");if(n&&(n!=androidNode||st!=androidStarted||!androidCapture||androidCapture->failed)){try{androidInput.reset();androidCapture.reset();androidHasFrame=false;androidCapture=std::make_unique<Capture>(n,720,1600,(uint64_t)readnum(aj,"serial"));androidInput=std::make_unique<AndroidTouch>(readstr(aj,"display"));androidNode=n;androidStarted=st;androidUploaded=0;std::cerr<<"Connected Android display "<<readstr(aj,"display")<<" node="<<n<<"\n";}catch(std::exception&e){std::cerr<<"Android: "<<e.what()<<"\n";}}json_object_put(aj);}else{androidInput.reset();androidCapture.reset();androidHasFrame=false;}
   lastSource=now;dirty=true;}
  if(AndroidOnly&&now-lastControlsPoll>std::chrono::milliseconds(200)){
   lastControlsPoll=now;auto*c=json_object_from_file((base+"/android-controls.json").c_str());
   if(c){if(androidInput)androidInput->configure((int)readnum(c,"container_pid"));double revision=readnum(c,"updated");float level=std::clamp((float)readnum(c,"brightness",brightness),.25f,1.f);
    if(!brightnessDragging&&revision!=controlsRevision&&(now-lastBrightnessCommand>std::chrono::milliseconds(1500)||fabsf(level-brightness)<.01f)){
     controlsRevision=revision;if(fabsf(level-brightness)>.001f){brightness=level;dirty=true;save();}
    }json_object_put(c);}
  }
  if(capture){captureRecovery.poll(capture);if(capture->frames!=uploaded){if(!androidOpen&&!settingsOpen&&visible)renderer.screenPixels(capture->info.size.width,capture->info.size.height,capture->rgba.data());uploaded=capture->frames;if(!hasFrame){hasFrame=true;dirty=true;std::cerr<<"Live game screen received\n";}}}if(input){input->tick();if(capture&&hasFrame)autoresume.tick(*capture,*input);}
  if(petKeyboard.take(now)&&!AndroidOnly&&visible&&!settingsOpen&&!grab.active&&input&&capture&&hasFrame&&!capture->failed
     &&petTextFieldAt(capture->rgba,capture->info.size.width,capture->info.size.height,petKeyboard.x,petKeyboard.y)){
   input->end();keyboard();
  }
  if(androidCapture){androidCapture->poll();if(androidCapture->frames!=androidUploaded){if(androidOpen&&!settingsOpen&&visible)renderer.screenPixels(androidCapture->info.size.width,androidCapture->info.size.height,androidCapture->rgba.data());androidUploaded=androidCapture->frames;if(!androidHasFrame){androidHasFrame=true;dirty=true;std::cerr<<"Android screen received\n";}}}if(androidInput)androidInput->tick();
  TrackedDevicePose_t poses[k_unMaxTrackedDeviceCount];VRSystem()->GetDeviceToAbsoluteTrackingPose(TrackingUniverseStanding,.011f,poses,k_unMaxTrackedDeviceCount);M head=fromvr(poses[0].mDeviceToAbsoluteTracking);dock.refresh();dock.show(poses[0].bPoseIsValid);if(dock.takeHomeRequest()&&visible)returnToDock("layout changed");if(!placed&&!docked&&poses[0].bPoseIsValid)recenter(head);
  if(recallRequested&&poses[0].bPoseIsValid){auto left=VRSystem()->GetTrackedDeviceIndexForControllerRole(TrackedControllerRole_LeftHand);if(!wrist||left>=k_unMaxTrackedDeviceCount||!poses[left].bPoseIsValid){recenter(head);haveWristPose=false;}recallRequested=false;save();}
  if(grab.active){
   if(wrist||!visible||!poses[0].bPoseIsValid||grab.controller>=k_unMaxTrackedDeviceCount||!poses[grab.controller].bPoseIsValid){grab.end();save();}
   else{world=grab.follow(fromvr(poses[grab.controller].mDeviceToAbsoluteTracking));VRControllerState_t state{};
    if(grab.watchTriggerRelease&&VRSystem()->GetControllerState(grab.controller,&state,sizeof(state))&&!(state.ulButtonPressed&ButtonMaskFromId(k_EButton_SteamVR_Trigger))){releaseGrab();}}
  }
  VREvent_t e;while(VRSystem()->PollNextEvent(&e,sizeof(e))){
   if(e.eventType==VREvent_KeyboardOpened_Global)keyboardOwned=e.data.keyboard.overlayHandle==device;
   if(e.eventType==VREvent_KeyboardClosed_Global&&e.data.keyboard.overlayHandle==device)keyboardOwned=false;
   if(dockResetEvent(e.eventType))resetPlacement(e.eventType);if(e.eventType==VREvent_Quit){VRSystem()->AcknowledgeQuit_Exiting();running=0;}}
  while(dash&&ov->PollNextOverlayEvent(dash,&e,sizeof(e))){
   if(dockResetEvent(e.eventType))resetPlacement(e.eventType);
   if(e.eventType==VREvent_Quit)running=0;
   if(e.eventType==VREvent_OverlayShown){openFromLauncher();std::cerr<<"Taskbar opened settings on phone\n";}
  }
  while(queuedClick||ov->PollNextOverlayEvent(device,&e,sizeof(e))){
   bool simulatedClick=queuedClick;if(queuedClick){e=lastPointer;e.eventType=VREvent_MouseButtonDown;e.data.mouse.button=VRMouseButton_Left;queuedClick=false;}
   if(dockResetEvent(e.eventType)){resetPlacement(e.eventType);continue;}
   if(returnedHome)continue;
   if(e.eventType==VREvent_ScrollSmooth&&grab.active){
    auto source=e.trackedDeviceIndex;
    if(source==0||source==k_unTrackedDeviceIndexInvalid)source=ov->GetPrimaryDashboardDevice();
    bool accepted=grab.queueScrollDepth(source,e.data.scroll.ydelta,e.eventAgeSeconds);
    if(grabDebug&&!accepted&&now-lastGrabLog>std::chrono::seconds(1)){lastGrabLog=now;std::cerr<<"Grab scroll ignored: source="<<source<<" owner="<<grab.controller<<" age="<<e.eventAgeSeconds<<"\n";}
    continue;
   }
   if(e.eventType==VREvent_FocusLeave){finishBrightness();pointerOnScreen=false;pointerOnHandle=false;if(androidInput)androidInput->end();}
   if(AndroidOnly&&e.eventType==VREvent_ScrollDiscrete&&pointerOnScreen&&!settingsOpen&&visible&&!grab.active&&androidInput&&ov->IsHoverTargetOverlay(device)){
    androidInput->stickScroll(e.data.scroll.xdelta,e.data.scroll.ydelta,.05f,pointerX,pointerY);continue;
   }
   if(e.eventType==VREvent_MouseButtonUp&&e.data.mouse.button==VRMouseButton_Left)finishBrightness();
   if(e.eventType==VREvent_MouseButtonUp&&e.data.mouse.button==VRMouseButton_Left&&androidInput)androidInput->end();
   if(e.eventType==VREvent_MouseButtonUp&&e.data.mouse.button==VRMouseButton_Left&&grab.active){if(e.trackedDeviceIndex==grab.controller||e.trackedDeviceIndex==k_unTrackedDeviceIndexInvalid||e.trackedDeviceIndex==0){releaseGrab();}continue;}
   if(e.eventType==VREvent_KeyboardClosed)keyboardOwned=false;
   if(grab.active)continue;
   // Read chars only from this overlay, not the duplicate global VR events.
   if(e.eventType==VREvent_KeyboardCharInput&&!settingsOpen){std::string value(e.data.keyboard.cNewInput,strnlen(e.data.keyboard.cNewInput,8));if(androidOpen&&androidInput)androidInput->text(value);else if(input)input->text(value);}
   if(e.eventType!=VREvent_MouseMove&&e.eventType!=VREvent_MouseButtonDown)continue;
   if(e.eventType==VREvent_MouseButtonDown&&e.data.mouse.button!=VRMouseButton_Left)continue;
   auto index=e.trackedDeviceIndex;if((index==0||index>=k_unMaxTrackedDeviceCount||!poses[index].bPoseIsValid))index=ov->GetPrimaryDashboardDevice();if(index>=k_unMaxTrackedDeviceCount||!poses[index].bPoseIsValid)index=0;
   V origin=point(fromvr(poses[index].mDeviceToAbsoluteTracking),{0,0,0});
   V target=point(lastPlane,{(e.data.mouse.x/512-.5f)*lastWidth,(e.data.mouse.y/512-.5f)*lastWidth,0});
   M inv=inverse(lastModel);V a=mul(point(inv,origin),1/lastSize),b=mul(point(inv,target),1/lastSize);V delta=sub(b,a);if(fabsf(delta[2])<.0001f)continue;float t=(ScreenZ-a[2])/delta[2];if(t<0)continue;V hit=add(a,mul(delta,t));bool down=e.eventType==VREvent_MouseButtonDown;
   if(down)std::cerr<<"Device input "<<e.data.mouse.x<<","<<e.data.mouse.y<<" hit="<<hit[0]<<","<<hit[1]<<" source="<<index<<"\n";
   bool front=a[2]>ScreenZ;
   pointerOnScreen=front&&onGameScreen(hit);pointerOnHandle=renderer.showGrabHandle&&onGrabHandle(hit);pointerController=index;
   if(pointerOnScreen){lastPointer=e;lastPointer.trackedDeviceIndex=index;auto targetPixel=screenPixel(hit);pointerX=targetPixel[0];pointerY=targetPixel[1];}
   if(down&&front&&onHomeButton(hit)){
    if(now-lastSettingsTap>=std::chrono::milliseconds(180)){lastSettingsTap=now;toggleSettings(!settingsOpen);std::cerr<<"Phone settings "<<(settingsOpen?"opened":"closed")<<"\n";}
    continue;
   }
   if(down&&!simulatedClick&&!wrist&&(pointerOnHandle||(onPhoneBody(hit)&&(!front||!onGameScreen(hit))))&&index!=0&&poses[index].bPoseIsValid){
    VRControllerState_t state{};bool watch=VRSystem()->GetControllerState(index,&state,sizeof(state))&&(state.ulButtonPressed&ButtonMaskFromId(k_EButton_SteamVR_Trigger));
    V grabPoint=point(lastModel,mul(hit,lastSize));
    if(docked){world=undockedRoomPose(lastModel,yaw);V restoredPoint=point(mm(world,yawPose(yaw)),mul(hit,size));for(int axis=0;axis<3;axis++)world[12+axis]+=grabPoint[axis]-restoredPoint[axis];placed=true;docked=false;save();std::cerr<<"Creation taken out of dock\n";}
    closeKeyboard();finishBrightness();if(androidInput)androidInput->end();if(input)input->end();pointerOnScreen=false;queuedClick=false;
    shake.reset();dockCandidate=false;grab.beginAt(index,fromvr(poses[index].mDeviceToAbsoluteTracking),world,grabPoint,watch);
    ov->TriggerLaserMouseHapticVibration(device,.035f,80,.25f);std::cerr<<"Room grab began: controller "<<index<<"\n";continue;
   }
   if(front&&onGameScreen(hit)){
    auto pixel=settingsOpen?screenPixel(hit,SettingsWidth,SettingsHeight):screenPixel(hit);int x=pixel[0],y=pixel[1];
    if(settingsOpen){
     // Brightness belongs to the VR screen; Android has no hardware backlight.
     if(AndroidOnly&&(brightnessDragging||(down&&phoneActionAt(x,y)==PhoneAction::BrightnessSlider))){
      brightnessDragging=true;brightness=phoneBrightnessAt(x);dirty=true;lastBrightnessCommand=now;
      if(simulatedClick)finishBrightness();continue;
     }
     // Consume every screen event here; none may leak through to the game.
     if(down&&now-lastSettingsTap>=std::chrono::milliseconds(180)){
      lastSettingsTap=now;PhoneAction action=phoneActionAt(x,y);
      switch(action){
       case PhoneAction::Back:toggleSettings(false);break;
       case PhoneAction::Home:androidCommand("{\"op\":\"home\"}");toggleSettings(false);break;
       
       case PhoneAction::Room:grab.end();wrist=false;if(docked){world=undockedRoomPose(lastModel,yaw);placed=true;docked=false;}break;
       case PhoneAction::Hand:grab.end();docked=false;wrist=true;break;
       case PhoneAction::Dock:returnToDock();break;
       case PhoneAction::Recenter:visible=true;wrist=false;if(poses[0].bPoseIsValid)recenter(head);break;
       case PhoneAction::Smaller:size=std::max(.6f,size-.2f);break;
       case PhoneAction::Larger:size=std::min(3.f,size+.2f);break;
       case PhoneAction::Dimmer:brightness=std::max(.25f,brightness-.1f);break;
       case PhoneAction::Brighter:brightness=std::min(1.f,brightness+.1f);break;
       case PhoneAction::TurnLeft:yaw-=.25f;break;
       case PhoneAction::TurnRight:yaw+=.25f;break;
       case PhoneAction::Keyboard:toggleSettings(false);keyboard();break;
       case PhoneAction::Hide:visible=false;grab.end();toggleSettings(false);break;
       case PhoneAction::Reset:yaw=0;size=DefaultSize;returnToDock("position and size reset");break;
       case PhoneAction::BrightnessSlider:break;
       case PhoneAction::NoAction:break;
      }
      if(action!=PhoneAction::NoAction){
       if(AndroidOnly&&(action==PhoneAction::Dimmer||action==PhoneAction::Brighter)){lastBrightnessCommand=now;androidCommand("{\"op\":\"brightness\",\"value\":"+std::to_string(brightness)+"}");}
       dirty=true;save();ov->TriggerLaserMouseHapticVibration(device,.025f,80,.15f);std::cerr<<"Phone settings action "<<(int)action<<" at "<<x<<","<<y<<"\n";}
     }
     continue;
    }
    if(androidOpen){if(androidInput&&androidHasFrame){if(down){androidInput->stopScroll();if(simulatedClick)androidInput->tap(x,y);else androidInput->begin(x,y);}else androidInput->move(x,y);}continue;}
    if(input&&hasFrame){if(down){autoresume.cancel();input->tap(x,y);
     bool field=capture&&!capture->failed&&petTextFieldAt(capture->rgba,capture->info.size.width,capture->info.size.height,x,y);
     petKeyboard.tap(field&&!keyboardOwned,x,y,now);
     std::cerr<<"Game tap "<<x<<","<<y<<(field?" text field":"")<<"\n";
    }else input->move(x,y);}
   }else{finishBrightness();if(androidInput&&androidOpen)androidInput->end();}
  }
  if(navigation){
   auto controller=grab.active?grab.controller:pointerController;
   bool focused=visible&&(!docked||dock.available)&&poses[0].bPoseIsValid&&controller<k_unMaxTrackedDeviceCount&&poses[controller].bPoseIsValid&&(grab.active||(AndroidOnly&&pointerOnScreen&&ov->IsHoverTargetOverlay(device)));
   int role=controller<k_unMaxTrackedDeviceCount?(int)VRSystem()->GetControllerRoleForTrackedDeviceIndex(controller):0;
   float dt=std::chrono::duration<float>(now-lastNavigation).count();lastNavigation=now;
   auto action=navigation->update(focused,role,dt,grab.active);
   if(grab.active){
    float depth=action.scrollY;bool axisAvailable=action.axisAvailable;
    if(!action.axisAvailable){VRControllerState_t state{};if(VRSystem()->GetControllerState(controller,&state,sizeof(state)))for(int axis=0;axis<5;axis++)if(VRSystem()->GetInt32TrackedDeviceProperty(controller,(ETrackedDeviceProperty)(Prop_Axis0Type_Int32+axis))==k_eControllerAxis_Joystick){depth=ControllerNav::deflection(state.rAxis[axis].y);axisAvailable=true;break;}}
    bool scrollInput=grab.pendingScrollDepth!=0;float before=grab.distance;
    grab.updateDepth(depth,dt,axisAvailable);world=grab.follow(fromvr(poses[controller].mDeviceToAbsoluteTracking));
    if(grabDebug&&now-lastGrabLog>std::chrono::seconds(1)){lastGrabLog=now;std::cerr<<"Grab depth controller="<<controller<<" input="<<(axisAvailable?"axis":scrollInput?"scroll":"none")<<" distance="<<grab.distance<<" change="<<grab.distance-before<<"\n";}
   }else if(focused&&AndroidOnly){
   if(action.home){if(androidInput)androidInput->end();androidCommand("{\"op\":\"home\"}");toggleSettings(false);std::cerr<<"Controller Home\n";}
   else if(action.back){if(androidInput)androidInput->end();if(settingsOpen)toggleSettings(false);else androidCommand("{\"op\":\"back\"}");std::cerr<<"Controller Back\n";}
   else if(action.click){if(androidInput)androidInput->stopScroll();queuedClick=true;std::cerr<<"Controller click\n";}
   if(!settingsOpen&&androidInput&&!action.click&&!action.back&&!action.home){androidInput->stickScroll(action.scrollX,action.scrollY,dt,pointerX,pointerY);if(action.x||action.y)std::cerr<<"Controller scroll "<<action.x<<","<<action.y<<"\n";}
   }
  }
  if(!visible||!poses[0].bPoseIsValid||(docked&&!dock.available)){finishBrightness();if(androidInput)androidInput->end();}
  if(androidOpen&&!androidHasFrame&&!settingsOpen&&dirty){ui.fill(.035,.048,.065);ui.text(30,480,"Starting Android...",33);ui.text(30,530,AndroidOnly?"Bottom bezel button: settings":"Center button: settings",23);renderer.screen(ui);dirty=false;}
  if(settingsOpen&&(dirty||now-lastUI>std::chrono::seconds(30))){
   auto*care=json_object_from_file((base+"/care-mode.json").c_str());bool protectedCare=readstr(care,"mode")=="no-death";if(care)json_object_put(care);
   drawPhoneSettings(ui,wrist,size,AndroidOnly?androidHasFrame:hasFrame,protectedCare,brightness,docked);renderer.screen(ui);dirty=false;lastUI=now;
  }
  if(grab.active){
   M hand=fromvr(poses[grab.controller].mDeviceToAbsoluteTracking);
   V gesturePoint=sub(point(hand,{0,0,-.15f}),point(head,{0,0,0}));
   if(shake.update(gesturePoint,std::chrono::duration<double>(now.time_since_epoch()).count())){
    ov->TriggerLaserMouseHapticVibration(device,.09f,100,.5f);returnToDock("shake");
   }else{
    bool candidate=dock.canSnap(mm(world,yawPose(yaw)),size,dockCandidate);
    if(candidate&&!dockCandidate)ov->TriggerLaserMouseHapticVibration(device,.025f,80,.2f);
    dockCandidate=candidate;
   }
  }else{shake.reset();dockCandidate=false;}
  renderer.brightness=brightness;
  renderer.dockGrabHandle=dockCandidate;
  renderer.showGrabHandle=!wrist&&(ov->IsDashboardVisible()||grab.active||ov->IsHoverTargetOverlay(device));
  renderer.highlightGrabHandle=grab.active||pointerOnHandle;
  if(visible&&(docked?dock.available:placed)&&poses[0].bPoseIsValid){M model=docked?dock.model(size):world;float effective=docked?dock.size(size):size;bool show=true;if(wrist){auto idx=VRSystem()->GetTrackedDeviceIndexForControllerRole(TrackedControllerRole_LeftHand);if(idx<k_unMaxTrackedDeviceCount&&poses[idx].bPoseIsValid){model=mm(fromvr(poses[idx].mDeviceToAbsoluteTracking),pose({1,0,0},{0,0,-1},{0,1,0},{0,.055f,-.10f}));wristFallback=model;haveWristPose=true;effective*=.55f;}else if(haveWristPose){model=wristFallback;effective*=.55f;}}
   if(!docked)model=mm(model,yawPose(yaw));V p=point(model,{0,0,ScreenZ*effective}),h=point(head,{0,0,0}),z=norm(sub(h,p)),x=norm(cross({0,1,0},z)),y=cross(z,x);M plane=pose(x,y,z,p);float width=InteractionPlaneWidth*effective;
   if(show&&dot(sub(h,p),sub(h,p))>.03f){
    V eye[2];for(int i=0;i<2;i++)eye[i]=point(mm(head,fromvr(VRSystem()->GetEyeToHeadTransform((EVREye)i))),{0,0,0});renderer.mode=(androidOpen?androidHasFrame:hasFrame)?mode:-1;renderer.draw(plane,model,eye[0],eye[1],width,effective,false);glFinish();Texture_t texture{(void*)(uintptr_t)renderer.tex,TextureType_OpenGL,ColorSpace_Gamma};check(ov->SetOverlayTexture(device,&texture),"Phone texture");check(ov->SetOverlayWidthInMeters(device,width),"Phone width");auto tr=tovr(plane);check(ov->SetOverlayTransformAbsolute(device,TrackingUniverseStanding,&tr),"Phone placement");check(ov->ShowOverlay(device),"Phone show");lastPlane=plane;lastModel=model;lastSize=effective;lastWidth=width;}else ov->HideOverlay(device);
  }else ov->HideOverlay(device);
  if(!ready){ftruncate(lock,0);std::string pid=std::to_string(getpid());pwrite(lock,pid.data(),pid.size(),0);if(std::getenv("MAGICSHELF_SESSION"))magicshelfReady();std::cerr<<AppTitle<<" ready\n";ready=true;}
  bool active=VRSystem()->GetTrackedDeviceActivityLevel(0)==k_EDeviceActivityLevel_UserInteraction;std::this_thread::sleep_for(std::chrono::milliseconds(active&&visible?22:100));
 }
 closeKeyboard();dock.destroy();ov->DestroyOverlay(device);if(dash)ov->DestroyOverlay(dash);VR_Shutdown();close(lock);return 0;
 }catch(std::exception&e){std::cerr<<AppTitle<<": "<<e.what()<<"\n";VR_Shutdown();return 1;}}
