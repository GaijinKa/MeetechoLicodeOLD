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

typedef struct {
	uint32_t blockcount :5;
	uint32_t padding :1;
	uint32_t version :2;
	uint32_t packettype :8;
	uint32_t length :16;
	uint32_t ssrc;
  uint32_t ssrcsource;
  uint32_t fractionLost:8;
} rtcpheader;

/*typedef struct rtp_header
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
} rtp_header_t;*/

#define RTP_HEADER_MIN 12
typedef struct {
  int version;
  int type;
  int pad, ext, cc, mark;
  int seq, time;
  int ssrc;
  int *csrc;
  int header_size;
  int payload_size;
} rtp_header;

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
	int parse_rtp_header(const unsigned char *packet, int size, rtp_header *rtp)
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
	uint32_t ts, lastTs, ssrc, blockcount, padding, versionrtcp, packettype, length;
	//uint16_t version, padbit, extbit, cc, markbit, paytype, seq_number;
	uint16_t seq_number;
	int version, padbit, extbit, cc, markbit, paytype, seqnumber, hSize;
	unsigned long int firstSeq, lastSeq;
	int mlen, err, wlen;
	};
}
#endif /* RTPRECORDER_H_ */
