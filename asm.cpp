#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <inttypes.h>

using namespace std::literals::string_literals;

/* Grammar:
<Program>    ::= <Line> \n <Program> | <Line>
<Line>       ::= <Mark> <OpCode> <Args>
<Mark>       ::= <Identifier>: | .<Identifier>: | eps
<Identifier> ::= [a-zA-Z_][a-zA-Z_0-9]*
<OpCode>     ::= 2-4 Letter Akronym
<Args>       ::= <Register> | <Register>, <Register> | <Register>, <Register>, <Register>
<Register>   ::= r0 - r15

Token types:
idf
num
:
.
,
endl
*/


struct Token {
	std::string type;
	std::string val;
	Token(std::string const& t, std::string const& v) {
		type = t;
		val = v;
	}
	Token() { type=""; val=""; }
};

std::vector<Token> tokens;
std::map<std::string, int> labels;

void exitError(std::string const& msg) {
	std::cout << "Error: " << msg << "\nExiting now." << std::endl;
	exit(-1);
}

int main(int argc, char **argv) {
	if(argc < 3) {
		exitError("too few arguments.\nSyntax: "s+argv[0]+" [binary] [assembly]");
	}

	std::ifstream file(argv[2]);
	if(!file.is_open()) exitError("Could not open file "s+argv[2]);

	//LEXER
	std::string line;
	Token nextToken;
	for(int linenum=0; std::getline(file, line); ++linenum) {
		int tokennum = tokens.size();
		for(int i=0;i<line.size();++i) {
			if(line[i]==' ' || line[i]=='\t') continue;
			else if(line[i]=='#') { //number
				nextToken.type = "num";
				nextToken.val = "";
				while(line[i+1]>='0'&&line[i+1]<='9') nextToken.val += line[++i];
				tokens.push_back(nextToken);
			}
			else if((line[i]>='a')&&(line[i]<='z')||(line[i]>='A')&&(line[i]<='Z')||(line[i]=='_')) { //identifier
				nextToken.type = "idf";
				nextToken.val = line[i];
				while(line[i+1]>='a'&&line[i+1]<='z'||line[i+1]>='A'&&line[i+1]<='Z'||line[i+1]>='0'&&line[i+1]<='9') {
					nextToken.val += line[++i];
				}
				tokens.push_back(nextToken);
			}
			else if(line[i]=='.') tokens.push_back(Token(".",""));
			else if(line[i]==':') tokens.push_back(Token(":",""));
			else if(line[i]==',') tokens.push_back(Token(",",""));
			else exitError("Unexpected Char "s+((char)line[i])+" in Line "+std::to_string(linenum)+":  "+line);
		}
		if(tokens.size() > tokennum) tokens.push_back(Token("endl",""));
	}
	std::cout << "Lexing complete." << std::endl;
	for(auto t : tokens) std::cout << "["<<t.type<<"|"<<t.val<<"]"<<std::endl;

	//LABEL FINDER
	int pc=0;
	bool afterLabel = false;
	bool idfSeen = false;
	bool idfSeenAfterLabel = false;
	std::string lastLabel("__toplevel");
	for(int i=0;i<tokens.size();++i) {
		if(tokens[i].type==":") { //label found
			if(i==0||tokens[i-1].type != "idf") exitError("Label symbol without Label name");
			std::string newLabel;
			if((i>1)&&(tokens[i-2].type == ".")) {
				newLabel = lastLabel+"."+tokens[i-1].val;
			} else {
				lastLabel = tokens[i-1].val;
				newLabel = tokens[i-1].val;
			}

			if(labels.count(newLabel)>0) exitError("Dublicate Label "+newLabel);
			labels[newLabel] = pc;
			afterLabel = true;
		}
		if(tokens[i].type=="idf") {idfSeen = true; }
		if(tokens[i].type=="idf"&&afterLabel) {idfSeenAfterLabel = true; }
		if(tokens[i].type=="endl") {
			if(!afterLabel && idfSeen) pc += 4; //line without label
			if(afterLabel && idfSeenAfterLabel) pc += 4; //line with label and command
			afterLabel = idfSeen = idfSeenAfterLabel = false;
		}
	}
	std::cout << "Finding Labels complete." << std::endl;
	for(auto l : labels) std::cout << "{"<<l.first<<","<<l.second<<"}"<<std::endl;

	std::ofstream ofile(argv[1], std::ios::binary);

#define CHECK_TOKENS(A,B) ((i+3>=tokens.size())||(tokens[i+1].type!=(A))||(tokens[i+2].type!=",")||(tokens[i+3].type!=(B)))
#define CHECK_REGISTER(A) (((A)[0]!='r')||(A).size()<=1)||((A).size()>3)
#define GET_REGNUM(A) (((A).size()==2)?((A)[1]-'0'):(((A)[1]-'0')*10+(A)[2]-'0'))

	pc = 0;
	//Code Generator
	for(int i=0;i<tokens.size();++i) {
		if(tokens[i].type=="idf") {
			if((i+1<tokens.size())&&(tokens[i+1].type=="idf")) {
				//if not label
				if(tokens[i].val=="SET") {
					if(CHECK_TOKENS("idf","num")) exitError("SET Opcode requires two Arguments: reg and num");
					if(CHECK_REGISTER(tokens[i+1].val)) exitError(tokens[i+1].val+" is not a Register definition.");
					uint32_t cmd = 0x10000000;
					cmd |= GET_REGNUM(tokens[i+1].val)<<24;
					cmd |= std::stoi(tokens[i+3].val)&0xFFFFFF;
					std::cout << "CMD: "<<std::hex<<cmd<<" | "<<tokens[i+3].val<<" | "<< ((int)GET_REGNUM(tokens[i+1].val))<<std::endl;
					ofile.write((char*)&cmd, sizeof(uint32_t));
					ofile.flush();

					pc += 4;
					i+=3;//skip extra arguments
				}
				else exitError("Unknown Opcode "+tokens[i].val);
			}
		}
	}
	ofile.close();
}
