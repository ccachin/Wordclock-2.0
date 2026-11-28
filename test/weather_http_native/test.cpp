#include "weather_http_body.h"
#include <cassert>
#include <cstring>
#include <string>
#include <iostream>
using WeatherHttp::Result;
static Result run(const std::string& s,long length=-1,int end=-1,size_t cap=1536){char buf[1536];size_t pos=0,n=0;return WeatherHttp::read([&](){return pos<s.size()?static_cast<unsigned char>(s[pos++]):end;},buf,cap,length,n);}
int main(){
 const std::string valid=R"({"current":{"time":1795850000,"temperature_2m":2.2,"weather_code":3,"is_day":1},"hourly":{"time":[1,2,3,4]},"text":"escaped \" and } [ \\"})";
 assert(run(valid)==Result::Ok);assert(run(valid,valid.size())==Result::Ok);assert(run(valid+" \r\n")==Result::Ok);
 for(size_t i=1;i<valid.size();++i){assert(run(valid.substr(0,i))==Result::Incomplete);assert(run(valid.substr(0,i),valid.size())==Result::Incomplete);assert(run(valid.substr(0,i),-1,-2)==Result::Timeout);}
 assert(run("")==Result::Empty);assert(run(" ")==Result::Empty);assert(run("[]")==Result::Invalid);assert(run("{] ")==Result::Invalid);assert(run(valid+"{} ")==Result::Invalid);assert(run(valid+std::string(1,'\0'))==Result::Invalid);assert(run(valid,-1,-1,10)==Result::Limit);assert(run(valid,1536)==Result::Limit);assert(run(valid,-1,-2)==Result::Timeout);
 assert(run(valid,valid.size()+1)==Result::Incomplete);assert(run(valid,-2)==Result::Limit);
 std::cout<<"PASS weather HTTP framing: "<<3*(valid.size()-1)+14<<" cases\n";
}
