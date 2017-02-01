/*
 * MemoryCapturer.hpp
 *
 *  Created on: 07 mar 2016
 *      Author: jfellus
 */

#ifndef MEMORYCAPTURER_HPP_
#define MEMORYCAPTURER_HPP_

#include "webrtc/media/base/videocapturer.h"
#include "webrtc/media/base/videocapturerfactory.h"
#include <stdlib.h>
#include <stdint.h>

namespace maya {



	class MemoryCapturer : public cricket::VideoCapturer {


		public:
			MemoryCapturer(uint w, uint h);
			~MemoryCapturer();

			void ResetSupportedFormats(const std::vector<cricket::VideoFormat>& formats) { SetSupportedFormats(formats); }
			bool CaptureFrame(const char* img, int w, int h);

			sigslot::signal1<MemoryCapturer*> SignalDestroyed;

			virtual cricket::CaptureState Start(const cricket::VideoFormat& format);
			virtual void Stop();
			virtual bool IsRunning() { return running_; }
			void SetScreencast(bool is_screencast) { is_screencast_ = is_screencast; }
			virtual bool IsScreencast() const { return is_screencast_; }
			bool GetPreferredFourccs(std::vector<uint32_t>* fourccs);

			void SetRotation(webrtc::VideoRotation rotation) { rotation_ = rotation; }
			webrtc::VideoRotation GetRotation() { return rotation_; }

		private:
			bool running_;
			int64_t initial_unix_timestamp_;
			int64_t next_timestamp_;
			bool is_screencast_;
			webrtc::VideoRotation rotation_;
	};


}





#endif /* MEMORYCAPTURER_HPP_ */
