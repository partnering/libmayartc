/*
 * RTCChannel.cpp
 *
 *  Created on: 6 août 2014
 *      Author: sylvain
 */

#include <iostream>

#include "webrtc/media/base/videosourceinterface.h"
#include "webrtc/base/copyonwritebuffer.h"

#include "RTCChannel.hpp"
#include "MemoryRenderer.hpp"

namespace maya{

RTCChannel::RTCChannel(char * name, int reliable){
	this->name = name;
	this->channel = NULL;
	this->reliable = reliable;
	this->negociated = false;
	this->negociationMessage = NULL;
	this->nextChannel = NULL;
	this->stream_cb = NULL;
	this->stream_cb_data = NULL;
}

RTCChannel::~RTCChannel(){
	unsetDataChannel();
}

char * RTCChannel::getName(){
	return name;
}

int RTCChannel::isReliable(){
	return reliable;
}


void RTCChannel::setStream(webrtc::MediaStreamInterface *stream) {
	// TODO : Handle [0] out of bound
	rtc::scoped_refptr<webrtc::VideoTrackInterface> track = stream->GetVideoTracks()[0];

	if(memoryRenderer) {delete memoryRenderer; memoryRenderer = NULL;}
	memoryRenderer = new MemoryRenderer();
	memoryRenderer->setFrameObserver(this);

	track->AddOrUpdateSink(memoryRenderer, rtc::VideoSinkWants());
}

void RTCChannel::onFrame(unsigned char* frame, int w, int h) {
	if(stream_cb) stream_cb(frame, w, h, stream_cb_data);
}


void RTCChannel::unsetDataChannel(){
	if(channel != NULL){
		channel->UnregisterObserver();
		channel = NULL;
	}
	//std::cout << "CH " << name << " : data channel unregistered" << std::endl;
}

void RTCChannel::setDataChannel(webrtc::DataChannelInterface *channel){

	//if a channel already exists and is opened close it
	if(this->channel != NULL && this->channel->state() != webrtc::DataChannelInterface::DataState::kClosed) {

		//if a next channe already exosts and is opened, close it
		if(this->nextChannel != NULL && this->nextChannel->state() != webrtc::DataChannelInterface::DataState::kClosed) {
			this->nextChannel->Close();
		}

		this->nextChannel = channel;
		this->nextChannel->AddRef();
		this->close();
	}else{
		channel->AddRef();
		doSetDataChannel(channel);
	}

}


void RTCChannel::doSetDataChannel(webrtc::DataChannelInterface *channel){
	this->channel = channel;
	channel->RegisterObserver(this);
	std::cout << "[CH (" << this->name << ")] Connected !"<< std::endl;
}


void RTCChannel::setNegociationMessage(char * buffer, int bufferSize){
	this->negociationMessage = buffer;
	this->negociationMessageSize = bufferSize;

	//If negociation is set after the connection is established, immediately send the negociation message
	if(this->isConnected()){
		this->negociated = true;
		this->sendData(buffer, bufferSize);
	}
}

// The data channel state have changed.
void RTCChannel::OnStateChange(){
	if(!channel) return ;

	_channel_state = channel->state();

	std::cout << "[CH] (" << this->name << ")] State changed : "<< channel->state() << " !" <<std::endl;

	if(channel->state() == webrtc::DataChannelInterface::DataState::kOpen && this->negociationMessage != NULL){
		//When a data channel is open, first send the negociation messages
		this->negociated = true;
		this->sendData(this->negociationMessage, this->negociationMessageSize);

	}else if(channel->state() == webrtc::DataChannelInterface::DataState::kClosed){

		//When the datachannel is closed, unregister it
		this->negociated = false;
		this->unsetDataChannel();

		//If an channel is waiting to be set, do set it
		if(this->nextChannel){
			doSetDataChannel(this->nextChannel);
			this->nextChannel = NULL;
		}
	}
}
//  A data buffer was successfully received.
void RTCChannel::OnMessage(const webrtc::DataBuffer& buffer){

	if(recv_cb != NULL){
		float * b = (float*) buffer.data.data();
		this->recv_cb(b, buffer.data.size() * sizeof(char) / sizeof(float), this->recv_cb_data);
	}

}


bool RTCChannel::isConnected(){
	return _channel_state;
	//return channel != NULL && channel->state() == webrtc::DataChannelInterface::DataState::kOpen;
}

void RTCChannel::sendData(const char* buffer, int bufferSize){
	if(!this->negociated) return ;
	if(!this->channel->Send(webrtc::DataBuffer(rtc::CopyOnWriteBuffer(buffer, bufferSize),true))){
		std::cerr << "cannot send buffer (size:" << bufferSize << ")" << std::endl;
	}
}

void RTCChannel::registerReceiveCallback(ReceiveCallback cb, void * userData){
	this->recv_cb = cb;
	this->recv_cb_data = userData;
}

void RTCChannel::registerStreamCallback(StreamCallback cb, void * userData) {
	this->stream_cb = cb;
	this->stream_cb_data = userData;
}


void RTCChannel::close(){
	this->channel->Close();
	this->negociated = false;
}

}
