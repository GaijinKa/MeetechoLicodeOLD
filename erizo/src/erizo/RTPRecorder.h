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
	mblk_t *m;
	uint32_t ts, lastTs;
	unsigned long int firstSeq, lastSeq;
	int mlen, err, wlen;
	};
}
#endif /* RTPRECORDER_H_ */
