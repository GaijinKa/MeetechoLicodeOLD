/*
 * WebRTCConnection.cpp
 */
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <netinet/in.h>
#include <opus/opus.h>
#include "RTPRecorder.h"

#define OPUS_PAYLOAD_TYPE 111

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

/* helper, write a big-endian 32 bit int to memory */
void be32(unsigned char *p, int v)
{
  p[0] = (v >> 24) & 0xff;
  p[1] = (v >> 16) & 0xff;
  p[2] = (v >> 8) & 0xff;
  p[3] = v & 0xff;
}

/* helper, write a big-endian 16 bit int to memory */
void be16(unsigned char *p, int v)
{
  p[0] = (v >> 8) & 0xff;
  p[1] = v & 0xff;
}

/* manufacture a generic OpusHead packet */
ogg_packet *op_opushead(void)
{
  int size = 19;
  unsigned char *data = (unsigned char *)malloc(size);
  ogg_packet *op = (ogg_packet *)malloc(sizeof(*op));

  if (!data) {
    printf("Couldn't allocate data buffer.\n");
    return NULL;
  }
  if (!op) {
    printf("Couldn't allocate Ogg packet.\n");
    return NULL;
  }

  memcpy(data, "OpusHead", 8);  /* identifier */
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

/* free a packet and its contents */
void op_free(ogg_packet *op) {
  if (op) {
    if (op->packet) {
      std::free(op->packet);
    }
    std::free(op);
  }
}

/* manufacture a generic OpusTags packet */
ogg_packet *op_opustags(void)
{
  char *identifier = const_cast<char *>("OpusTags");
  char *vendor = const_cast<char *>("opus rtp packet dump");
  int size = strlen(identifier) + 4 + strlen(vendor) + 4;
  unsigned char *data = (unsigned char *)malloc(size);
  ogg_packet *op = (ogg_packet *)malloc(sizeof(*op));

  if (!data) {
    printf("Couldn't allocate data buffer.\n");
    return NULL;
  }
  if (!op) {
    printf("Couldn't allocate Ogg packet.\n");
    return NULL;
  }

  memcpy(data, identifier, 8);
  le32(data + 8, strlen(vendor));
  memcpy(data + 12, vendor, strlen(vendor));
  le32(data + 12 + strlen(vendor), 0);

  op->packet = data;
  op->bytes = size;
  op->b_o_s = 0;
  op->e_o_s = 0;
  op->granulepos = 0;
  op->packetno = 1;

  return op;
}

ogg_packet *op_from_pkt(const unsigned char *pkt, int len)
{
  ogg_packet *op = (ogg_packet *)malloc(sizeof(*op));
  if (!op) {
    printf("Couldn't allocate Ogg packet.\n");
    return NULL;
  }

  op->packet = (unsigned char *)pkt;
  op->bytes = len;
  op->b_o_s = 0;
  op->e_o_s = 0;

  return op;
}

/* helper, write out available ogg pages */
int ogg_write(state *params)
{
  ogg_page page;
  size_t written;

  if (!params || !params->stream || !params->out) {
    return -1;
  }

  while (ogg_stream_pageout(params->stream, &page)) {
    written = fwrite(page.header, 1, page.header_len, params->out);
    if (written != (size_t)page.header_len) {
      printf("Error writing Ogg page header\n");
      return -2;
    }
    written = fwrite(page.body, 1, page.body_len, params->out);
    if (written != (size_t)page.body_len) {
      printf("Error writing Ogg page body\n");
      return -3;
    }
  }

  return 0;
}



/* helper, flush remaining ogg data */
int ogg_flush(state *params)
{
  ogg_page page;
  size_t written;

  if (!params || !params->stream || !params->out) {
	printf("Error ogg_flush\n");
    return -1;
  }

  while (ogg_stream_flush(params->stream, &page)) {
    written = fwrite(page.header, 1, page.header_len, params->out);
    if (written != (size_t)page.header_len) {
      printf("Error writing Ogg page header\n");
      return -2;
    }
    written = fwrite(page.body, 1, page.body_len, params->out);
    if (written != (size_t)page.body_len) {
      printf("Error writing Ogg page body\n");
      return -3;
    }
  }

  return 0;
}

/*************** SEROVONO? ***********************/
/* helper, read a big-endian 16 bit int from memory */
static int rbe16(const unsigned char *p)
{
  int v = p[0] << 8 | p[1];
  return v;
}

/* helper, read a big-endian 32 bit int from memory */
static int rbe32(const unsigned char *p)
{
  int v = p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
  return v;
}

/* helper, read a native-endian 32 bit int from memory */
static int rne32(const unsigned char *p)
{
  /* On x86 we could just cast, but that might not meet
   * arm alignment requirements. */
  int d = 0;
  memcpy(&d, p, 4);
  return d;
}
/*******************************************************/

int parse_rtp_header(const unsigned char *packet, int size, rtp_header *rtp)
{
  if (!packet || !rtp) {
    return -2;
  }
  if (size < RTP_HEADER_MIN) {
    printf("Packet too short for rtp\n");
    return -1;
  }
  rtp->version = (packet[0] >> 6) & 3;
  rtp->pad = (packet[0] >> 5) & 1;
  rtp->ext = (packet[0] >> 4) & 1;
  rtp->cc = packet[0] & 7;
  rtp->header_size = 12 + 4 * rtp->cc;
  rtp->payload_size = size - rtp->header_size;

  rtp->mark = (packet[1] >> 7) & 1;
  rtp->type = (packet[1]) & 127;
  rtp->seq  = rbe16(packet + 2);
  rtp->time = rbe32(packet + 4);
  rtp->ssrc = rbe32(packet + 8);
  rtp->csrc = NULL;
  if (size < rtp->header_size) {
    printf("Packet too short for RTP header\n");
    return -1;
  }

  return 0;
}

/* calculate the number of samples in an opus packet */
int opus_samples(const unsigned char *packet, int size)
{
  /* number of samples per frame at 48 kHz */
  int samples = opus_packet_get_samples_per_frame(packet, 48000);
  /* number "frames" in this packet */
  int frames = opus_packet_get_nb_frames(packet, size);

  return samples*frames;
}

namespace erizo {

  RTPRecorder::RTPRecorder(){
    printf("RTPRecorder constructor called");
    bundle_ = false;
    videoReceiver_ = NULL;
    audioReceiver_ = NULL;
	ts = 0, lastTs = 0;
	firstSeq = 0, lastSeq = 0;
	mlen = 0, err = 0, wlen = 0;
  }

  RTPRecorder::~RTPRecorder() {
	  this->close();
  }


  bool RTPRecorder::init(std::string path) {
	    printf("initializing RTPRecorder");
		params = (state *)malloc(sizeof(state));
		if (!params) {
			printf("Couldn't allocate param struct.\n");
			return false;
		}
		params->stream = (ogg_stream_state *)malloc(sizeof(ogg_stream_state));
		if (!params->stream) {
			printf("Couldn't allocate stream struct.\n");
			return false;
		}
		if (ogg_stream_init(params->stream, rand()) < 0) {
			printf("Couldn't initialize Ogg stream state.\n");
			return false;
		}
		params->out = fopen(path.c_str(), "w+");
		if (!params->out) {
			printf("Couldn't open output file.\n");
			return false;
		} else {
			printf("File opened in %s.\n",path.c_str());
		}
		  params->seq = 0;
		  params->granulepos = 0;

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
	   ogg_flush(params);
	   /* clean up */
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

	  const unsigned char *packet;
	  int size;
	  rtp_header rtp;
	  packet = reinterpret_cast<const unsigned char*> (buf);
	  size = len;

	  if (parse_rtp_header(packet, size, &rtp)) {
	      printf("error parsing rtp header\n");
	      return len;
	    }
	    printf("  rtp 0x%08x %d %d %d",
	            rtp.ssrc, rtp.type, rtp.seq, rtp.time);
	    printf("  v%d %s%s%s CC %d", rtp.version,
	            rtp.pad ? "P":".", rtp.ext ? "X":".",
	            rtp.mark ? "M":".", rtp.cc);
	    printf(" %5d bytes\n", rtp.payload_size);

	    packet += rtp.header_size;
	    size -= rtp.header_size;

	    if (size < 0) {
	      printf("skipping short packet\n");
	      return len;
	    }

	    if (rtp.seq < params->seq) {
	      printf("skipping out-of-sequence packet\n");
	      return len;
	    }
	    params->seq = rtp.seq;

	    if (rtp.type != OPUS_PAYLOAD_TYPE) {
	       printf("skipping non-opus packet\n");
	       return len;
	     }


	    if (bundle_){
	    	printf("Recorder for Bundle Communication\n");
	    	if (len <= 10) {
	    		printf("Packet length < 10");
	    		return len;
	    	}

	    	 /* write the payload to our opus file */
	    	  ogg_packet *op = op_from_pkt(packet, size);
	    	  op->packetno = rtp.seq;
	    	  params->granulepos += opus_samples(packet, size);
	    	  op->granulepos = params->granulepos;
	    	  ogg_stream_packetin(params->stream, op);
	    	  std::free(op);
	    	  ogg_write(params);

	    	  if (size < rtp.payload_size) {
	    	    fprintf(stderr, "!! truncated %d uncaptured bytes\n",
	    	            rtp.payload_size - size);
	    	  }

	    } else {
	    	printf("Not Bundle");
	    		//Missing
	    }

	  return 0;

  }

  int RTPRecorder::receiveVideoData(char* buf, int len) {
	//  printf("RTPRecorder received video %d \n", len);
  }

  void RTPRecorder::setBundle(int bund) {
	  printf("setting recorder bundle to %d \n", bund);
	  bundle_ = bund;
  }

}
