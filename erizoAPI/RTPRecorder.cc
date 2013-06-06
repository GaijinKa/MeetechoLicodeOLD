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
  tpl->PrototypeTemplate()->Set(String::NewSymbol("init"), FunctionTemplate::New(init)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("start"), FunctionTemplate::New(start)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("stop"), FunctionTemplate::New(stop)->GetFunction());

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

Handle<Value> RTPRecorder::init(const Arguments& args) {
  HandleScope scope;

  RTPRecorder* obj = ObjectWrap::Unwrap<RTPRecorder>(args.This());
  erizo::RTPRecorder *me = obj->me;
  me->init();
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
