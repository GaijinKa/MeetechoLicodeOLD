#ifndef RTPRECORDER_H_
#define RTPRECORDER_H_

#include <string>
#include <ogg/ogg.h>
#include "MediaDefinitions.h"

typedef struct {
	ogg_stream_state *stream;
	FILE *out;
	int seq;
} state;

typedef struct rtp_header
{
#if __BYTE_ORDER == __BIG_ENDIAN
        uint16_t version:2;
        uint16_t padbit:1;
        uint16_t extbit:1;
        uint16_t cc:4;
        uint16_t markbit:1;
        uint16_t paytype:7;
#else
        uint16_t cc:4;
        uint16_t extbit:1;
        uint16_t padbit:1;
        uint16_t version:2;
        uint16_t paytype:7;
        uint16_t markbit:1;
#endif
        uint16_t seq_number;
        uint32_t timestamp;
        uint32_t ssrc;
        uint32_t csrc[16];
} rtp_header_t;

namespace erizo {

class RTPRecorder: public MediaReceiver {
public:
	/**
	 * Constructor.
	 * Constructs an empty RTPRecorder without any configuration.
	 */
	RTPRecorder();
	/**
	 * Destructor.
	 */
	virtual ~RTPRecorder();
	/**
	 * Inits the RTPRecorder Interacting with filsystem.
	 * @return True if the file opens correctly.
	 */
	bool init(const std::string path);
	/**
	 * Closes the RTPRecorder.
	 * The object cannot be used after this call.
	 */
	void close();
	/**
	 * Starts the RTPRecorder.
	 */
	void start();
	/**
	 * Stops the RTPRecorder.
	 */
	void stop();
	/**
	 * set bundle_ var.
	 */
	void setBundle(int bund);

	int receiveAudioData(char* buf, int len);
	int receiveVideoData(char* buf, int len);

	/**
	 * Sets a MediaReceiver that is going to receive Audio Data
	 * @param receiv The MediaReceiver to send audio to.
	 */
	void setAudioReceiver(MediaReceiver *receiv);
	/**
	 * Sets a MediaReceiver that is going to receive Video Data
	 * @param receiv The MediaReceiver
	 */
	void setVideoReceiver(MediaReceiver *receiv);

private:
	int bundle_;
	MediaReceiver* audioReceiver_;
	MediaReceiver* videoReceiver_;
	state* params;
	ogg_packet *op;
	uint32_t ts, lastTs, blockcount, padding, versionrtcp, packettype, length;
	uint16_t version, padbit, extbit, cc, markbit, paytype, seq_number;
	unsigned long int firstSeq, lastSeq;
	int mlen, err, wlen;
	};
}
#endif /* RTPRECORDER_H_ */
