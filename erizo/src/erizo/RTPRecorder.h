#ifndef RTPRECORDER_H_
#define RTPRECORDER_H_

#include <string>
#include <ogg/ogg.h>
#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif
#include <libavcodec/avcodec.h>	/* FFmpeg libavcodec */
#include <libavformat/avformat.h>	/* FFmpeg libavformat */
#include "MediaDefinitions.h"

typedef struct {
  ogg_stream_state *stream;
  FILE *out;
  int seq;
  ogg_int64_t granulepos;
  int linktype;
} state;

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

	//audio
	state* params;
	ogg_packet *op;

	//video
	uint8_t *received_frame, *buffer, *start_f;
	int frameLen, dec_errors, have_more, mlen, marker, wlen, frames, fps, step, vp8gotFirstKey, keyFrame, vp8w, vp8h, numBytes;
	uint32_t video_ts, video_lastTs;
	unsigned long int lastSeq, now, before, resync;
	struct timeval tv;

	AVCodecContext *dec_context;	/* FFmpeg decoding context */
	AVCodec *dec_codec;		/* FFmpeg decoding codec */
	AVFormatContext *fctx;
	AVStream *vStream;
	AVCodec *vCodec;
	AVFrame *frame;
	};
}
#endif /* RTPRECORDER_H_ */
