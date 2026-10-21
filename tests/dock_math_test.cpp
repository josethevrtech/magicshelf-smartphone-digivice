#include "dock_layout.h"
#include "phone_ui.h"
#include <cassert>
void nearDock(M a,M b){for(int i=0;i<16;i++)assert(fabsf(a[i]-b[i])<.00003f);}
int main(){
 DockLayout layout;
 for(int n=0;n<240;n++){
  layout.joinYaw=(n%9)*2.f;layout.joinPitch=(n%5)-2.f;layout.joinRoll=(n%7)-3.f;
  float yaw=n*.047f,scale=.1f+(n%19)*.13f,userSize=.6f+(n%25)*.1f;
  M rigid=yawPose(yaw);rigid[12]=.1f;rigid[13]=1.1f;rigid[14]=-.75f;
  M scaled=rigid;for(int c=0;c<3;c++)for(int r=0;r<3;r++)scaled[r+c*4]*=scale;
  M recovered{};assert(rigidDockAnchor(scaled,recovered));nearDock(rigid,recovered);
  M a=layout.slot(recovered,0),b=layout.slot(recovered,1);
  V local=point(inverse(a),{b[12],b[13],b[14]});assert(fabsf(local[0]-.13f)<.00003f&&fabsf(local[1])<.00003f&&fabsf(local[2])<.00003f);
  float fit=dockObjectSize(userSize,layout.objectHeight);assert(fit<=userSize&&2*BodyY*fit<=.18001f);
  M object=dockObjectPose(a,fit);V feet=point(inverse(a),point(object,{0,-BodyY*fit,0}));assert(fabsf(feet[1])<.00003f);
  assert(2*BodyX*fit<layout.spacing);
  nearDock(mm(undockedRoomPose(object,.73f),yawPose(.73f)),object);
  M moved=mm(yawPose(.4f),rigid);nearDock(layout.slot(moved,0),mm(yawPose(.4f),a));
  M back=layout.backing(recovered);V edge=point(inverse(recovered),point(back,{layout.width()/2,0,0}));assert(fabsf(edge[0]-layout.rightEdge)<.00003f&&fabsf(edge[1]-layout.centerY)<.00003f&&fabsf(edge[2]-layout.depth)<.00003f);V loc=point(inverse(back),point(object,{0,0,0}));
  assert(fabsf(loc[0])+BodyX*fit<layout.width()/2);assert(loc[2]>.012f);
  V foot=point(inverse(back),point(object,{0,-BodyY*fit,0}));
  assert(fabsf(foot[1]-(-layout.railHeight*.5f+layout.footInset))<.00003f);
  assert(fabsf(foot[2]-layout.frontOffset)<.00003f);
  V top=point(inverse(back),point(object,{0,BodyY*fit,0}));assert(top[1]>layout.railHeight*.5f); // Most of the device extends above the rail.
  auto larger=layout.scaled(1.5f);assert(fabsf(larger.height()-layout.height()*1.5f)<.00001f);assert(fabsf(larger.x(0)-layout.x(0)*1.5f)<.00001f);

 }
 for(int n=1;n<=16;n++){layout.count=n;float last=-100;for(int i=0;i<n;i++){float x=layout.x(i);assert(x>last&&x<layout.rightEdge);last=x;}assert(fabsf(layout.x(n-1)-(layout.rightEdge-.01f-.5f*layout.spacing))<.00001f);}
 // The seam is fixed as the rail curves; each object's feet follow its local surface.
 for(int n=1;n<=16;n++){
  layout.count=n;layout.curveRadius=2.8f;layout.curvePitch=-40.f;
  M anchor=yawPose(.8f);nearDock(layout.surface(anchor,0),layout.seam(anchor));
  assert(layout.curvature()>0&&layout.curvature()<=.50001f);
  for(int i=0;i<n;i++){
   M surf=layout.surface(anchor,layout.x(i)-layout.rightEdge),slot=layout.slot(anchor,i);
   float size=dockObjectSize(1.f,layout.objectHeight);M obj=dockObjectPose(slot,size);
   V foot=point(inverse(surf),point(obj,{0,-BodyY*size,0}));
   assert(fabsf(foot[0])<.00003f&&fabsf(foot[1]-(-layout.railHeight/2+layout.footInset))<.00003f&&fabsf(foot[2]-layout.frontOffset)<.00003f);
  }
  auto bigger=layout.scaled(2);assert(fabsf(layout.curvature()-bigger.curvature())<.00001f);
  nearDock(mm(layout.backing(anchor),layout.pitch(layout.curvePitch)),layout.surface(anchor,-layout.width()/2));
 }
 // The shelf supports both feet on its top, follows the launcher, and scales as one unit.
 layout=DockLayout{};layout.shelf=true;layout.leanDegrees=-6;layout.joinYaw=56;layout.rightEdge=-.942;layout.centerY=-.091;layout.depth=.459;
 for(float scale:{.5f,1.f,2.f}){auto shelf=layout.scaled(scale);M anchor=yawPose(.8f),top=shelf.shelfPart(anchor,0),lip=shelf.shelfPart(anchor,1);
  for(int i=0;i<2;i++){float size=dockObjectSize(1.f,shelf.objectHeight);M obj=dockObjectPose(shelf.slot(anchor,i),size);V feet=point(inverse(top),point(obj,{0,-BodyY*size,0}));assert(fabsf(feet[2])<.00003f&&fabsf(feet[0])<shelf.width()/2&&fabsf(feet[1])<shelf.height()/2);}
  V lipTop=point(inverse(top),point(lip,{0,shelf.lipHeight()/2,0}));assert(fabsf(lipTop[2])<.00003f&&fabsf(lipTop[1]+shelf.height()/2)<.00003f);
  nearDock(shelf.shelfPart(mm(yawPose(.7f),anchor),0),mm(yawPose(.7f),top));
 }
 for(bool dashboard:{false,true})for(bool visible:{false,true})for(bool active:{false,true}){assert(dockVisible(dashboard,visible,active,true)==(dashboard&&visible&&active));assert(dockVisible(dashboard,visible,active,false)==dashboard);}
 M bad=identity(),out{};bad[0]=0;assert(!rigidDockAnchor(bad,out));bad=identity();bad[4]=.5f;assert(!rigidDockAnchor(bad,out));bad=identity();bad[12]=NAN;assert(!rigidDockAnchor(bad,out));bad=identity();bad[10]=-1;assert(!rigidDockAnchor(bad,out));
 assert(phoneActionAt(100,440)==PhoneAction::Dock);assert(phoneActionAt(320,440)==PhoneAction::Recenter);if(AndroidOnly)assert(phoneActionAt(320,140)==PhoneAction::Home);
 std::cout<<AppTitle<<": rail and shelf placement, foot contact, scaling, launcher visibility, grab and menu checks passed\n";
}
