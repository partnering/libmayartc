#ifndef __WEBRTCPEER_HPP
#define __WEBRTCPEER_HPP

#include <iostream>
#include <thread>
#include <pthread.h>
#include <unordered_map>

#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/api/peerconnectioninterface.h"

#include "RTCConnection.hpp"
#include "RTCSignaling.hpp"
#include "RTCCommon.hpp"
#include "RTCChannel.hpp"
#include "RTCStream.hpp"

#include "RTCPeerInterface.hpp"

namespace maya{

class RTCConnection;

	class RTCPeer : public RTCPeerInterface{

	private:
		rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peerConnectionFactory;

		std::unordered_map<int, rtc::RefCountedObject<RTCConnection>*> connections;
		RTCSignalingChannel *signalingChannel;
		std::unordered_map<std::string, RTCChannel*> channels;
		std::unordered_map<std::string, RTCStream*> streams;

		// jfellus 26/02/2016
		std::unordered_map<std::string, std::string> channelsStreamsMappings;
		//

		rtc::Thread* sig_thread;
		rtc::Thread* worker_thread;

		pthread_mutex_t mutex;
		pthread_mutex_t mutexConnection;

	public:
		RTCPeer(RTCSignalingChannel *signalingChannel);
		~RTCPeer();

		void createPeerConnectionFactory();

		RTCSignalingChannel * getSignalingChannel();
		void cleanConnections();
		void deleteConnection(int peerid);
		RTCConnection * getConnection(int peerid);
		virtual void join();
		bool offerChannel(webrtc::DataChannelInterface *channel);
		virtual RTCChannelInterface* registerChannel(const char* name, int reliable);
		virtual RTCStreamInterface* registerStream(const char* name, uint w, uint h);


		void disconnect();

		void onSignalingThreadStarted();
		void onSignalingThreadStopped();
		void onConnectionClosed(int peerId);
		void processMessages();

		void onStateChanged(RTCSignalingChannelState state);
		void onRemoteSDP(int peerid, std::string type, std::string sdp);
		void onRemoteICECandidate(int peerid, std::string sdp_mid, int sdp_mlineindex, std::string sdp);
		void onMessage(int peerid, const char * msg, int msglength);
		void onConnectionRequest(int peerid, std::vector<std::string> channels, std::vector<MayaIceServer> iceservers);

		// jfellus 26/02/2016
		void addChannelStreamMapping(int peerid, std::string channel, std::string stream);
		RTCChannelInterface* getChannelForStream(std::string stream);
		//

		std::vector<std::string> getChannelNames();

		// implements the MessageHandler interface
		void OnMessage(rtc::Message* msg);

	};


	void initRTC();
	void destroyRTC();
}

#endif
