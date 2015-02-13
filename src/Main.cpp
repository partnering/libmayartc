#include "RTCPeerInterface.hpp"
#include "MayaSignaling.hpp"

using namespace maya;

int main(void){
	initRTC();

	RTCPeerInterface * peer = RTCPeerInterface::create(MayaSignalingInterface::create());
	peer->registerChannel("ctrl.info");
	peer->join();

	return 0;
}
