#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include <unistd.h>

#include "webrtc/base/helpers.h"
#include "webrtc/base/json.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/basictypes.h"
#include "webrtc/base/refcount.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/base/stringencode.h"

#include "webrtc/media/base/videocapturer.h"

#include "webrtc/api/jsep.h"
#include "webrtc/api/mediaconstraintsinterface.h"
#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/api/datachannelinterface.h"

#include "RTCPeer.hpp"

namespace maya{

/*============================================================*/
/*======================CONSTANTS=============================*/
/*============================================================*/

const char kAudioLabel[] = "audio_label";
const char kVideoLabel[] = "video_label";

/*============================================================*/
/*============================================================*/
/*============================================================*/




//Initialize SSL
void initRTC(){
	rtc::InitializeSSL();
}

void destroyRTC(){
	rtc::CleanupSSL();
}




RTCPeer::RTCPeer(RTCSignalingChannel *signalingChannel){
	this->signalingChannel = signalingChannel;

	signalingChannel->setPeer(this);

	mutex = PTHREAD_MUTEX_INITIALIZER;
	mutexConnection = PTHREAD_MUTEX_INITIALIZER;
}



RTCPeer::~RTCPeer(){

}

RTCSignalingChannel * RTCPeer::getSignalingChannel(){
	return signalingChannel;
}

void RTCPeer::disconnect(){
	for(auto kv : channels){
		kv.second->unsetDataChannel();
	}

	pthread_mutex_lock(&mutexConnection);

	for(auto kv : connections){
		kv.second->close();
		kv.second->Release();
	}

	connections.clear();

	pthread_mutex_unlock(&mutexConnection);
}

void RTCPeer::join(){
	printf("RTC join\n"); sleep(3); // jfellus

	signalingChannel->join();
}

void RTCPeer::createPeerConnectionFactory(){

	sig_thread = rtc::ThreadManager::Instance()->WrapCurrentThread();
	worker_thread = new rtc::Thread;
	worker_thread->Start();

	peerConnectionFactory = webrtc::CreatePeerConnectionFactory(worker_thread, sig_thread, NULL, NULL, NULL);
}

RTCStreamInterface* RTCPeer::registerStream(const char* name, uint w, uint h){
	pthread_mutex_lock(&mutex);
	RTCStream* stream = new RTCStream(name, w, h);
	streams[std::string(name, strlen(name))] = stream;
	pthread_mutex_unlock(&mutex);
	return stream;
}

RTCChannelInterface* RTCPeer::registerChannel(const char* name, int reliable){
	pthread_mutex_lock(&mutex);
	RTCChannel *channel = new RTCChannel((char*)name, reliable);

	channels[std::string(name, strlen(name))] = channel;
	pthread_mutex_unlock(&mutex);

	return channel;
}

bool RTCPeer::offerChannel(webrtc::DataChannelInterface *channel){
	try{
		RTCChannel *ch = channels.at(channel->label());
		if(ch->isConnected()) return false;
		ch->setDataChannel(channel);
		return true;
	}catch(std::out_of_range& ex){
		return false;
	}
}

//look for garbage connections and remove them
void RTCPeer::cleanConnections() {
	std::vector<int> garbageConnections;

	bool connectionCleaned = false;
	
	pthread_mutex_lock(&mutexConnection);	
	for(auto kv : connections){
		if(kv.second->hasTimeoutExpired()) {
			kv.second->close();
			garbageConnections.push_back(kv.second->getPeerID());	
			connectionCleaned = true;
		}
	}

	for(int i=0; i<garbageConnections.size(); i++) {
		connections.erase(garbageConnections[i]);
	}

	if(connectionCleaned) {
		std::cout << "[RTC] active connections:" << connections.size() << std::endl;
	}

	pthread_mutex_unlock(&mutexConnection);
}

void RTCPeer::deleteConnection(int peerid){
	rtc::RefCountedObject<RTCConnection> *connection;

	try{
		connection = connections.at(peerid);
		connection->close();
		connection->Release();
		connections.erase(peerid);

	}catch(std::out_of_range& error){
	}
	
	std::cout << "[RTC] active connections :" << connections.size() << std::endl;
}

RTCConnection * RTCPeer::getConnection(int peerid){

	rtc::RefCountedObject<RTCConnection> *connection;

	pthread_mutex_lock(&mutexConnection);
	try{
		connection = connections.at(peerid);
	}catch(std::out_of_range& error){
		connection = new rtc::RefCountedObject<RTCConnection>(this, peerConnectionFactory, peerid);
		connection->AddRef();
		connections[peerid] = connection;
	}
	pthread_mutex_unlock(&mutexConnection);
	return connection;
}

//////////////////////////////////////////////////////////////////////
/////////////SignalingChannel Observer implementation/////////////////
//////////////////////////////////////////////////////////////////////

void RTCPeer::onSignalingThreadStarted(){
	createPeerConnectionFactory();
}

void RTCPeer::onSignalingThreadStopped() {
	//std::cout << "deleting RTCPeer... " << std::endl;

	pthread_mutex_lock(&mutexConnection);
	for(auto kv : connections){
		kv.second->Release();
	}

	connections.clear();
	pthread_mutex_unlock(&mutexConnection);

	for(auto kv : channels){
		delete kv.second;
	}

	for(auto kv : streams) {
		delete kv.second;
	}

	channels.clear();

	peerConnectionFactory = NULL;


	//std::cout << "[ OK ]" << std::endl;
}

void RTCPeer::onConnectionClosed(int peerId) {
	pthread_mutex_lock(&mutexConnection);
	deleteConnection(peerId);
	pthread_mutex_unlock(&mutexConnection);
}

void RTCPeer::processMessages(){
	cleanConnections();
	sig_thread->ProcessMessages(10);
	//worker_thread->ProcessMessages(10);
}

void RTCPeer::onStateChanged(RTCSignalingChannelState state){

}

void RTCPeer::onRemoteSDP(int peerid, std::string type, std::string sdp){

	RTCConnection* connection = getConnection(peerid);

	webrtc::SessionDescriptionInterface* session_description = webrtc::CreateSessionDescription(type, sdp, NULL);
	if (!session_description) {
		std::cerr << "[SIG] error : cannot parse SDP string" << std::endl;
		return;
	}

	connection->setRemoteSessionDescription(session_description);
}

std::vector<std::string> RTCPeer::getChannelNames(){

	std::vector<std::string> ret;

	for(auto kv : this->channels){
		ret.push_back(kv.first);
	}

	return ret;
}

void RTCPeer::onConnectionRequest(int peerid, std::vector<std::string> channelnames, std::vector<MayaIceServer> iceservers) {

	std::vector<RTCChannel*> requestedChannels;

	//limit the max number of simultaneous peer connections
	if (connections.size() > 5) {
		std::cout << "[RTCPeer] rejected peer connection (maxPeerConnections reached)" << std::endl;
		return ;
	}	

	for(int i=0;i<channelnames.size();i++){
		try{
			RTCChannel * ch = this->channels.at(channelnames[i]);
			//TODO : check is the channel is already connected or not
			requestedChannels.push_back(ch);
		}catch(std::out_of_range &error){
			std::cerr << "no channel found for \"" << channelnames[i] <<"\"" << std::endl;
		}
	}

	//If no channel is requested or no valid channel names are provided, abort connection attempt
	if(requestedChannels.size() <= 0){
		printf("connect : no match !\n");
		return ;
	}

	//Create new Connection
	RTCConnection *connection = getConnection(peerid);

	// Attach data channels
	for(int i=0; i<requestedChannels.size(); i++){
		rtc::scoped_refptr<webrtc::DataChannelInterface> wch = connection->createDataChannel(requestedChannels[i]->getName(), requestedChannels[i]->isReliable());
		requestedChannels[i]->setDataChannel(wch);
	}

	// Attach outgoing streams
	for(int i=0;i<channelnames.size();i++){
		try {
			RTCStream* stream = this->streams.at(channelnames[i]);
			stream->init(peerConnectionFactory, worker_thread);
			connection->addStream(stream->getStream());
		} catch(std::out_of_range &error){}
	}

	connection->createOffer(iceservers);
}

void RTCPeer::onRemoteICECandidate(int peerid, std::string sdp_mid, int sdp_mlineindex, std::string sdp){
	if(sdp_mid == "" && sdp_mlineindex == 0 && sdp == "") return;

	RTCConnection *connection = getConnection(peerid);

	//Create the received candidate
	std::unique_ptr<webrtc::IceCandidateInterface> candidate(webrtc::CreateIceCandidate(sdp_mid, sdp_mlineindex, sdp, NULL));

	if (!candidate.get()) {
		std::cout << "[SIG] cannot parse candidate information" << std::endl;
		return;
	}

	connection->addICECandidate(candidate.get());
}

// jfellus 26/02/2016
void RTCPeer::addChannelStreamMapping(int peerid, std::string channel, std::string stream) {
	printf("ADD CHANNEL STREAM MAPPING %s -> %s \n", stream.c_str(), channel.c_str());
	this->channelsStreamsMappings[stream] = channel;
}

RTCChannelInterface* RTCPeer::getChannelForStream(std::string stream) {
	try {
		std::string channelname = this->channelsStreamsMappings.at(stream);
		return this->channels.at(channelname);
	} catch(std::out_of_range &error) {
		std::cerr << "no channel found for stream \"" << stream <<"\"" << std::endl;
		return NULL;
	}
}


//

void RTCPeer::onMessage(int peerid, const char * msg, int msglength){	}



//Factory for RTCPeer
RTCPeerInterface* RTCPeerInterface::create(RTCSignalingChannel *signalingChannel){
	RTCPeer *p = new RTCPeer(signalingChannel);
	return p;
}


} //NAMESPACE MAYA
