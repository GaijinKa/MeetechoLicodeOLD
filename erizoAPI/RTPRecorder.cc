#define BUILDING_NODE_EXTENSION
#include <node.h>
#include "RTPRecorder.h"

using namespace v8;

RTPRecorder::RTPRecorder() {};
RTPRecorder::~RTPRecorder() {};

void RTPRecorder::Init(Handle<Object> target) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
  tpl->SetClassName(String::NewSymbol("RTPRecorder"));
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  // Prototype
  tpl->PrototypeTemplate()->Set(String::NewSymbol("close"), FunctionTemplate::New(close)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("initVideo"), FunctionTemplate::New(initVideo)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("initAudio"), FunctionTemplate::New(initAudio)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("start"), FunctionTemplate::New(start)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("stop"), FunctionTemplate::New(stop)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("getCurrentState"), FunctionTemplate::New(getCurrentState)->GetFunction());

  Persistent<Function> constructor = Persistent<Function>::New(tpl->GetFunction());
  target->Set(String::NewSymbol("RTPRecorder"), constructor);
}

Handle<Value> RTPRecorder::New(const Arguments& args) {
  HandleScope scope;
  if (args.Length()>0){
    ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
    return args.This();
  }

  RTPRecorder* obj = new RTPRecorder();
  obj->me = new erizo::RTPRecorder();
  obj->Wrap(args.This());

  return args.This();
}

Handle<Value> RTPRecorder::close(const Arguments& args) {
  HandleScope scope;

  RTPRecorder* obj = ObjectWrap::Unwrap<RTPRecorder>(args.This());
  erizo::RTPRecorder *me = obj->me;

  delete me;

  return scope.Close(Null());
}

Handle<Value> RTPRecorder::initVideo(const Arguments& args) {
  HandleScope scope;

  RTPRecorder* obj = ObjectWrap::Unwrap<RTPRecorder>(args.This());
  erizo::RTPRecorder *me = obj->me;

  v8::String::Utf8Value param(args[0]->ToString());
  v8::String::Utf8Value param2(args[1]->ToString());

// convert it to string
  std::string path = std::string(*param);
  std::string name = std::string(*param2);

  me->initVideo(path, name);

  return scope.Close(Null());
}

Handle<Value> RTPRecorder::initAudio(const Arguments& args) {
  HandleScope scope;

  RTPRecorder* obj = ObjectWrap::Unwrap<RTPRecorder>(args.This());
  erizo::RTPRecorder *me = obj->me;

  v8::String::Utf8Value param(args[0]->ToString());
  v8::String::Utf8Value param2(args[1]->ToString());

// convert it to string
  std::string path = std::string(*param);
  std::string name = std::string(*param2);

  me->initAudio(path, name);

  return scope.Close(Null());
}

Handle<Value> RTPRecorder::start(const Arguments& args) {
  HandleScope scope;

  RTPRecorder* obj = ObjectWrap::Unwrap<RTPRecorder>(args.This());
  erizo::RTPRecorder *me = obj->me;
  me->start();

  return scope.Close(Null());
}

Handle<Value> RTPRecorder::stop(const Arguments& args) {
  HandleScope scope;

  RTPRecorder* obj = ObjectWrap::Unwrap<RTPRecorder>(args.This());
  erizo::RTPRecorder *me = obj->me;
  me->stop();

  return scope.Close(Null());
}
Handle<Value> RTPRecorder::getCurrentState(const Arguments& args) {
  HandleScope scope;

  WebRtcConnection* obj = ObjectWrap::Unwrap<WebRtcConnection>(args.This());
  erizo::WebRtcConnection *me = obj->me;

  int state = me->getCurrentState();

  return scope.Close(Number::New(state));
}
