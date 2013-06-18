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
    cout << "Couldn't allocate data buffer." << endl;
    return NULL;
  }
  if (!op) {
    cout << "Couldn't allocate Ogg packet." << endl;
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
    cout << "Couldn't allocate data buffer." << endl;
    return NULL;
  }
  if (!op) {
    cout << "Couldn't allocate Ogg packet." << endl;
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
    cout << "Couldn't allocate Ogg packet." << endl;
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
      cout << "Error writing Ogg page header" << endl;
      return -2;
    }
    written = fwrite(page.body, 1, page.body_len, params->out);
    if (written != (size_t)page.body_len) {
      cout << "Error writing Ogg page body" << endl;
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
	cout << "Error ogg_flush" << endl;
    return -1;
  }

  while (ogg_stream_flush(params->stream, &page)) {
    written = fwrite(page.header, 1, page.header_len, params->out);
    if (written != (size_t)page.header_len) {
      cout << "Error writing Ogg page header" << endl;
      return -2;
    }
    written = fwrite(page.body, 1, page.body_len, params->out);
    if (written != (size_t)page.body_len) {
      cout << "Error writing Ogg page body" << endl;
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
    cout << "Packet too short for rtp" << endl;
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
    cout << "Packet too short for RTP header" << endl;
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
    cout << "RTPRecorder constructor called" << endl;
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

	    cout << "initializing RTPRecorder " << endl;
		params = (state *)malloc(sizeof(state));
		if (!params) {
			cout << "Couldn't allocate param struct." << endl;
			return false;
		}
		params->stream = (ogg_stream_state *)malloc(sizeof(ogg_stream_state));
		if (!params->stream) {
			cout << "Couldn't allocate stream struct." << endl;
			return false;
		}
		if (ogg_stream_init(params->stream, rand()) < 0) {
			cout << "Couldn't initialize Ogg stream state." << endl;
			return false;
		}
		std::string point_file = path+"/"+name+".opus";
		params->out = fopen(path.c_str(), "w+");
		if (!params->out) {
			cout << "Couldn't open output file." << endl;
			return false;
		} else {
			cout << "File opened in " << path.c_str() << endl;
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
      start_f = buffer;
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
	   start_f = NULL;
	   buffer = NULL;
	   received_frame = NULL;
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
	      cout << "error parsing rtp header" << endl;
	      return len;
	    }
//	    cout << " AUDIO rtp 0x%08x %d %d %d",
//	            rtp.ssrc, rtp.type, rtp.seq, rtp.time);
//	    cout << "  v%d %s%s%s CC %d", rtp.version,
//	            rtp.pad ? "P":".", rtp.ext ? "X":".",
//	            rtp.mark ? "M":".", rtp.cc);
//	    cout << " %5d bytes\n", rtp.payload_size);

	    packet += rtp.header_size;
	    size -= rtp.header_size;

	    if (size < 0) {
	      cout << "AUDIO skipping short packet" << endl;
	      return len;
	    }

	    if (rtp.seq < params->seq) {
	      cout << "skipping out-of-sequence packet" << endl;
	      return len;
	    }
	    params->seq = rtp.seq;

	    if (rtp.type != OPUS_PAYLOAD_TYPE) {
	       cout << "skipping non-opus packet" << endl;
	       return len;
	     }


	    if (bundle_){
	    	cout << "Recorder for Bundle Communication" << endl;
	    	if (len <= 10) {
	    		cout << "Packet length < 10" << endl;
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
	    	    cout << "!! truncated" << (rtp.payload_size - size) << "uncaptured bytes" << endl;
	    	  }
	    } else {
	    	cout << "Not Bundle" << endl;
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
     	  cout << "Starting fps evaluation - before="<<before<< endl;
     	  cout << "Waiting for RTP frames..." << endl;
	  }


	  const unsigned char *packet;
	  int size;
	  rtp_header rtp_v;
	  packet = reinterpret_cast<const unsigned char*> (buf);
	  size = len;

	  if (parse_rtp_header(packet, size, &rtp_v)) {
	      cout << "VIDEO error parsing rtp header" << endl;
	      return len;
	    }
//	    cout << " VIDEO rtp 0x%08x %d %d %d",
//	    		rtp_v.ssrc, rtp_v.type, rtp_v.seq, rtp_v.time);
//	    cout << "  v%d %s%s%s CC %d", rtp_v.version,
//	    		rtp_v.pad ? "P":".", rtp_v.ext ? "X":".",
//	    		rtp_v.mark ? "M":".", rtp_v.cc);
//	    cout << " %5d bytes\n", rtp_v.payload_size);

	    packet += rtp_v.header_size;
	    size -= rtp_v.header_size;

	    if (size < 0) {
	      cout << "VIDEO skipping short packet" << endl;
	      return len;
	    }

	  //video_ts = rtp_v.time;
//	  if (video_ts==video_lastTs) { 	//continue encoding
	    if((rtp_v.seq-lastSeq) > 1)
			  cout << "VIDEO unexpected seq - " << rtp_v.seq << ", should have been " << lastSeq << endl;

		  lastSeq = rtp_v.seq;
		  cout << "VIDEO lastSeq set to " << lastSeq << endl << "VIDEO copying packet in buffer...   " << endl;
		  memcpy(start_f, packet, size);
		  cout << "VIDEO packet copied in start_f" << endl;
		  //First VP8 Header Line
		  int skipped = 1;
		  size--;
		  uint8_t vp8pd = *start_f;
		  uint8_t xbit = (vp8pd & 0x80);
		  uint8_t rbit = (vp8pd & 0x40);
		  uint8_t nbit = (vp8pd & 0x20);
		  uint8_t sbit = (vp8pd & 0x10);
		  uint8_t partid = (vp8pd & 0x0F);
		  cout << "VIDEO first VP8 Header Readed" << endl;

		  if(!xbit) {	// Just skip the first byte
			  start_f++;
			  cout << "VIDEO Xbit not marked -> go ahead" << endl;
		  } else {   // XLine
			  cout << "VIDEO Xbit marked -> reading.." << endl;
			  start_f++;
			  size--;
			  skipped++;
			  vp8pd = *start_f;
			  uint8_t ibit = (vp8pd & 0x80);
			  uint8_t lbit = (vp8pd & 0x40);
			  uint8_t tbit = (vp8pd & 0x20);
			  uint8_t kbit = (vp8pd & 0x10);
			  if(ibit) {	// Read the PictureID octet
				  cout << "VIDEO Ibit marked -> reading.." << endl;
				  start_f++;
				  size--;
				  skipped++;
				  vp8pd = *start_f;
				  uint16_t picid = vp8pd, wholepicid = picid;
				  uint8_t mbit = (vp8pd & 0x80);
				  if(mbit) {
					  cout << "VIDEO Mbit marked -> reading.." << endl;
					  memcpy(&picid, start_f, sizeof(uint16_t));
					  wholepicid = ntohs(picid);
					  picid = (wholepicid & 0x7FFF);
					  start_f++;
					  size--;
					  skipped++;
				  }
			  }
			  if(lbit) {	// Read the TL0PICIDX octec
				  cout << "VIDEO Lbit marked -> reading.." << endl;
				  start_f++;
				  size--;
				  skipped++;
				  vp8pd = *start_f;
			  }
			  if(tbit || kbit) { // Read the TID/KEYIDX octec
				  cout << "VIDEO Tbit or Kbit marked -> reading.." << endl;
				  start_f++;
				  size--;
				  skipped++;
				  vp8pd = *start_f;
			  }
			  start_f++;	// Now we're in the payload
			  cout << "VIDEO Now we're in payload" << endl;
			  if(sbit) {
				  cout << "VIDEO Sbit marked -> reading.." << endl;
				  unsigned long int vp8ph = 0;
				  memcpy(&vp8ph, start_f, 4);
				  cout << "VIDEO start_f copied in vp8ph (Vp8 Payload Header?)" << endl;
				  vp8ph = ntohl(vp8ph);
				  uint8_t size0 = ((vp8ph & 0xE0000000) >> 29);
				  uint8_t hbit = ((vp8ph & 0x10000000) >> 28);
				  uint8_t ver = ((vp8ph & 0x0E000000) >> 25);
				  uint8_t pbit = ((vp8ph & 0x01000000) >> 24);
				  uint8_t size1 = ((vp8ph & 0x00FF0000) >> 16);
				  uint8_t size2 = ((vp8ph & 0x0000FF00) >> 8);
				  int fpSize = size0 + 8 * size1 + 2048 * size2;
				  if(!pbit) {
					  cout << "VIDEO Pbit not marked! is a KeyFrame? -> reading.." << endl;
					  vp8gotFirstKey = 1;
					  keyFrame = 1;
					  // Get resolution
					  unsigned char *c = start_f+3;
					  // vet via sync code
					  if(c[0]!=0x9d||c[1]!=0x01||c[2]!=0x2a)
						  cout << "First 3-bytes after header not what they're supposed to be?" << endl;

					  vp8w = swap2(*(unsigned short*)(c+3))&0x3fff;
					  int vp8ws = swap2(*(unsigned short*)(c+3))>>14;
					  vp8h = swap2(*(unsigned short*)(c+5))&0x3fff;
					  int vp8hs = swap2(*(unsigned short*)(c+5))>>14;
					  cout << "VP8 source: " << vp8w << "x" << vp8h << endl;
				  }
			  }
		  }
		  /* Frame manipulation */
		  cout << "VIDEO End of all reading, Starting Frame Manipulation.." << endl;
		  cout << "VIDEO frameLen " << frameLen  << endl;
		  cout << "VIDEO received_frame " <<received_frame  << endl;
		  cout << "VIDEO size " << size  << endl;
		  memcpy(&(received_frame)+frameLen, start_f, size);
		  frameLen += size;
		  if(rtp_v.mark) {	/* Marker bit is set, the frame is complete */
			  start_f = buffer;
			  cout << "VIDEO MarkBit marked (!!!) -> start dumping.." << endl;
			  //video_lastTs = rtp_v.time;
			  if(frameLen > 0) {
				  cout << "VIDEO the frame is not null -> go ahead.." << endl;
				  memset(&(received_frame)+frameLen, 0, FF_INPUT_BUFFER_PADDING_SIZE);
				  cout << "VIDEO memset called correctly -> go ahead.." << endl;
				  frame = avcodec_alloc_frame();
				  cout << "VIDEO Allocated Frame, configuring AVPacket" << endl;

				  AVPacket packet;
				  av_init_packet(&packet);
				  cout << "VIDEO AVPacket initialized... " << endl;
				  packet.stream_index = 0;
				  packet.data = received_frame;
				  packet.size = frameLen;
				  if(keyFrame)
					  packet.flags |= AV_PKT_FLAG_KEY;

				  /* First we save to the file... */
				  cout << " ### Writing frame to file..." << endl;
				  packet.dts = AV_NOPTS_VALUE;
				  packet.pts = AV_NOPTS_VALUE;
				  cout << "VIDEO AVPacket SettedUp" << endl;

				  if(fctx) {
					  if(av_write_frame(fctx, &packet) < 0)
						  cout << "Error writing video frame to file..." << endl;
					  else
						  cout << " ### ### frame written pts=" << packet.pts  << endl;
				  } else {
					  cout << "Still waiting for fps evaluation to create the file..." << endl;
				  }
				  /* Try evaluating the incoming FPS */
				  frames++;
		     	  clock_gettime(CLOCK_MONOTONIC, &tv);
		     	  before = tv.tv_sec*1000 + tv.tv_nsec;
				  if((now-before) >= 1000) {	/* Evaluate every second */
					  cout << "fps=" << frames << " (in " << (now-before) <<" ms" << endl;
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
						  cout << "Creating WebM file: " << fps  << " fps" << endl;
						  create_webm(fps);
					  }
					  frames = 0;
					  before = now;
				  }

				  cout << "VIDEO Resetting all 4 next cycle of reading" << endl;
				  //video_lastTs = rtp_v.time;
				  keyFrame = 0;
				  frameLen = 0;
			  }
			  //video_ts += step;	/* FIXME was 4500, but this implied fps=20 at max */
		  }
//		  if(size == 0)
//			  return size;
//	  }
		  cout << "VIDEO Returning Video Receive Function\n\n" << endl;
		  return 0;
  }

  void RTPRecorder::setBundle(int bund) {
	  cout << "setting recorder bundle to " << bund << endl;
	  bundle_ = bund;
  }

  /* Create WebM context and file */
  int RTPRecorder::create_webm(int fps) {
  	/* WebM output */
  	fctx = avformat_alloc_context();
  	if(fctx == NULL) {
  		cout << "Error allocating context" << endl;
  		return -1;
  	}
  	//~ fctx->oformat = guess_format("webm", NULL, NULL);
  	fctx->oformat = av_guess_format("webm", NULL, NULL);
  	if(fctx->oformat == NULL) {
  		cout << "Error guessing format" << endl;
  		return -1;
  	}
  	snprintf(fctx->filename, sizeof(fctx->filename), "/home/ubuntu/lynckia/recorded/rtpdump-src.webm");
  	//~ vStream = av_new_stream(fctx, 0);
  	vStream = avformat_new_stream(fctx, 0);
  	if(vStream == NULL) {
  		cout << "Error adding stream" << endl;
  		return -1;
  	}
  	//~ avcodec_get_context_defaults2(vStream->codec, CODEC_TYPE_VIDEO);
  	avcodec_get_context_defaults2(vStream->codec, AVMEDIA_TYPE_VIDEO);
  	cout << "VIDEO get context" << endl;
  	vStream->codec->codec_id = CODEC_ID_VP8;
  	cout << "VIDEO defined codec id" << endl;
  //	vStream->codec->codec_id = AV_CODEC_ID_VP8;
  	//~ vStream->codec->codec_type = CODEC_TYPE_VIDEO;
  	vStream->codec->codec_type = AVMEDIA_TYPE_VIDEO;
  	cout << "VIDEO defined codec id" << endl;
  	vStream->codec->time_base = (AVRational){1, fps};
  	vStream->codec->width = 640;
  	vStream->codec->height = 480;
  	cout << "VIDEO defined codec id" << endl;
  	vStream->codec->pix_fmt = PIX_FMT_YUV420P;
  	if (fctx->flags & AVFMT_GLOBALHEADER)
  		vStream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
  	//~ fctx->timestamp = 0;
  	//~ if(url_fopen(&fctx->pb, fctx->filename, URL_WRONLY) < 0) {
  	if(avio_open(&fctx->pb, fctx->filename, AVIO_FLAG_WRITE) < 0) {
  		cout << "Error opening file for output" << endl;
  		return -1;
  	}
  	//~ memset(&parameters, 0, sizeof(AVFormatParameters));
  	//~ av_set_parameters(fctx, &parameters);
  	//~ fctx->preload = (int)(0.5 * AV_TIME_BASE);
  	//~ fctx->max_delay = (int)(0.7 * AV_TIME_BASE);
  	//~ if(av_write_header(fctx) < 0) {
  	if(avformat_write_header(fctx, NULL) < 0) {
  		cout << "Error writing header" << endl;
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
