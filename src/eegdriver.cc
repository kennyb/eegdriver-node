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
	void gobble(unsigned char c);
}

#include "eegdriver.h"

using namespace v8;
using namespace node;

// static function declarations
//static std::string xml_special_chars(std::string str);

// static vars
//static EEGDriver *eegdriver;

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
	EEGDriver *eegdriver = ObjectWrap::Unwrap<EEGDriver>(args.This());
	
	int c;
	switch(args.Length()) {
		case 1:
			c = args[0]->ToInt32()->Value();
			break;
			
		default:
			return VException("should be one argument only as an int [0-255] or a string (TODO), or a buffer (TODO)");
			break;
	}
	
	if(c > 255) {
		return VException("unfortunately, I don't accept utf8 chars, please pass a buffer[i]");
	}
	
	//TODO: call the local function in the class
	//TODO: create the class with the static variables used there
	//TODO: implement the routine to gobble the bytes
	//TODO: when it wants to print, emit an event (I do have to research how to do this again...)
	eegdriver->gobble((unsigned char)c);
	
	return True();
}




extern "C" {
/* This is the maximum size of a protocol packet */

static struct EDFDecodedConfig modEEGCfg = {
		{ 0,   // header bytes, to be set later
			-1,  // data record count
			2,   // channels
			"0", // data format
			"Mr. Default Patient",
			"Ms. Default Recorder",
			"date",
			"time",
			"manufacturer",
			1.0,  // 1 second per data record
		}, {
				{
					256, // samples per second
					-512, 512, // physical range
					0, 1023,   // digital range
					"electrode 0", // label
					"metal electrode",  // transducer
					"uV",  // physical unit dimensions
					"LP:59Hz", // prefiltering
					""         // reserved
				},
				{
					256, -512, 512, 0, 1023, 
					"electrode 1", "metal electrode", "uV", "LP:59Hz", ""
				}
		}
	};

//static struct EDFDecodedConfig current;

int isValidPacket(unsigned short chan, unsigned short *samples)
{
	int i;
	if (chan != 2 && chan != 4 && chan != 6) return 0;
	for (i = 0; i < chan; i++) {
		if (samples[i] > 1023) return 0;
	}
	return 1;
}

void EEGDriver::resetBuffer(void)
{
	this->bufCount = 0;
}

void EEGDriver::gobbleChars(int howMany)
{
	assert(howMany <= bufCount);
	if (bufCount > howMany)
		memmove(buf, buf+howMany, bufCount-howMany);
	bufCount -= howMany;
}

}

void EEGDriver::gobble(unsigned char c)
{
#define MINGOOD 6
	int oldHasMatched;
	unsigned short vals[MAXCHANNELS];
	int ns;
	int didMatch = 0;
	oldHasMatched = hasMatchedProtocol;
	if (bufCount == PROTOWINDOW - 1)
		memmove(buf, buf+1, PROTOWINDOW - 1);
	this->buf[this->bufCount] = c;
	if (bufCount < PROTOWINDOW - 1)
		bufCount += 1;
	if (hasMatchedProtocol) {
		if (isP2)
			didMatch = doesMatchP2(c, vals, &ns);
		if (isP3)
			didMatch = doesMatchP3(c, vals, &ns);
	} else {
		if ( (didMatch = doesMatchP2(c, vals, &ns)) ) {
			if (goodCount > MINGOOD) {
				hasMatchedProtocol = 1;
				isP2 = 1;
			}
		} else {
			if ( (didMatch = doesMatchP3(c, vals, &ns)) ) {
				if (goodCount > MINGOOD) {
					hasMatchedProtocol = 1;
					isP3 = 1;
				}
			}
		}
	}
	if (didMatch) {
		char *pstr = "xx";
		if (isValidPacket(ns, vals)) {
			goodCount += 1;
		}
		else {
			printf("Warning: invalid serial packet ahead!\n");
		}
		if (isP2)
			pstr = "P2";
		if (isP3)
			pstr = "P3";
			
		failCount = 0;
	
//		printf("Got %s packet with %d values: ", pstr, ns);
//		for (i = 0; i < ns; ++i)
//			printf("%d ", vals[i]);
//		printf("\n");
	}
	else {
		failCount += 1;
		if (failCount % PROTOWINDOW == 0) {
			printf("Serial packet sync error -- missed window.\n");
			goodCount = 0;
		}
	}
}

