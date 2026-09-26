#pragma once
#include "Arduino.h"
#include <vector>
#define HOLDING_REGISTERS 0
struct ModbusStub {
 int nt_count=8, dial_count=8, count=0, pos=0;
 uint16_t nt[8]={700,600,400,800,500,400,0,0};
 uint16_t dial[9]={0,0,0,0,0,60,0,55,0};
 uint16_t* current=nullptr;
 unsigned writes=0, nt_reads=0, dial_reads=0, dial_writes=0;
 uint32_t last_nt_read=0, max_nt_gap=0;
 std::vector<uint32_t> dial_read_times;
 uint32_t fail_delay=1000;
 bool begin_result=true, initialized=false;
 std::vector<uint32_t> init_times;
 bool begin(int,int) {
   assert(!hw_relay[2]);
   init_times.push_back(fake_now);
   fake_now+=100;
   initialized=begin_result;
   return initialized;
 }
 void transaction(bool ok) { assert(initialized); assert(!hw_relay[2]); fake_now+=ok?100:fail_delay; }
 int requestFrom(int id,int,int,int) {
   if(id==2) {
     if(nt_reads) { const uint32_t gap=fake_now-last_nt_read; if(gap>max_nt_gap)max_nt_gap=gap; }
     last_nt_read=fake_now; ++nt_reads;
   } else {++dial_reads; dial_read_times.push_back(fake_now);}
   current=id==2?nt:dial; count=id==2?nt_count:dial_count;
   transaction(count==8); pos=0; return count;
 }
 int available() {return count-pos;}
 long read() {return current[pos++];}
 bool holdingRegisterWrite(int,int addr,uint16_t val) { transaction(dial_count==8); ++writes; ++dial_writes; if(dial_count<=0)return false; dial[addr]=val; return true; }
 bool coilWrite(int,int,int) {transaction(dial_count==8); ++writes; ++dial_writes; return dial_count>0;}
};
inline ModbusStub ModbusRTUClient;
