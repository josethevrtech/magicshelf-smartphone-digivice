#pragma once
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstdlib>
#include <string>
#include <cstring>
// Local datagrams keep container command latency out of the VR rendering loop.
bool androidCommand(const std::string&message){
 auto*runtime=std::getenv("XDG_RUNTIME_DIR");if(!runtime)return false;
 std::string path=std::string(runtime)+"/android-phone-control.sock";
 int fd=socket(AF_UNIX,SOCK_DGRAM|SOCK_NONBLOCK|SOCK_CLOEXEC,0);if(fd<0)return false;
 sockaddr_un address{};address.sun_family=AF_UNIX;
 if(path.size()>=sizeof(address.sun_path)){close(fd);return false;}
 std::strcpy(address.sun_path,path.c_str());
 bool ok=sendto(fd,message.data(),message.size(),MSG_NOSIGNAL,(sockaddr*)&address,sizeof(address))==(ssize_t)message.size();close(fd);return ok;
}
