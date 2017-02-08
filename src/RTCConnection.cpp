/*
 * RTCConnection.cpp
 *
 *  Created on: 5 août 2014
 *      Author: sylvain
 */

#include <ctime>

#include "RTCConnection.hpp"
#include "RTCPeer.hpp"

#include "webrtc/api/jsep.h"
#include "webrtc/api/peerconnectioninterface.h"

#include "RTCChannel.hpp"
#include "MemoryRenderer.hpp"

#define RTCCONNECTION_OFFER_TIMEOUT 20


namespace maya{

RTCConnection::RTCConnection(RTCPeer *peer, webrtc::PeerConnectionFactoryInterface * peerConnectionFactory, int peerID){
	this->peerID = peerID;
	this->peer = peer;
	this->createOfferTimestamp = -1;

	webrtc::PeerConnectionInterface::RTCConfiguration config;

	this->peerConnection = peerConnectionFactory->CreatePeerConnection(config, getMediaConstraints(), NULL, NULL, this);
}

RTCConnection::~RTCConnection(){
	if(peerConnection.get() != NULL) {
		peerConnection->Close();
		peerConnection = NULL;
	}
	std::cout << "RTCConnection closed !" << std::endl;
}

SimpleConstraints * RTCConnection::getMediaConstraints(){
	SimpleConstraints * constraints = new SimpleConstraints;

	constraints->AddMandatory("EnableDtlsSrtp","true");
	constraints->AddMandatory("DtlsSrtpKeyAgreement", "true");
	constraints->AddMandatory("kEnableSctpDataChannels", "true");
	constraints->AddMandatory("OfferToReceiveAudio", "true");
	constraints->AddMandatory("OfferToReceiveVideo", "true");

	return constraints;
}



///////////////////////////////////////////////////////////////////////////////
///////////////////////PeerConnectionObserver Implementation///////////////////
///////////////////////////////////////////////////////////////////////////////

// Triggered when a remote peer opens a data channel.
void RTCConnection::OnDataChannel(webrtc::DataChannelInterface* data_channel){
//	data_channel->Close(); //Remote opening of data channels not allowed, fuckers !
	std::cout << data_channel->label() << std::endl;
}

// jfellus 26/02/2016
// Triggered when a remote peer opens a stream.
void RTCConnection::OnAddStream(webrtc::MediaStreamInterface* stream) {
	printf("ADD STREAM : %s !!\n", stream->label().c_str());

	RTCChannel* channel = static_cast<RTCChannel*>(peer->getChannelForStream(stream->label()));
	if(!channel) {fprintf(stderr, "No channel registered for stream %s\n", stream->label().c_str()); return; }

	channel->setStream(stream);
}
//


// New Ice candidate have been found.
void RTCConnection::OnIceCandidate(const webrtc::IceCandidateInterface* candidate){

	std::string sdp_mid = candidate->sdp_mid();
	int sdp_mlineindex = candidate->sdp_mline_index();
	std::string sdp;
	if (!candidate->ToString(&sdp)) {
		return;
	}

	peer->getSignalingChannel()->sendLocalICECandidate(peerID, sdp_mid, sdp_mlineindex, sdp);
}

void RTCConnection::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state){
	if(new_state == webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionDisconnected
		|| new_state == webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionFailed){
		peer->onConnectionClosed(peerID);
	}
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
/////////////////CreateSessionDescriptionObserver implementation/////////////
/////////////////////////////////////////////////////////////////////////////

void RTCConnection::OnSuccess(webrtc::SessionDescriptionInterface* desc){
	peerConnection->SetLocalDescription(DummySetSessionDescriptionObserver::Create(0), desc);

	std::string sdp;
	if(!desc->ToString(&sdp)){
		return ; //Cannot serialize SDP
	}

	peer->getSignalingChannel()->sendLocalSDP(peerID, desc->type(), sdp);
}

void RTCConnection::OnFailure(const std::string& error){
	std::cerr << "CREATE ANSWER ERROR: " << error << std::endl;
}
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

void RTCConnection::close(){
	peerConnection->Close();
	peerConnection = NULL;
}

int RTCConnection::getPeerID(){
	return peerID;
}

bool RTCConnection::hasTimeoutExpired() {
	if(createOfferTimestamp == -1) { return false; }
	std::time_t currentTime = std::time(NULL);

	if(peerConnection.get() != NULL && currentTime - createOfferTimestamp > RTCCONNECTION_OFFER_TIMEOUT && 
		peerConnection->ice_connection_state() != webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionConnected &&
		peerConnection->ice_connection_state() != webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionDisconnected && //disconnected status may trigger intermittently on unreliable networks
		peerConnection->ice_connection_state() != webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionCompleted) {
		return true;
	} else {
		return false;
	}
}


rtc::scoped_refptr<webrtc::DataChannelInterface> RTCConnection::createDataChannel(char *name, int reliable){

	struct webrtc::DataChannelInit *init = new webrtc::DataChannelInit();

	if(!reliable){
		init->maxRetransmits = 0;
		init->ordered = false;
	}

	rtc::scoped_refptr<webrtc::DataChannelInterface> ch = peerConnection->CreateDataChannel(std::string(name,strlen(name)), init);

	return ch;
}

void RTCConnection::createOffer(std::string turn_url, std::string turn_username, std::string turn_password){
	createOfferTimestamp = std::time(NULL);
	webrtc::PeerConnectionInterface::RTCConfiguration config;
	webrtc::PeerConnectionInterface::IceServers servers;

	webrtc::PeerConnectionInterface::IceServer turnserver;
	turnserver.uri = turn_url;
	turnserver.username = turn_username;
	turnserver.password = turn_password;

	servers.push_back(turnserver);
	
	config.servers = servers;
	peerConnection->SetConfiguration(config);
	peerConnection->CreateOffer(this, getMediaConstraints());
}

void RTCConnection::setRemoteSessionDescription(webrtc::SessionDescriptionInterface* session_description){

	peerConnection->SetRemoteDescription(DummySetSessionDescriptionObserver::Create(0), session_description);
}

void RTCConnection::addStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream){
	peerConnection->AddStream(stream);
}

void RTCConnection::addICECandidate(webrtc::IceCandidateInterface *candidate){

	if (!peerConnection->AddIceCandidate(candidate)) {
		std::cout << "[SIG] cannot use new candidate" << std::endl;
	}
}

// implements the MessageHandler interface
void RTCConnection::OnMessage(rtc::Message* msg){
	std::cout << "MSG" << std::endl;
}


}
