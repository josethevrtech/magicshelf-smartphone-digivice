#pragma once
// This creation owns its identity and geometry. The SDK consumes this profile.
constexpr bool AndroidOnly=false;
constexpr const char* ApplicationKey="local.digitalpartner.vr";
constexpr const char* DeviceKey="local.digitalpartner.device";
constexpr const char* AppTitle="Smartphone Digivice";
constexpr const char* ManifestFile="digitalpartner.vrmanifest";
constexpr const char* SettingsFile="settings.json";
constexpr const char* LockFile="instance.lock";
constexpr const char* IconFile="season1-icon.png";
constexpr const char* AssetDirectory="assets";
constexpr int ScreenWidth=480,ScreenHeight=1046;
constexpr float ScreenY=.0825f,ScreenX=ScreenY*ScreenWidth/ScreenHeight;
constexpr float ScreenCY=.0132f,ScreenZ=.0034925f,PhonePlaneWidth=.272f;
constexpr float BodyX=.066275f,BodyY=.1144f,HomeY=-.0935f;
constexpr float HomeHalfX=.020f,HomeHalfY=.014f,DefaultSize=1.6f;
constexpr int RenderWidth=2048,RenderHeight=1024;
constexpr int SettingsWidth=480,SettingsHeight=1046;
constexpr float GrabHalfWidth=.025f,GrabHalfHeight=.003f,GrabY=-BodyY-.013f;
// The transparent stereo canvas includes the handle and its larger hit target.
constexpr float InteractionPlaneWidth=2.f*(BodyY+.025f)>PhonePlaneWidth?2.f*(BodyY+.025f):PhonePlaneWidth;
