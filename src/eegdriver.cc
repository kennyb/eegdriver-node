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

// events
static Persistent<String> symbol_syncError = NODE_PSYMBOL("syncError");
static Persistent<String> symbol_setHeader = NODE_PSYMBOL("setHeader");
static Persistent<String> symbol_data = NODE_PSYMBOL("data");

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

static struct EDFDecodedConfig current;

const char *getDMY(void)
{
	static char buf[81];
	time_t t;
	struct tm *it;
	time(&t);
	it = localtime(&t);
	sprintf(buf, "%02d.%02d.%02d", it->tm_mday, it->tm_mon+1, it->tm_year % 100);
	return buf;
}

const char *getHMS(void)
{
	static char buf[81];
	time_t t;
	struct tm *it;
	time(&t);
	it = localtime(&t);
	sprintf(buf, "%02d.%02d.%02d", it->tm_hour, it->tm_min, it->tm_sec);
	return buf;
}



static Handle<Value> VException(const char *msg) {
	HandleScope scope;
	return ThrowException(Exception::Error(String::New(msg)));
};


void EEGDriver::Initialize(Handle<Object> target)
{
	// Grab the scope of the call from Node
	HandleScope scope;
	// Define a new function template
	Local<FunctionTemplate> t = FunctionTemplate::New(New);
	t->Inherit(EventEmitter::constructor_template);
	t->InstanceTemplate()->SetInternalFieldCount(1);
	t->SetClassName(String::NewSymbol("EEGDriver"));
	
	// gobbles a string or a buffer of bytes
	NODE_SET_PROTOTYPE_METHOD(t, "gobble", gobble);
	NODE_SET_PROTOTYPE_METHOD(t, "header", header);
	
	target->Set(String::NewSymbol("EEGDriver"), t->GetFunction());
}

// Create a new instance of BSON and assing it the existing context
Handle<Value> EEGDriver::New(const Arguments &args)
{
	HandleScope scope;
	EEGDriver *eegdriver = new EEGDriver();
	
	//TODO: get out the device properties
	//TODO: this should also, actually have a start/stop function
	
	eegdriver->Wrap(args.This());
	return args.This();
}

EEGDriver::EEGDriver() : EventEmitter() {
	failCount = 0;
	bufCount = 0;
	goodCount = 0;
}

Handle<Value> EEGDriver::header(const Arguments &args) {
	HandleScope scope;
	char EDFPacket[MAXHEADERLEN];
	int EDFLen = MAXHEADERLEN;
	
	makeREDFConfig(&current, &modEEGCfg);
	writeEDFString(&current, EDFPacket, &EDFLen);
	
	Local<Value> event[1];
	Buffer *header = Buffer::New(EDFPacket, EDFLen);
	Local<Object> globalObj = Context::GetCurrent()->Global();
 
	// Now we need to grab the Buffer constructor function.
	Local<Function> bufferConstructor = Local<Function>::Cast(globalObj->Get(String::New("Buffer")));
	 
	// Great. We can use this constructor function to allocate new Buffers.
	// Let's do that now. First we need to provide the correct arguments.
	// First argument is the JS object Handle for the SlowBuffer.
	// Second arg is the length of the SlowBuffer.
	// Third arg is the offset in the SlowBuffer we want the .. "Fast"Buffer to start at.
	Handle<Value> constructorArgs[3] = { header->handle_, Integer::New(EDFLen), Integer::New(0) };
	
	// Now we have our constructor, and our constructor args. Let's create the
	// damn Buffer already!
	Local<Object> actualBuffer = bufferConstructor->NewInstance(3, constructorArgs);

	//event[0] = actualBuffer;
	//this->Emit(symbol_setHeader, 1, event);
	return scope.Close(actualBuffer);
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
	
	if(c > 255) {
		return VException("unfortunately, I don't accept utf8 chars, please pass a buffer[i]");
	}
	
	EEGDriver *eegdriver = ObjectWrap::Unwrap<EEGDriver>(args.This());
	eegdriver->gobble((unsigned char)c);
	
	return False();
}




int EEGDriver::isValidPacket(unsigned short chan, unsigned short *samples)
{
	int i;
	if (chan != 2 && chan != 4 && chan != 6) return 0;
	for (i = 0; i < chan; i++) {
		if (samples[i] > 1023) return 0;
	}
	return 1;
}

int writeEDFString(const struct EDFDecodedConfig *cfg, char *buf, int *buflen)
{
	int retval;
	char pktbuf[(MAXCHANNELS+1) * 256];
	retval = EDFEncodePacket(pktbuf, cfg);
	if (retval) return retval;
	if (cfg->hdr.headerRecordBytes <= *buflen) {
		*buflen = cfg->hdr.headerRecordBytes;
		memcpy(buf, pktbuf, *buflen);
		return 0;
	}
	else {
		*buflen = 0;
		return 1;
	}
}

void storeEDFString(char *packet, size_t memsize, const char *str)
{
	char fmt[8];
	char buf[257];
	sprintf(fmt, "%%- %ds", memsize);
	sprintf(buf, fmt, str);
	memcpy(packet, buf, memsize);
}

