/*
 * RTPRecorder.cpp
 * Note per ora è usata la divisione tra bundle_ e not bundle_ ma credo che non ce ne sia bisogno anche perchè noi passiamo l'attributo name
 * che identifica lo stream, e questo fa sempre scattare il bundle con i dati.
 */
#include <cstdio>
#include <ctime>
#include <cstring>
#include <cstdlib>
#include <netinet/in.h>
#include <opus/opus.h>

#include "RTPRecorder.h"
#include <signal.h>

/******VIDEO RECORDING******/


/* WebRTC stuff (VP8) */
#if defined(__ppc__) || defined(__ppc64__)
	# define swap2(d)  \
	((d&0x000000ff)<<8) |  \
	((d&0x0000ff00)>>8)
#else
	# define swap2(d) d
#endif



/******AUDIO RECORDING******/
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
	lastSeq = 0;
 	numBytes = 640*480*3, frameLen = 0, marker = 0, frames = 0, fps = 0, step = 0, vp8gotFirstKey = 0, keyFrame = 0, vp8w = 0, vp8h = 0;
   	frame = NULL;
   //	video_ts = 0, video_lastTs=0;
   	now = 0, before = 0, resync = 0;
  }

  RTPRecorder::~RTPRecorder() {
	  this->close();
  }


  bool RTPRecorder::initAudio(std::string path, std::string name) {

		struct timespec tsp;
		clock_gettime(CLOCK_MONOTONIC, &tsp);

	    printf("initializing RTPRecorder \n");
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

  bool RTPRecorder::initVideo(std::string path, std::string name) {
	  av_register_all();
      received_frame = (uint8_t *)calloc(numBytes, sizeof(uint8_t));
      memset(received_frame, 0, numBytes);
      buffer = (uint8_t *)calloc(10000, sizeof(uint8_t));
      memset(buffer, 0, 10000);

      return true;
  }

  void RTPRecorder::close() {
	   ogg_flush(params);
	   /* clean up */
	   fclose(params->out);
	   ogg_stream_destroy(params->stream);
	   std::free(params);

	   close_webm();
	   std::free(buffer);
	   std::free(received_frame);


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
	    printf(" AUDIO rtp 0x%08x %d %d %d",
	            rtp.ssrc, rtp.type, rtp.seq, rtp.time);
	    printf("  v%d %s%s%s CC %d", rtp.version,
	            rtp.pad ? "P":".", rtp.ext ? "X":".",
	            rtp.mark ? "M":".", rtp.cc);
	    printf(" %5d bytes\n", rtp.payload_size);

	    packet += rtp.header_size;
	    size -= rtp.header_size;

	    if (size < 0) {
	      printf("AUDIO skipping short packet\n");
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
	    	    printf("!! truncated %d uncaptured bytes\n",
	    	            rtp.payload_size - size);
	    	  }

	    } else {
	    	printf("Not Bundle");
	    		//Missing
	    }

	  return 0;

  }

  int RTPRecorder::receiveVideoData(char* buf, int len) {

	  if (resync==0) {
    	  //test - inserisco qui l'inizio del fps?
     	  clock_gettime(CLOCK_MONOTONIC, &tv);
     	  before = tv.tv_sec*1000 + tv.tv_nsec;
          resync = before;
     	  printf("Starting fps evaluation (start_f=%lu)...\n", before);
     	  printf("Waiting for RTP frames...\n");
	  }


	  const unsigned char *packet;
	  int size;
	  rtp_header rtp_v;
	  packet = reinterpret_cast<const unsigned char*> (buf);
	  size = len;

	  if (parse_rtp_header(packet, size, &rtp_v)) {
	      printf("VIDEO error parsing rtp header\n");
	      return len;
	    }
	    printf(" VIDEO rtp 0x%08x %d %d %d",
	    		rtp_v.ssrc, rtp_v.type, rtp_v.seq, rtp_v.time);
	    printf("  v%d %s%s%s CC %d", rtp_v.version,
	    		rtp_v.pad ? "P":".", rtp_v.ext ? "X":".",
	    		rtp_v.mark ? "M":".", rtp_v.cc);
	    printf(" %5d bytes\n", rtp_v.payload_size);

	    packet += rtp_v.header_size;
	    size -= rtp_v.header_size;

	    if (size < 0) {
	      printf("VIDEO skipping short packet\n");
	      return len;
	    }

	  //video_ts = rtp_v.time;
//	  if (video_ts==video_lastTs) { 	//continue encoding
		  if((rtp_v.seq-lastSeq) > 1)
			  printf("VIDEO unexpected seq (%d, should have been %d)!\n", rtp_v.seq, lastSeq);

		  lastSeq = rtp_v.seq;
		  printf("VIDEO lastSeq setted to %d \nVIDEO copying packet in buffer...   ",lastSeq);
		  memcpy(buffer, packet, size);
		  printf("VIDEO packet copied in buffer\n");
		  //First VP8 Header Line
		  int skipped = 1;
		  size--;
		  uint8_t vp8pd = *buffer;
		  uint8_t xbit = (vp8pd & 0x80);
		  uint8_t rbit = (vp8pd & 0x40);
		  uint8_t nbit = (vp8pd & 0x20);
		  uint8_t sbit = (vp8pd & 0x10);
		  uint8_t partid = (vp8pd & 0x0F);
		  printf("VIDEO first VP8 Header Readed\n");

		  if(!xbit) {	// Just skip the first byte
			  buffer++;
			  printf("VIDEO Xbit not marked -> go ahead\n");
		  } else {   // XLine
			  printf("VIDEO Xbit marked -> reading..\n");
			  buffer++;
			  size--;
			  skipped++;
			  vp8pd = *buffer;
			  uint8_t ibit = (vp8pd & 0x80);
			  uint8_t lbit = (vp8pd & 0x40);
			  uint8_t tbit = (vp8pd & 0x20);
			  uint8_t kbit = (vp8pd & 0x10);
			  if(ibit) {	// Read the PictureID octet
				  printf("VIDEO Ibit marked -> reading..\n");
				  buffer++;
				  size--;
				  skipped++;
				  vp8pd = *buffer;
				  uint16_t picid = vp8pd, wholepicid = picid;
				  uint8_t mbit = (vp8pd & 0x80);
				  if(mbit) {
					  printf("VIDEO Mbit marked -> reading..\n");
					  memcpy(&picid, buffer, sizeof(uint16_t));
					  wholepicid = ntohs(picid);
					  picid = (wholepicid & 0x7FFF);
					  buffer++;
					  size--;
					  skipped++;
				  }
			  }
			  if(lbit) {	// Read the TL0PICIDX octec
				  printf("VIDEO Lbit marked -> reading..\n");
				  buffer++;
				  size--;
				  skipped++;
				  vp8pd = *buffer;
			  }
			  if(tbit || kbit) { // Read the TID/KEYIDX octec
				  printf("VIDEO Tbit or Kbit marked -> reading..\n");
				  buffer++;
				  size--;
				  skipped++;
				  vp8pd = *buffer;
			  }
			  buffer++;	// Now we're in the payload
			  printf("VIDEO Now we're in payload\n");
			  if(sbit) {
				  printf("VIDEO Sbit marked -> reading..\n");
				  unsigned long int vp8ph = 0;
				  memcpy(&vp8ph, buffer, 4);
				  printf("VIDEO Buffer copied in vp8ph (Vp8 Payload Header?)\n");
				  vp8ph = ntohl(vp8ph);
				  uint8_t size0 = ((vp8ph & 0xE0000000) >> 29);
				  uint8_t hbit = ((vp8ph & 0x10000000) >> 28);
				  uint8_t ver = ((vp8ph & 0x0E000000) >> 25);
				  uint8_t pbit = ((vp8ph & 0x01000000) >> 24);
				  uint8_t size1 = ((vp8ph & 0x00FF0000) >> 16);
				  uint8_t size2 = ((vp8ph & 0x0000FF00) >> 8);
				  int fpSize = size0 + 8 * size1 + 2048 * size2;
				  if(!pbit) {
					  printf("VIDEO Pbit not marked! is a KeyFrame? -> reading..\n");
					  vp8gotFirstKey = 1;
					  keyFrame = 1;
					  // Get resolution
					  unsigned char *c = buffer+3;
					  // vet via sync code
					  if(c[0]!=0x9d||c[1]!=0x01||c[2]!=0x2a)
						  printf("First 3-bytes after header not what they're supposed to be?\n");

					  vp8w = swap2(*(unsigned short*)(c+3))&0x3fff;
					  int vp8ws = swap2(*(unsigned short*)(c+3))>>14;
					  vp8h = swap2(*(unsigned short*)(c+5))&0x3fff;
					  int vp8hs = swap2(*(unsigned short*)(c+5))>>14;
					  printf("VP8 source: %dx%d (scale=%dx%d)\n", vp8w, vp8h, vp8ws, vp8hs);
					  if(dec_context) {
						  dec_context->coded_width = vp8w;
						  dec_context->coded_height = vp8h;
					  }
				  }
			  }
		  }
		  /* Frame manipulation */
		  printf("VIDEO End of all reading, Starting Frame Manipulation..\n");
		  printf("VIDEO frameLen %i\n", frameLen);
		  printf("VIDEO received_frame %lu\n", received_frame);
		  printf("VIDEO size %d\n", size);
		  memcpy(&(received_frame)+frameLen, buffer, size);
		  frameLen += size;
		  if(rtp_v.mark) {	/* Marker bit is set, the frame is complete */
			  printf("VIDEO MarkBit marked (!!!) -> start dumping..\n");
			  //video_lastTs = rtp_v.time;
			  if(frameLen > 0) {
				  printf("VIDEO the frame is not null -> go ahead..\n");
				  memset(&(received_frame)+frameLen, 0, FF_INPUT_BUFFER_PADDING_SIZE);
				  printf("VIDEO memset called correctly -> go ahead..\n");
				  frame = avcodec_alloc_frame();
				  printf("VIDEO Allocated Frame, configuring AVPacket\n");

				  AVPacket packet;
				  av_init_packet(&packet);
				  printf("VIDEO AVPacket initialized... ");
				  packet.stream_index = 0;
				  packet.data = received_frame;
				  packet.size = frameLen;
				  if(keyFrame)
					  packet.flags |= AV_PKT_FLAG_KEY;

				  /* First we save to the file... */
				  printf(" ### Writing frame to file...\n");
				  packet.dts = AV_NOPTS_VALUE;
				  packet.pts = AV_NOPTS_VALUE;
				  printf("VIDEO AVPacket SettedUp\n");

				  if(fctx) {
					  if(av_write_frame(fctx, &packet) < 0)
						  printf("Error writing video frame to file...\n");
					  else
						  printf(" ### ### frame written (pts=%d)...\n", packet.pts);
				  } else {
					  printf("Still waiting for fps evaluation to create the file...\n");
				  }
				  /* Try evaluating the incoming FPS */
				  frames++;
		     	  clock_gettime(CLOCK_MONOTONIC, &tv);
		     	  before = tv.tv_sec*1000 + tv.tv_nsec;
				  if((now-before) >= 1000) {	/* Evaluate every second */
					  printf("fps=%d (in %lu ms)\n", frames, now-before);
					  if(fps == 0) {
						  /* Adapt framerate: this is just an evaluation (FIXME) */
						  if(frames > 27)
							  fps = 30;
						  else if(frames > 22)
							  fps = 25;
						  else if(frames > 17)
							  fps = 20;
						  else if(frames > 12)
							  fps = 15;
						  else if(frames > 7)
							  fps = 10;
						  else if(frames > 2)
							  fps = 5;
						  else //TEST PER VEDERE SE IL PROB E' QUI
							  fps = 15;
						  printf("Creating WebM file: %d fps\n", fps);
						  create_webm(fps);
					  }
					  frames = 0;
					  before = now;
				  }

				  printf("VIDEO Resetting all 4 next cycle of reading\n");
				  //video_lastTs = rtp_v.time;
				  keyFrame = 0;
				  frameLen = 0;
			  }
			  //video_ts += step;	/* FIXME was 4500, but this implied fps=20 at max */
		  }
//		  if(size == 0)
//			  return size;
//	  }
		  printf("VIDEO Returning Video Receive Function\n\n\n");
		  return 0;
  }

  void RTPRecorder::setBundle(int bund) {
	  printf("setting recorder bundle to %d \n", bund);
	  bundle_ = bund;
  }

  /* Create WebM context and file */
  int RTPRecorder::create_webm(int fps) {
  	/* WebM output */
  	fctx = avformat_alloc_context();
  	if(fctx == NULL) {
  		printf("Error allocating context\n");
  		return -1;
  	}
  	//~ fctx->oformat = guess_format("webm", NULL, NULL);
  	fctx->oformat = av_guess_format("webm", NULL, NULL);
  	if(fctx->oformat == NULL) {
  		printf("Error guessing format\n");
  		return -1;
  	}
  	snprintf(fctx->filename, sizeof(fctx->filename), "/home/ubuntu/lynckia/recorded/rtpdump-src.webm");
  	//~ vStream = av_new_stream(fctx, 0);
  	vStream = avformat_new_stream(fctx, 0);
  	if(vStream == NULL) {
  		printf("Error adding stream\n");
  		return -1;
  	}
  	//~ avcodec_get_context_defaults2(vStream->codec, CODEC_TYPE_VIDEO);
  	avcodec_get_context_defaults2(vStream->codec, AVMEDIA_TYPE_VIDEO);
  	printf("VIDEO get context");
  	vStream->codec->codec_id = CODEC_ID_VP8;
  	printf("VIDEO defined codec id");
  //	vStream->codec->codec_id = AV_CODEC_ID_VP8;
  	//~ vStream->codec->codec_type = CODEC_TYPE_VIDEO;
  	vStream->codec->codec_type = AVMEDIA_TYPE_VIDEO;
  	printf("VIDEO defined codec id");
  	vStream->codec->time_base = (AVRational){1, fps};
  	vStream->codec->width = 640;
  	vStream->codec->height = 480;
  	printf("VIDEO defined codec id");
  	vStream->codec->pix_fmt = PIX_FMT_YUV420P;
  	if (fctx->flags & AVFMT_GLOBALHEADER)
  		vStream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
  	//~ fctx->timestamp = 0;
  	//~ if(url_fopen(&fctx->pb, fctx->filename, URL_WRONLY) < 0) {
  	if(avio_open(&fctx->pb, fctx->filename, AVIO_FLAG_WRITE) < 0) {
  		printf("Error opening file for output\n");
  		return -1;
  	}
  	//~ memset(&parameters, 0, sizeof(AVFormatParameters));
  	//~ av_set_parameters(fctx, &parameters);
  	//~ fctx->preload = (int)(0.5 * AV_TIME_BASE);
  	//~ fctx->max_delay = (int)(0.7 * AV_TIME_BASE);
  	//~ if(av_write_header(fctx) < 0) {
  	if(avformat_write_header(fctx, NULL) < 0) {
  		printf("Error writing header\n");
  		return -1;
  	}
  	return 0;
  }

  /* Close WebM file */
  void RTPRecorder::close_webm() {
  	if(fctx != NULL)
  		av_write_trailer(fctx);
  	if(vStream->codec != NULL)
  		avcodec_close(vStream->codec);
  	if(fctx->streams[0] != NULL) {
  		av_free(fctx->streams[0]->codec);
  		av_free(fctx->streams[0]);
  	}
  	if(fctx != NULL) {
  		//~ url_fclose(fctx->pb);
  		avio_close(fctx->pb);
  		av_free(fctx);
  	}
  }



}
