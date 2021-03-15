#include "mediadistributor.hpp"
#include <set>
#include "rtp.hpp"
#include "logcounter.hpp"
#include "peerconnection.hpp"
#include "track.hpp"

static rtc::LogCounter COUNTER_MEDIA_TRUNCATED(plog::warning,
											   "Number of RTP packets truncated over past second");
static rtc::LogCounter
		COUNTER_UNKNOWN_PACKET_TYPE(plog::warning,
									"Number of unknown RTCP packet types over past second");

void rtc::DefaultRTCPMediaDistributor::process(rtc::message_ptr message) {
	// Browsers like to compound their packets with a random SSRC.
	// we have to do this monstrosity to distribute the report blocks
	if (message->type == Message::Control) {
		std::set<uint32_t> ssrcs;
		size_t offset = 0;
		while ((sizeof(rtc::RTCP_HEADER) + offset) <= message->size()) {
			auto header = reinterpret_cast<rtc::RTCP_HEADER *>(message->data() + offset);
			if (header->lengthInBytes() > message->size() - offset) {
				COUNTER_MEDIA_TRUNCATED++;
				break;
			}
			offset += header->lengthInBytes();
			if (header->payloadType() == 205 || header->payloadType() == 206) {
				auto rtcpfb = reinterpret_cast<rtc::RTCP_FB_HEADER *>(header);
				ssrcs.insert(rtcpfb->getPacketSenderSSRC());
				ssrcs.insert(rtcpfb->getMediaSourceSSRC());

			} else if (header->payloadType() == 200 || header->payloadType() == 201) {
				auto rtcpsr = reinterpret_cast<rtc::RTCP_SR *>(header);
				ssrcs.insert(rtcpsr->senderSSRC());
				for (int i = 0; i < rtcpsr->header.reportCount(); ++i)
					ssrcs.insert(rtcpsr->getReportBlock(i)->getSSRC());
			} else if (header->payloadType() == 202) {
				continue;
				auto sdes = reinterpret_cast<rtc::RTCP_SDES *>(header);
				if (!sdes->isValid()) {
					PLOG_WARNING << "RTCP SDES packet is invalid";
					continue;
				}
				for (unsigned int i = 0; i < sdes->chunksCount(); i++) {
					auto chunk = sdes->getChunk(i);
					ssrcs.insert(chunk->ssrc());
				}
			} else {
				// PT=207 == Extended Report
				if (header->payloadType() != 207) {
					COUNTER_UNKNOWN_PACKET_TYPE++;
				}
			}
		}

		if (!ssrcs.empty()) {
			for (uint32_t ssrc : ssrcs) {
				if (auto track = getTrackFromSsrc(ssrc)) {
					distribute(track, message);
				}
			}
			return;
		}
	}
	uint32_t ssrc = uint32_t(message->stream);
	if (auto track = getTrackFromSsrc(ssrc)) {
		distribute(track, message);
	} else {
		/*
		 * TODO: So the problem is that when stop sending streams, we stop getting report blocks for
		 * those streams Therefore when we get compound RTCP packets, they are empty, and we can't
		 * forward them. Therefore, it is expected that we don't know where to forward packets. Is
		 * this ideal? No! Do I know how to fix it? No!
		 */
		// PLOG_WARNING << "Track not found for SSRC " << ssrc << ", dropping";
		return;
	}
}

std::shared_ptr<rtc::Track> rtc::MediaDistributor::getTrackFromSsrc(uint32_t ssrc) {
	if (auto pc = mPeerConnection.lock()) {
		return pc->getTrackFromSsrc(ssrc);
	}
	return nullptr;
}

void rtc::MediaDistributor::distribute(const std::shared_ptr<rtc::Track> &track, rtc::message_ptr ptr) {
	track->incoming(ptr);
}
