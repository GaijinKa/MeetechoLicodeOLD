/*
 * WebRTCConnection.cpp
 */
#include <cstdio>
#include "RTPRecorder.h"

namespace erizo {

  RTPRecorder::RTPRecorder(){
    printf("RTPRecorder constructor called");
    bundle_ = false;
    videoReceiver_ = NULL;
    audioReceiver_ = NULL;
  }

  RTPRecorder::~RTPRecorder() {
	  this->close();
  }


  bool RTPRecorder::init(const std::string path) {
		state *params;
		ogg_packet *op;
		params = malloc(sizeof(state));
		if (!params) {
			fprintf(stderr, "Couldn't allocate param struct.\n");
			return false;
		}
		params->stream = malloc(sizeof(ogg_stream_state));
		if (!params->stream) {
			fprintf(stderr, "Couldn't allocate stream struct.\n");
			return false;
		}
		if (ogg_stream_init(params->stream, rand()) < 0) {
			fprintf(stderr, "Couldn't initialize Ogg stream state.\n");
			return false;
		}
		params->out = fopen(path, "wb");
		if (!params->out) {
			fprintf(stderr, "Couldn't open output file.\n");
			return false;
		}
		params->seq = 0;

		/* write stream headers */
		op = op_opushead();
		ogg_stream_packetin(params->stream, op);
		op_free(op);
		op = op_opustags();
		ogg_stream_packetin(params->stream, op);
		op_free(op);
		ogg_flush(params);

		return true;
  }

  void RTPRecorder::close() {
		fclose(params->out);
		ogg_stream_destroy(params->stream);
		free(params);
  }

  void RTPRecorder::start() {

  }

  void RTPRecorder::stop() {

  }

  void RTPRecorder::setAudioReceiver(MediaReceiver *receiv)	{
	 this->audioReceiver_ = receiv;
  }

  void RTPRecorder::setVideoReceiver(MediaReceiver *receiv)	{
	 this->videoReceiver_ = receiv;
  }

  int RTPRecorder::receiveAudioData(char* buf, int len) {

  }
  int RTPRecorder::receiveVideoData(char* buf, int len) {
	  printf("RTPRecorder received video %d \n", len);
  }

}
