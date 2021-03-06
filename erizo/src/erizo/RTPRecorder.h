#ifndef RTPRECORDER_H_
#define RTPRECORDER_H_

#include <string>
#include <vector>
#include <ogg/ogg.h>
#include "MediaDefinitions.h"
#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif
extern "C" {
#include <libavcodec/avcodec.h>	/* FFmpeg libavcodec */
#include <libavformat/avformat.h>	/* FFmpeg libavformat */
}

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
	bool initAudio(const std::string path, const std::string name, const std::string room);
	bool initVideo(const std::string path, const std::string name, const std::string room);
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

	int create_webm(int fps);
	void close_webm();
	int getCurrentState();

private:
	int bundle_, recorderState_;
	MediaReceiver* audioReceiver_;
	MediaReceiver* videoReceiver_;

	//audio
	state* params;
	ogg_packet *op;

	//video
	uint8_t *received_frame, *start_f;
	int frameLen, dec_errors, marker, frames, fps, step, vp8gotFirstKey, keyFrame, vp8w, vp8h, numBytes, lastSeq, video_lastTs;
	unsigned long int now, initTime, before, resync, lastOffset;
	//struct timespec tv;
	struct timeval tv;
	bool audioActive, videoActive;

	AVFormatContext *fctx;
	AVStream *vStream;
	AVCodec *vCodec;
	AVFrame *frame;
	std::string globalpath;
	};
}
#endif /* RTPRECORDER_H_ */
