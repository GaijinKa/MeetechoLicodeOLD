#ifndef RTPRECORDER_H
#define RTPRECORDER_H

#include <node.h>
#include <RTPRecorder.h>
#include "MediaReceiver.h"
#include "OneToManyProcessor.h"


/*
 * Wrapper class of erizo::RTPRecorder
 *
 * A RTP recorder. This class represents a RTPRecorder that can record the mediastream through a OneToManyProcessor
 *
 */
class RTPRecorder : public node::ObjectWrap {
 public:
  static void Init(v8::Handle<v8::Object> target);

  erizo::RTPRecorder *me;

 private:
  RTPRecorder();
   ~RTPRecorder();

   /*
      * Constructor.
      * Constructs a RTPRecorder without any configuration.
      */
     static v8::Handle<v8::Value> New(const v8::Arguments& args);
     /*
      * Close the recorder.
      * The object cannot be used after this call.
      */
     static v8::Handle<v8::Value> close(const v8::Arguments& args);
     /*
      * Inits the Recorder 4 Audio.
      * Returns true if the initialization success.
      */
     static v8::Handle<v8::Value> initAudio(const v8::Arguments& args);
     /*
      * Inits the Recorder 4 Video.
      * Returns true if the initialization success.
      */
     static v8::Handle<v8::Value> initVideo(const v8::Arguments& args);
     /*
      * Starts the Recorder.
      * Return true if recorder starts correctly
      */
     static v8::Handle<v8::Value> start(const v8::Arguments& args);
     /*
      * Stops the Recorder.
      * Return true if recorder stops correctly
      */
     static v8::Handle<v8::Value> stop(const v8::Arguments& args);

     /*
      * Gets the current state of the Recorder
      * Returns the state.
      */
     static v8::Handle<v8::Value> getCurrentState(const v8::Arguments& args);
};

#endif
