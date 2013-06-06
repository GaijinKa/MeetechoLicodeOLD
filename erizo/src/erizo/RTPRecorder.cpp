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

  bool RTPRecorder::init() {
	  return true;
  }

  void RTPRecorder::close() {
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
