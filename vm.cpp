//implementation of the test Vm
#include <iostream>
#include <fstream>
#include <inttypes.h>

using namespace std::literals::string_literals;

const int MEM_SIZE = 256 * 1024 * 1024; //256MiB

int32_t mem[MEM_SIZE];
int32_t reg[16];
int32_t cmp;

void exitError(std::string const& msg) {
	std::cout << "Error: " << msg << "\nExiting now.\n";
	exit(-1);
}

bool startsWith(std::string const& s, std::string const& start) {
	if(s.size()<start.size()) return false;
	for(int i=0;i<start.size();++i) {
		if(s[i]!=start[i]) return false;
	}
	return true;
}
int32_t hexToInt(std::string const& s) {
	int32_t n = 0;
	for(int i=0;i<s.size();++i) {
		if(s[i]>='0'&&s[i]<='9') n = n<<4 | s[i]-'0';
		if(s[i]>='a'&&s[i]<='f') n = n<<4 | s[i]-'a'+10;
		if(s[i]>='A'&&s[i]<='F') n = n<<4 | s[i]-'A'+10;
	}
	return n;
}

std::string intToHex(int32_t n) {
	std::string s("");
	for(int i=7;i>=0;--i) {
		if(((n>>(i*4))&0xF) > 9)
			s += ((n>>(i*4))&0xF)+'A'-10;
		else
			s += ((n>>(i*4))&0xF)+'0';	
	}
	return s;
}
char byteToChar(char c) {
	if(c>=' '&&c<='~') return c;
	return '.';
}

const int MEM_VIEWER_HEIGHT = 32;
const int MEM_VIEWER_WIDTH = 4;

void memoryViewer() {
	std::string pos;
	std::cin >> pos;
	auto base = hexToInt(pos);
	for(int i=0;i<MEM_VIEWER_HEIGHT;++i) {
		std::cout << intToHex(base+i*MEM_VIEWER_WIDTH*4) << " | ";
		for(int j=0;j<MEM_VIEWER_WIDTH;++j) {
			std::cout << intToHex(mem[base/4+i*MEM_VIEWER_WIDTH+j]) << " ";
		}
		std::cout << " | ";
		for(int j=0;j<MEM_VIEWER_WIDTH;++j) {
			std::cout << byteToChar(mem[base+i*MEM_VIEWER_WIDTH+j]&0xFF);
			std::cout << byteToChar(mem[base+i*MEM_VIEWER_WIDTH+j]>>8&0xFF);
			std::cout << byteToChar(mem[base+i*MEM_VIEWER_WIDTH+j]>>16&0xFF);
			std::cout << byteToChar(mem[base+i*MEM_VIEWER_WIDTH+j]>>24&0xFF);
			std::cout << " ";
		}
		std::cout << "\n";
	}
	std::cout << std::endl;
}

void registerViewer() {
	for(int i=0;i<16;++i) {
		std::cout << "r"<<i<<":\t"<<intToHex(reg[i])<<"  ("<<reg[i]<<")"<<std::endl;
	}
	std::cout << "cmp:\t"<<intToHex(cmp)<< " "
		<< ((cmp&8)?'<':'.')
		<< ((cmp&4)?'>':'.')
		<< ((cmp&2)?'!':'.')
		<< ((cmp&1)?'=':'.') << std::endl;
}

int32_t readMem(int32_t addr) {
	if(addr <= 0x0FFFFFFF) return mem[addr/4];
	if(addr == 0x10000000) { char c; std::cin.get(c); return c; }
	if(addr == 0x10000004) { int32_t i; std::cin >> i; return i; }
}
void writeMem(int32_t addr, int32_t val) {
	if(addr <= 0x0FFFFFFF) mem[addr/4] = val;
	if(addr == 0x10000000) std::cout << ((char)val) << std::flush;
	if(addr == 0x10000004) std::cout << val << std::flush;
}

