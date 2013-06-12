/*
 * OneToManyProcessor.cpp
 */

#include "OneToManyProcessor.h"
#include "WebRtcConnection.h"
#include "RTPRecorder.h"

namespace erizo {
  OneToManyProcessor::OneToManyProcessor() :
    MediaReceiver() {

      sendVideoBuffer_ = (char*) malloc(2000);
      sendAudioBuffer_ = (char*) malloc(2000);
      publisher = NULL;
      recorder = NULL;
      sentPackets_ = 0;

    }

  OneToManyProcessor::~OneToManyProcessor() {
    this->closeAll();
    if (sendVideoBuffer_)
      delete sendVideoBuffer_;
    if (sendAudioBuffer_)
      delete sendAudioBuffer_;
  }

  int OneToManyProcessor::receiveAudioData(char* buf, int len) {

	if (recorder!=NULL) {
		recorder->receiveAudioData(buf, len);
	}

    if (subscribers.empty() || len <= 0)
      return 0;

    std::map<std::string, WebRtcConnection*>::iterator it;
    for (it = subscribers.begin(); it != subscribers.end(); it++) {
      memset(sendAudioBuffer_, 0, len);
      memcpy(sendAudioBuffer_, buf, len);
      (*it).second->receiveAudioData(sendAudioBuffer_, len);
    }

    return 0;
  }

  int OneToManyProcessor::receiveVideoData(char* buf, int len) {

	if (recorder!=NULL) {
		recorder->receiveVideoData(buf, len);
	}
    if (subscribers.empty() || len <= 0)
      return 0;
    rtcpheader* head = reinterpret_cast<rtcpheader*>(buf);
    if(head->packettype==201 || head->packettype==206){
      int offset = 0;
      /*
      rtcpheader* head2;

      while (offset<len){
        head2 = reinterpret_cast<rtcpheader*>(&buf[offset]);
        if (head2->packettype==206 && head2->blockcount==1){
          //printf("NACK\n");
        }
        offset+=(ntohs(head2->length)+1)*4;
      }
      */

      head->ssrc = htonl(publisher->localVideoSsrc_);
      publisher->receiveVideoData(buf,len);
      return 0;
    }

    std::map<std::string, WebRtcConnection*>::iterator it;
    for (it = subscribers.begin(); it != subscribers.end(); it++) {
      memset(sendVideoBuffer_, 0, len);
      memcpy(sendVideoBuffer_, buf, len);
      (*it).second->receiveVideoData(sendVideoBuffer_, len);
    }

    sentPackets_++;
    return 0;
  }

  void OneToManyProcessor::setPublisher(WebRtcConnection* webRtcConn) {

    this->publisher = webRtcConn;
  }

  void OneToManyProcessor::addSubscriber(WebRtcConnection* webRtcConn,
      const std::string& peerId) {
    printf("Adding subscriber\n");
    webRtcConn->localAudioSsrc_ = this->publisher->remoteAudioSSRC_;
    webRtcConn->localVideoSsrc_ = this->publisher->remoteVideoSSRC_;
//    if (this->subscribers.empty()|| this->rtcpReceiverPeerId_.empty()){
      printf("Adding rtcp\n");
  //    this->rtcpReceiverPeerId_= peerId;
      webRtcConn->setVideoReceiver(this);
    //}
    this->subscribers[peerId] = webRtcConn;
  }

  void OneToManyProcessor::addRecorder(RTPRecorder* rtprec) {
    printf("Adding recorder\n");
    rtprec->setBundle(this->publisher->isBundle());
    this->recorder = rtprec;
  }


  void OneToManyProcessor::removeSubscriber(const std::string& peerId) {
//    if (!rtcpReceiverPeerId_.compare(peerId)){
//      rtcpReceiverPeerId_.clear();
//    }
    if (this->subscribers.find(peerId) != subscribers.end()) {
      this->subscribers[peerId]->close();
      this->subscribers.erase(peerId);
    }
  }

  void OneToManyProcessor::removeRecorder() {
    if (this->recorder) {
      this->recorder->close();
      this->recorder = NULL;
    }
 }

  void OneToManyProcessor::closeAll() {
    std::map<std::string, WebRtcConnection*>::iterator it;
    for (it = subscribers.begin(); it != subscribers.end(); it++) {
      (*it).second->close();
    }
    this->publisher->close();
  }

}/* namespace erizo */
