#ifndef EEGDRIVER_H_
#define EEGDRIVER_H_

#include <node.h>
#include <node_object_wrap.h>
#include <v8.h>

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "openedf.h"
#include "edfmacros.h"

using namespace v8;
using namespace node;

#define MAXLEN 16384

class EEGDriver : public EventEmitter
{
  public:    
    EEGDriver() : EventEmitter() {
    	failCount = 0;
    	bufCount = 0;
    	goodCount = 0;
    }
    ~EEGDriver() {}
    
    static void Initialize(Handle<Object> target);
		static Handle<Value> gobble(const Arguments &args);
	
  protected:
    #define PROTOWINDOW 24
		#define MAXPACKETSIZE 17

		char buf[PROTOWINDOW];
		int bufCount;
		int failCount;
		int goodCount;
		//struct OutputBuffer ob;
		//struct InputBuffer ib;

		int hasMatchedProtocol;
		int isP2, isP3;
		void gobble(unsigned char c);
		void resetBuffer();
		void gobbleChars(int howMany);
		void handleSamples(int packetCounter, int chan, unsigned short *vals);
		int isValidPacket(unsigned short nchan, unsigned short *samples);
		int doesMatchP3(unsigned char c, unsigned short *vals,int *nsamples);
		int doesMatchP2(unsigned char c, unsigned short *vals,int *nsamples);
		
	private:
    static Handle<Value> New(const Arguments &args);
};




// trim functions ripped from
// http://www.zedwood.com/article/107/cpp-trim-function
std::string& trim(std::string &str)
{
    int i,j,start,end;

    //ltrim
    for (i=0; (str[i]!=0 && str[i]<=32); )
        i++;
    start=i;

    //rtrim
    for(i=0,j=0; str[i]!=0; i++)
        j = ((str[i]<=32)? j+1 : 0);
    end=i-j;
    str = str.substr(start,end-start);
    return str;
}
std::string& ltrim(std::string &str)
{
    int i,start;

    for (i=0; (str[i]!=0 && str[i]<=32); )
        i++;
    start=i;

    str = str.substr(start,str.length()-start);
    return str;
}
std::string& rtrim(std::string &str)
{
    int i,j,end;

    for(i=0,j=0; str[i]!=0; i++)
        j = ((str[i]<=32)? j+1 : 0);
    end=i-j;

    str = str.substr(0,end);
    return str;
}


#endif  // EEGDRIVER_H_