void doCycle() {
	int32_t inst = mem[reg[0]/4];
	if((inst&0xF0000000) == 0x10000000) //SET
		reg[(inst>>24)&0xF] = inst & 0x00FFFFFF;

	else if(inst>>16 == 0x0100) //ADD
		reg[(inst>>8)&0xF] = reg[(inst>>4)&0xF] + reg[inst&0xF];
	else if(inst>>16 == 0x0101) //SUB
		reg[(inst>>8)&0xF] = reg[(inst>>4)&0xF] - reg[inst&0xF];
	else if(inst>>16 == 0x0102) //MUL
		reg[(inst>>8)&0xF] = reg[(inst>>4)&0xF] * reg[inst&0xF];
	else if(inst>>16 == 0x0103) //DIV
		reg[(inst>>8)&0xF] = reg[(inst>>4)&0xF] / reg[inst&0xF];
	else if(inst>>16 == 0x0104) //MOD
		reg[(inst>>8)&0xF] = reg[(inst>>4)&0xF] % reg[inst&0xF];

	else if(inst>>16 == 0x0110) //AND
		reg[(inst>>8)&0xF] = reg[(inst>>4)&0xF] & reg[inst&0xF];
	else if(inst>>16 == 0x0111) //OR
		reg[(inst>>8)&0xF] = reg[(inst>>4)&0xF] | reg[inst&0xF];
	else if(inst>>16 == 0x0112) //NOT
		reg[(inst>>8)&0xF] = ~reg[(inst>>4)&0xF];
	else if(inst>>16 == 0x0113) //XOR
		reg[(inst>>8)&0xF] = reg[(inst>>4)&0xF] ^ reg[inst&0xF];
	else if(inst>>16 == 0x0120) { //CMP
		auto a = reg[(inst>>4)&0xF]; auto b = reg[inst&0xF];
		cmp = 0;
		if(a<b)  cmp |= 8;
		if(a>b)  cmp |= 4;
		if(a!=b) cmp |= 2;
		if(a==b) cmp |= 1;
	}

	else if(inst>>16 == 0x0000) //LDR
		reg[(inst>>4)&0xF] = readMem(reg[inst&0xF]);
	else if(inst>>16 == 0x0001) //STR
		writeMem(reg[inst&0xF], reg[(inst>>4)&0xF]);
	else if(inst>>16 == 0x0010) //PUSH
		{ reg[1]+=4; mem[reg[1]/4] = reg[inst&0xF]; }
	else if(inst>>16 == 0x0011) //POP
		{ reg[inst&0xF] = mem[reg[1]/4]; reg[1] -= 4; }

	else if(inst>>16 == 0x0200) //IFX
		{ if(!(inst & cmp)) reg[0] += 4; }

	else std::cout << "Unknown Instruction:" << std::hex << inst << " at pc " << reg[0] << std::endl;

	reg[0] += 4;
}

int main(int argc, char** argv) {
	if(argc < 2) {
		exitError("too few arguments.\nSyntax: "s+argv[0]+" [program]");
	}

	for(int i=0;i<16;++i) reg[i] = 0;
	reg[1] = 0x0FFFFFFF;

	std::ifstream file(argv[1], std::ios::ate| std::ios::binary);
	if(!file.is_open()) exitError("Could not open file "s+argv[1]);
	auto size = file.tellg();
	if((size/32)>MEM_SIZE) exitError("Program does not fit into RAM.");
	file.seekg(0, std::ios::beg);
	file.read((char*)mem, size);
	file.close();
	std::cout << "Program loaded." << std::endl;

	for(;;) {
		std::cout << "> " << std::flush;
		std::string cmd;
		std::cin >> cmd;
		     if(cmd == "mem"||cmd=="m") memoryViewer();
		else if(cmd == "reg"||cmd=="r") registerViewer();
		else if(cmd == "exit") exit(0);
		else if(cmd == "run") for(;;) doCycle();
		else if(cmd == "next"||cmd=="n") doCycle();
		else std::cout << "Command " << cmd << " unknown." << std::endl;
	}
}
