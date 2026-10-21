#pragma once
#include <fstream>
struct ResumePattern{
 struct Pixel{int x,y,r,g,b;};std::vector<Pixel>pixels;
 ResumePattern(std::string file){std::ifstream f(file);Pixel p;while(f>>p.x>>p.y>>p.r>>p.g>>p.b)pixels.push_back(p);}
 bool matchesPixels(const std::vector<unsigned char>&rgba,int w,int h){if(pixels.size()<100||w!=480||h!=1046||rgba.size()!=480*1046*4)return false;int good=0;for(auto&p:pixels){auto*q=&rgba[(p.y*480+p.x)*4];if(abs(q[0]-p.r)<28&&abs(q[1]-p.g)<28&&abs(q[2]-p.b)<28)good++;}return good>pixels.size()*.97;}
 bool matches(Capture&c){return matchesPixels(c.rgba,c.info.size.width,c.info.size.height);}
};
struct AutoResume{
 ResumePattern title,slots;int phase=0,hits=0,slot=1;bool enabled=false;std::chrono::steady_clock::time_point began,last;
 AutoResume(std::string base):title(base+"/assets/title.pattern"),slots(base+"/assets/slots.pattern"){}
 void reset(bool allow,int selectedSlot=1){slot=std::clamp(selectedSlot,1,9);enabled=allow;phase=hits=0;began=last=std::chrono::steady_clock::now();}
 void cancel(){enabled=false;}
 void tick(Capture&c,GameInput&i){auto now=std::chrono::steady_clock::now();if(!enabled)return;if(now-began>std::chrono::seconds(90)){enabled=false;return;}if(now-last<std::chrono::seconds(2))return;bool match=phase==0?title.matches(c):slots.matches(c);if(!match){hits=0;return;}if(++hits<3)return;hits=0;last=now;if(phase==0){i.tap(240,655);phase=1;std::cerr<<"Startup: Load screen recognized, opening saved pets\n";}else{i.tap(110+((slot-1)%3)*130,335+((slot-1)/3)*155);enabled=false;std::cerr<<"Startup: selecting existing Memory "<<slot<<"\n";}}
};
