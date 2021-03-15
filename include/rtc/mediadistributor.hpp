//
// Created by staz on 3/15/21.
//

#ifndef WEBRTC_SERVER_MEDIADISTRIBUTOR_HPP
#define WEBRTC_SERVER_MEDIADISTRIBUTOR_HPP
#include "include.hpp"
#include "message.hpp"
namespace rtc {
class PeerConnection;
class Track;

class MediaDistributor {
private:
	std::weak_ptr<rtc::PeerConnection> mPeerConnection;
public:
	MediaDistributor(std::weak_ptr<rtc::PeerConnection> peerConnection): mPeerConnection(std::move(peerConnection)) {}
	virtual void process(rtc::message_ptr ptr) = 0;

	std::shared_ptr<rtc::Track> getTrackFromSsrc(uint32_t ssrc);
	void distribute(const std::shared_ptr<rtc::Track>& track, rtc::message_ptr ptr);
};

class DefaultRTCPMediaDistributor : public MediaDistributor {
public:
	DefaultRTCPMediaDistributor(std::weak_ptr<rtc::PeerConnection> peerConnection): MediaDistributor(peerConnection) {}
	void process(rtc::message_ptr ptr) override;

private:
	};
}
#endif //WEBRTC_SERVER_MEDIADISTRIBUTOR_HPP
