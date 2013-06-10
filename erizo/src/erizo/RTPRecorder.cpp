/*
 * WebRTCConnection.cpp
 */
#include <cstdio>
#include <cstdlib>
#include "RTPRecorder.h"

/* helper, write a little-endian 32 bit int to memory */
void le32(unsigned char *p, int v)
{
  p[0] = v & 0xff;
  p[1] = (v >> 8) & 0xff;
  p[2] = (v >> 16) & 0xff;
  p[3] = (v >> 24) & 0xff;
}


/* helper, write a little-endian 16 bit int to memory */
void le16(unsigned char *p, int v)
{
  p[0] = v & 0xff;
  p[1] = (v >> 8) & 0xff;
}

/* manufacture a generic OpusHead packet */
ogg_packet *op_opushead(void)
{
  int size = 19;
  unsigned char *data = (unsigned char *)malloc(size);
  ogg_packet *op = (ogg_packet *)malloc(sizeof(*op));

  if (!data) {
    fprintf(stderr, "Couldn't allocate data buffer.\n");
    return NULL;
  }
  if (!op) {
    fprintf(stderr, "Couldn't allocate Ogg packet.\n");
    return NULL;
  }

  std::memcpy(data, "OpusHead", 8);  /* identifier */
  data[8] = 1;                  /* version */
  data[9] = 2;                  /* channels */
  le16(data+10, 0);             /* pre-skip */
  le32(data + 12, 48000);       /* original sample rate */
  le16(data + 16, 0);           /* gain */
  data[18] = 0;                 /* channel mapping family */

  op->packet = data;
  op->bytes = size;
  op->b_o_s = 1;
  op->e_o_s = 0;
  op->granulepos = 0;
  op->packetno = 0;

  return op;
}

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


  bool RTPRecorder::init(std::string path) {
	    fprintf(stderr, "initializing RTPRecorder");
		params = (state *)malloc(sizeof(state));
		if (!params) {
			fprintf(stderr, "Couldn't allocate param struct.\n");
			return false;
		}
		params->stream = (ogg_stream_state *)malloc(sizeof(ogg_stream_state));
		if (!params->stream) {
			fprintf(stderr, "Couldn't allocate stream struct.\n");
			return false;
		}
		if (ogg_stream_init(params->stream, rand()) < 0) {
			fprintf(stderr, "Couldn't initialize Ogg stream state.\n");
			return false;
		}
		params->out = fopen(path.c_str(), "wb");
		if (!params->out) {
			fprintf(stderr, "Couldn't open output file.\n");
			return false;
		}
		params->seq = 0;

		/* write stream headers*/
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
		std::free(params);
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
	  printf("RTPRecorder received audio %d \n", len);
  }
  int RTPRecorder::receiveVideoData(char* buf, int len) {
	  printf("RTPRecorder received video %d \n", len);
  }

}
