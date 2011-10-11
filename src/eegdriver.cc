#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>
#include <v8.h>
#include <node.h>
#include <node_events.h>
#include <node_buffer.h>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

extern "C" {
	//#include <avcodec.h>
	//#include <avformat.h>
}

#include "eegdriver.h"

using namespace v8;
using namespace node;

// static function declarations
//static std::string xml_special_chars(std::string str);

// static vars
static EEGDriver *eegdriver;

// events
//static Persistent<String> symbol_stateChange = NODE_PSYMBOL("stateChanged");


static Handle<Value> VException(const char *msg) {
	HandleScope scope;
	return ThrowException(Exception::Error(String::New(msg)));
};


void EEGDriver::Initialize(v8::Handle<v8::Object> target)
{
	// Grab the scope of the call from Node
	HandleScope scope;
	// Define a new function template
	Local<FunctionTemplate> t = FunctionTemplate::New(New);
	t->Inherit(EventEmitter::constructor_template);
	t->InstanceTemplate()->SetInternalFieldCount(1);
	t->SetClassName(String::NewSymbol("EEGDriver"));
	//constructor_template = Persistent<FunctionTemplate>::New(t);
	//constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
	
	// gobbles a string or a buffer of bytes
	NODE_SET_PROTOTYPE_METHOD(t, "gobble", gobble);
	
	target->Set(String::NewSymbol("EEGDriver"), t->GetFunction());
}

// Create a new instance of BSON and assing it the existing context
Handle<Value> EEGDriver::New(const Arguments &args)
{
	HandleScope scope;
	
	EEGDriver *eegdriver = new EEGDriver();
	eegdriver->Wrap(args.This());
	return args.This();
}

Handle<Value> EEGDriver::gobble(const Arguments &args) {
	int c;
	switch(args.Length()) {
		case 1:
			c = args[0]->ToInt32()->Value();
			break;
			
		default:
			return VException("should be one argument only as an int [0-255] or a string (TODO), or a buffer (TODO)");
			break;
	}
	
	//TODO: call the local function in the class
	//TODO: create the class with the static variables used there
	//TODO: implement the routine to gobble the bytes
	//TODO: when it wants to print, emit an event (I do have to research how to do this again...)
	
	return True();
}

/*
Handle<Value> EEGDriver::archive(const Arguments &args)
{
	HandleScope scope;
	
	Local<Object> result = Object::New();
	result->Set(String::New("get"), args[0]->ToString());
	
	return scope.Close(result);
}

static Handle<Value> entry_to_json(libtorrent::entry e) {
	using namespace boost;
	using namespace libtorrent;
	
	switch(e.type()) {
		case entry::int_t: {
			return Integer::New(e.integer());
		} case entry::string_t: {
			//Local<Value> s;
			//printf("str %d %d\n", e.string().length(), DecodeWrite(e.string().c_str(), e.string().length(), s, node::UTF8));
			
			Local<Value> s = Encode(e.string().c_str(), e.string().length(), node::BINARY);
			//Local<String> s = String::New(e.string().c_str());
			return s;
		} case entry::list_t: {
			entry::list_type& l = e.list();
			Local<Array> arr = Array::New(l.size());
			
			uint32_t idx = 0;
			for(entry::list_type::const_iterator i = l.begin(); i != l.end(); ++i) {
				arr->Set(idx++, entry_to_json(*i));
			}
			
			return arr;
			
		} case entry::dictionary_t: {
			entry::dictionary_type& dict = e.dict();
			Local<Object> obj = Object::New();
			for(entry::dictionary_type::const_iterator i = dict.begin(); i != dict.end(); ++i) {
				obj->Set(String::NewSymbol((*i).first.c_str()), entry_to_json((*i).second));
			}
			
			return obj;
			
		} default: break;
	}
	
	return Undefined();
}
*/

// Exporting function
extern "C" void init(Handle<Object> target)
{
	HandleScope scope;
	EEGDriver::Initialize(target);
}

// NODE_MODULE(l, Long::Initialize);
