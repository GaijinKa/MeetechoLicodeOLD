/*
 * WebRTCConnection.cpp
 */
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <netinet/in.h>
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
    printf("Couldn't allocate data buffer.\n");
    return NULL;
  }
  if (!op) {
    printf("Couldn't allocate Ogg packet.\n");
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

  std::memcpy(data, identifier, 8);
  le32(data + 8, strlen(vendor));
  std::memcpy(data + 12, vendor, strlen(vendor));
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

	  if(buf == NULL) {
		printf("buf is null\n");
		ts += 960;
		return len; //FIXME ?? che devo far ritornare?
	  }
	  lastSeq = ((rtp_header_t*)buf)->seq_number;
	  if(firstSeq == 0) {
	  		firstSeq = lastSeq;
	  		printf("First seq: %lu\n", firstSeq);
	  }


	  ssrc = ((rtp_header_t*)buf)->ssrc;
	  version = ((rtp_header_t*)buf)->version;
      padbit = ((rtp_header_t*)buf)->padbit;
      extbit = ((rtp_header_t*)buf)->extbit;
      cc = ((rtp_header_t*)buf)->cc;
      markbit = ((rtp_header_t*)buf)->markbit;
      paytype = ((rtp_header_t*)buf)->paytype;


      blockcount = ((rtcpheader*)buf)->blockcount;
      padding = ((rtcpheader*)buf)->padding;
      versionrtcp = ((rtcpheader*)buf)->version;
      packettype = ((rtcpheader*)buf)->packettype;
      length = ((rtcpheader*)buf)->length;

	  printf("Stampa pacchetto RTP: \n");
	  printf(" Version : %u \n Padding : %u \n Mark : %u \n PayloadType : %u \n Ext : %u \n CC : %u \n ",
			  version, padbit, markbit, paytype, extbit, cc);

	  printf("Stampa pacchetto RTCP: \n");
	  printf(" Version : %u \n Padding : %u \n packettype : %u \n length : %u \n Blockcount : %u \n ",
			  versionrtcp, padding, packettype, length, blockcount);

	  //	  printf(" Version : %i \n Padding : %i \n Mark : %i \n PayloadType : %i \n Ext : %i \n CC : %i \n ",
//			  ((rtp_header_t*)buf)->version, (rtp_header_t*)buf)->padbit, (rtp_header_t*)buf)->markbit, (rtp_header_t*)buf)->paytype, (rtp_header_t*)buf)->cc);
//	  printf(" Seq_num : %i \n timestamp: %i \n ssrc: %i \n csrc : %i \n ",
//			  ((rtp_header_t*)buf)->seq_number, (rtp_header_t*)buf)->timestamp, (rtp_header_t*)buf)->ssrc, (rtp_header_t*)buf)->csrc);


	    if (bundle_){
	    	printf("Recorder for Bundle Communication\n");
	    	if (len <= 10) {
	    		printf("Packet length < 10");
	    		return len;
	    	}

	    	ogg_packet *op = op_from_pkt(reinterpret_cast<const unsigned char*> (buf+12), len-12);
	    	printf("\t\tWriting at position %lu (%lu)\n", lastSeq-firstSeq+1, 960*(lastSeq-firstSeq+1));
	    	op->granulepos = 960*(lastSeq-firstSeq+1); // FIXME: get this from the toc byte
	    	ogg_stream_packetin(params->stream, op);
	    	std::free(op);
	    	ogg_write(params);
	    	ts += 960;

	    } else {
	    	printf("Not Bundle");
	    	rtp_header_t* inHead = reinterpret_cast<rtp_header_t*> (buf);
	    	inHead->ssrc = htonl(44444);
	    	if (len <= 10) {
	    		printf("Packet length < 10");
	    		return len;
	    	}

	    	ogg_packet *op = op_from_pkt(reinterpret_cast<const unsigned char*> (buf)+12, len-12);
	    	printf("\t\tWriting at position %lu (%lu)\n", lastSeq-firstSeq+1, 960*(lastSeq-firstSeq+1));
	    	op->granulepos = 960*(lastSeq-firstSeq+1); // FIXME: get this from the toc byte
	    	ogg_stream_packetin(params->stream, op);
	    	std::free(op);
	    	ogg_write(params);
	    	ts += 960;
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