void EEGDriver::handleSamples(int packetCounter, int chan, unsigned short *vals)
{
	char buf[MAXLEN];
	int bufPos = 0;
	int i;
//	printf("Got good packet with counter %d, %d chan\n", packetCounter, chan);
	//TODO:if (chan > current.hdr.dataRecordChannels)
	//TODO:	chan = current.hdr.dataRecordChannels;
	bufPos += sprintf(buf+bufPos, "! %d %d", packetCounter, chan);
	for (i = 0; i < chan; ++i)
		bufPos += sprintf(buf+bufPos, " %d", vals[i]);
	bufPos += sprintf(buf+bufPos, "\r\n");
	//TODO: writeString(sock_fd, buf, &ob);
	//TODO: mGetOK(sock_fd, &ib);
}

int EEGDriver::doesMatchP3(unsigned char c,  unsigned short *vals,int *nsamples)
{
	static int i = 0;
	static int j = 0;
	static int packetCounter = 0;
	static int auxChan = 0;
	static short samples[MAXCHANNELS];
	static int needsHeader = 1;
	static int needsAux;
	if (needsHeader) {
		if (c & 0x80) goto resetMachine;
		packetCounter = c >> 1;
		auxChan = c & 1;
		needsHeader = 0;
		needsAux = 1;
		return 0;
	}
	if (needsAux) {
		if (c & 0x80) goto resetMachine;
		auxChan += (int) c * 2;
		needsAux = 0;
		return 0;
	}
	if (2*i < MAXCHANNELS) {
       switch (j) {
           case 0: samples[2*i] = (int) c; break;
           case 1: samples[2*i+1] = (int) c; break;
           case 2:
               samples[2*i] += (int)   (c & 0x70) << 3;
               samples[2*i+1] += (int) (c & 0x07) << 7;
               i += 1;
               break;
       }
	}
	j = (j+1) % 3;
	if (c & 0x80) {
		if (j == 0) {
			int k;
			memcpy(vals, samples, sizeof(vals[0]) * i * 2);
			*nsamples = i * 2;
			for (k = 0; k < *nsamples; ++k)
				if (vals[k] > 1023)
					goto resetMachine;
			i = 0;
			j = 0;
			needsHeader = 1;
			this->handleSamples(packetCounter, *nsamples, vals);
			return 1;
		}
		else {
			if (isP3)
				printf("P3 sync error:i=%d,j=%d,c=%d.\n",i,j,c);
		}
		goto resetMachine;
	}
	return 0;

	resetMachine:
		i = 0;
		j = 0;
		needsHeader = 1;
	return 0;
}

int EEGDriver::doesMatchP2(unsigned char c, unsigned short *vals,int *nsamples)
{
#define P2CHAN 6
	enum P2State { waitingA5, waiting5A, waitingVersion, waitingCounter,
		waitingData, waitingSwitches };
	static int packetCounter = 0;
	static int chan = 0;
	static int i = 0;
	static enum P2State state = waitingA5;
	static unsigned short s[MAXCHANNELS];
	switch (state) {
		case waitingA5:
			if (c == 0xa5) {
				failCount = 0;
				state = waiting5A;
			}
			else {
				failCount += 1;
				if (failCount % PROTOWINDOW == 0) {
					printf("Sync error, packet lost.\n");
				}
			}
			break;

		case waiting5A:
			if (c == 0x5a)
				state = waitingVersion;
			else
				state = waitingA5;
			break;

		case waitingVersion:
			if (c == 0x02) /* only version 2 supported right now */
				state = waitingCounter;
			else
				state = waitingA5;
			break;

		case waitingCounter:
			packetCounter = c;
			state = waitingData;
			i = 0;
			break;

		case waitingData:
			if ((i%2) == 0)
				s[i/2] = c;
			else
				s[i/2] = s[i/2]* 256 + c;
			i += 1;
			if (i == P2CHAN * 2)
				state = waitingSwitches;
			break;

		case waitingSwitches:
			chan = P2CHAN;
			chan = 2;  /* todo: fix me */
			*nsamples = chan;
			memcpy(vals, s, chan * sizeof(s[0]));
			handleSamples(packetCounter, *nsamples, vals);
			state = waitingA5;
			return 1;

		default:
				printf("Error: unknown state in P2!\n");
				//TODO: rexit(0);
		}
	return 0;
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

extern "C" {
// Exporting function
void init(Handle<Object> target)
{
	HandleScope scope;
	EEGDriver::Initialize(target);
}
} //extern "C"
// NODE_MODULE(l, Long::Initialize);
