#ifndef PLAYBOX_H_
#define PLAYBOX_H_

#include <node.h>
#include <node_object_wrap.h>
#include <v8.h>

extern "C" {
	#include <stdio.h>
	#include <assert.h>
	#include <stdlib.h>
	#include <string.h>
	#include <nsnet.h>
	#include <nsutil.h>
	#include <nsser.h>
	#include <openedf.h>
	#include <config.h>
}

using namespace v8;
using namespace node;

class EEGDriver : public EventEmitter
{
  public:    
    EEGDriver() : EventEmitter() {}
    ~EEGDriver() {}
    
    static void Initialize(Handle<Object> target);
		static Handle<Value> gobble(const Arguments &args);
	
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


#endif  // PLAYBOX_H_