// TODO: Make this more correct for combinations of d and memsize
void storeEDFDouble(char *packet, size_t memsize, double d)
{
	char buf[64];
	sprintf(buf, "%g", d);
	storeEDFString(packet, memsize, buf);
}

void storeEDFInt(char *packet, size_t memsize, int i)
{
	char buf[64];
	sprintf(buf, "%d", i);
	storeEDFString(packet, memsize, buf);
}

int EDFEncodePacket(char *packet, const struct EDFDecodedConfig *cfg)
{
	int whichChannel, totalChannels;
	STOREHFS(dataFormat, cfg->hdr.dataFormat);
	STOREHFS(localPatient, cfg->hdr.localPatient);
	STOREHFS(localRecorder, cfg->hdr.localRecorder);
	STOREHFS(recordingStartDate, cfg->hdr.recordingStartDate);
	STOREHFS(recordingStartTime, cfg->hdr.recordingStartTime);
	STOREHFI(headerRecordBytes, cfg->hdr.headerRecordBytes);
	STOREHFS(manufacturerID, cfg->hdr.manufacturerID);
	STOREHFI(dataRecordCount, cfg->hdr.dataRecordCount);
	STOREHFD(dataRecordSeconds, cfg->hdr.dataRecordSeconds);
	STOREHFI(dataRecordChannels, cfg->hdr.dataRecordChannels);
	totalChannels = cfg->hdr.dataRecordChannels;
	packet += 256;
	for (whichChannel = 0; whichChannel < totalChannels; whichChannel++) {
		STORECFS(label, cfg->chan[whichChannel].label);
		STORECFS(transducer, cfg->chan[whichChannel].transducer);
		STORECFS(dimUnit, cfg->chan[whichChannel].dimUnit);
		STORECFD(physMin, cfg->chan[whichChannel].physMin);
		STORECFD(physMax, cfg->chan[whichChannel].physMax);
		STORECFD(digiMin, cfg->chan[whichChannel].digiMin);
		STORECFD(digiMax, cfg->chan[whichChannel].digiMax);
		STORECFS(prefiltering, cfg->chan[whichChannel].prefiltering);
		STORECFI(sampleCount, cfg->chan[whichChannel].sampleCount);
		STORECFS(reserved, cfg->chan[whichChannel].reserved);
	}
	return 0;
}


int setEDFHeaderBytes(struct EDFDecodedConfig *cfg)
{
	cfg->hdr.headerRecordBytes = 256 * (cfg->hdr.dataRecordChannels + 1);
	return 0;
}

int isValidREDF(const struct EDFDecodedConfig *cfg)
{
	int i;
	if (cfg->hdr.dataRecordSeconds != 1.0) {
		//TODO: setLastError("The data record must be exactly 1 second, not %f.", 
		//						 cfg->hdr.dataRecordSeconds);
		return 0;
	}
	if (cfg->hdr.dataRecordChannels < 1) {
		//TODO: setLastError("The data record must have at least one channel.");
		return 0;
	}
	if (cfg->chan[0].sampleCount < 1) {
		//TODO: setLastError("Channel 0 must have at least one sample.");
		return 0;
	}
	for (i = 1; i < cfg->hdr.dataRecordChannels; ++i) {
		if (cfg->chan[i].sampleCount != cfg->chan[0].sampleCount) {
			//TODO: setLastError("Channel %d has %d samples, but channel 0 has %d.  These must be the same.", cfg->chan[i].sampleCount, cfg->chan[0].sampleCount);
			return 0;
		}
	}
	return 1;
}


int makeREDFConfig(struct EDFDecodedConfig *result, const struct EDFDecodedConfig *source)
{
	int newSamples, i;
	*result = *source;
	if (source->hdr.dataRecordSeconds != 1.0) {
		result->hdr.dataRecordSeconds = 1;
		newSamples = ((double)source->chan[0].sampleCount) / 
			             source->hdr.dataRecordSeconds;
		for (i = 0; i < source->hdr.dataRecordChannels; ++i)
			result->chan[i].sampleCount = newSamples;
	}
	setEDFHeaderBytes(result);
	strcpy(result->hdr.recordingStartDate, getDMY());
	strcpy(result->hdr.recordingStartTime, getHMS());
	assert(isValidREDF(result));
	return 0;
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
	
		//printf("Got %s packet with %d values: ", pstr, ns);
		//for (int i = 0; i < ns; ++i) {
		//	printf("%d ", vals[i]);
		//}
		//printf("\n");
	}
	else {
		failCount += 1;
		if (failCount % PROTOWINDOW == 0) {
			this->Emit(symbol_syncError, 0, NULL);
			//printf("Serial packet sync error -- missed window.\n");
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
	
	Local<Object> result = Object::New();
	result->Set(String::New("packet"), Integer::New(packetCounter));
	Local<Object> channels = Object::New();
	for (i = 0; i < chan; ++i)
		channels->Set(Integer::New(i), Integer::New(vals[i]));
	result->Set(String::New("channels"), channels);
	Local<Value> event[1];
	event[0] = result;
	this->Emit(symbol_data, 1, event);
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


extern "C" {
// Exporting function
void init(Handle<Object> target)
{
	HandleScope scope;
	EEGDriver::Initialize(target);
}
} //extern "C"
// NODE_MODULE(l, Long::Initialize);
