/*
 * MemoryCapturer.cpp
 *
 *  Created on: 07 mar 2016
 *      Author: jfellus
 */
#include <iostream>

#include "MemoryCapturer.hpp"

#include <libyuv/convert.h>
#include <webrtc/common_video/include/video_frame_buffer.h>




namespace maya {
	MemoryCapturer::MemoryCapturer(uint w, uint h)
		: cricket::VideoCapturer(), running_(false),
		initial_unix_timestamp_(time(NULL) * rtc::kNumNanosecsPerSec),
		next_timestamp_(rtc::kNumNanosecsPerMillisec),
		is_screencast_(false),
		rotation_(webrtc::kVideoRotation_0) {

			std::vector<cricket::VideoFormat> formats;
			formats.push_back(cricket::VideoFormat(w, h, cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
			ResetSupportedFormats(formats);
		}

	MemoryCapturer::~MemoryCapturer() {
		SignalDestroyed(this);
	}

	bool MemoryCapturer::CaptureFrame(const char* img, int w, int h) {
		if (!GetCaptureFormat()) {
			return false;
		}
		if (!running_) return false;

		rtc::scoped_refptr<webrtc::I420Buffer> framebuffer = webrtc::I420Buffer::Create(w, h);
		
		libyuv::RAWToI420((uint8*)img, 
			w*3, 
			framebuffer->MutableDataY(),
			framebuffer->StrideY(),
			framebuffer->MutableDataU(),
			framebuffer->StrideU(),
			framebuffer->MutableDataV(),
			framebuffer->StrideV(),
			w,
			h);

		OnFrame(webrtc::VideoFrame(framebuffer, rotation_, next_timestamp_ / rtc::kNumNanosecsPerMicrosec), w, h);
	
		next_timestamp_ += 33333333;
		
		return true;
	}


	cricket::CaptureState MemoryCapturer::Start(const cricket::VideoFormat& format) {
		cricket::VideoFormat supported;
		if (GetBestCaptureFormat(format, &supported)) SetCaptureFormat(&supported);
		running_ = true;
		SetCaptureState(cricket::CS_RUNNING);
		return cricket::CS_RUNNING;
	}

	void MemoryCapturer::Stop() {
		running_ = false;
		SetCaptureFormat(NULL);
		SetCaptureState(cricket::CS_STOPPED);
	}

	bool MemoryCapturer::GetPreferredFourccs(std::vector<uint32_t>* fourccs) {
		fourccs->push_back(cricket::FOURCC_ARGB);
		return true;
	}



}
