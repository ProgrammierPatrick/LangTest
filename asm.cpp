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
				while(line[i+1]>='0'&&line[i+1]<='9'||line[i+1]=='x'||line[i+1]>='A'&&line[i+1]<='F'||line[i+1]>='a'&&line[i+1]<='f') nextToken.val += line[++i];
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

	//add newline to the end for code generator
	tokens.push_back(Token("endl",""));

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

#define GET_REGNUM(A) (((A).size()==2)?((A)[1]-'0'):(((A)[1]-'0')*10+(A)[2]-'0'))
#define ASSERT_TOKEN(I) if((I)>=tokens.size()) exitError("Additional Text Expected after "+tokens[i].val)

	pc = 0;
	//Code Generator
	for(int i=0;i<tokens.size();++i) {
		if(tokens[i].type=="idf") {
			if((i+1<tokens.size())&&((tokens[i+1].type=="idf")||(tokens[i+1].type=="endl"))) {
				std::string opcode = tokens[i].val;
				for(int j=0;j<opcode.size();++j) opcode[j]&=0x5F; //toUpperCase ASCII
				std::string args[3];
				uint32_t cmd = 0; //instruction to be written to file

				if(tokens[i+1].type=="endl") {
					//HANDLE EXPRESSION <OPCODE>
					     if(opcode=="IFL")	cmd = 0x02000008;
					else if(opcode=="IFM")	cmd = 0x02000004;
					else if(opcode=="IFLE")	cmd = 0x02000009;
					else if(opcode=="IFME")	cmd = 0x02000005;
					else if(opcode=="IFE")	cmd = 0x02000001;
					else if(opcode=="IFNE")	cmd = 0x02000002;
					else exitError("Unsupported Opcode "+opcode);
				} else {
					args[0] = tokens[i+1].val;
					if(tokens[i+1].type!="idf") exitError("Identifier Expected after "+tokens[i].val);
					ASSERT_TOKEN(i+2);
					if(tokens[i+2].type=="endl") {
						//HANDLE EXPRESSION <OPCODE ARG>
						if(opcode=="PUSH") {
							cmd = 0x00100000 | GET_REGNUM(args[0]);
						} else if(opcode=="POP") {
							cmd = 0x00110000 | GET_REGNUM(args[0]);
						} else exitError("Unsupported Opcode "+opcode);
						i += 2;
					}
					else {
						if(tokens[i+2].type!=",") exitError("Comma Expected after "+tokens[i+1].val);
						ASSERT_TOKEN(i+3);
						args[1] = tokens[i+3].val;

						if(tokens[i+3].type=="num") {
							ASSERT_TOKEN(i+4);
							if(tokens[i+4].type!="endl")
								exitError("Lineend Expected after"+tokens[i+3].val);

							//HANDLE EXPRESSION <OPCODE ARG1, NUM>
							if(opcode=="SET") {
								cmd = 0x10000000;
								cmd |= GET_REGNUM(args[0])<<24;
								if(args[1][0]=='x') cmd |= std::strtoul(args[1].substr(1).c_str(),NULL,16);
								else cmd |= std::stoi(args[1])&0xFFFFFF;
							} else exitError("Unsupported Opcode "+opcode+" <Num> at Token "+std::to_string(i));
		
							i += 4;
						} else if(tokens[i+3].type=="idf") {
							ASSERT_TOKEN(i+4);
							if(tokens[i+4].type=="endl") {
		
								//HANDLE EXPRESSION <OPCODE ARG1, ARG2>
								if(opcode=="SET") {
									cmd = 0x10000000;
									cmd |= GET_REGNUM(args[0])<<24;
									if(labels.find(args[1]) == labels.end()) exitError("Cannot find Label "s+args[1]);
                                    cmd |= labels[args[1]] && 0xFFFFFF;
								}
								else if(opcode=="NOT") {
									cmd = 0x01120000;
									cmd |=GET_REGNUM(args[0])<<8|GET_REGNUM(args[1])<<4;
								} else if(opcode=="CMP") {
									cmd = 0x01200000;
									cmd |=GET_REGNUM(args[0])<<4|GET_REGNUM(args[1]);
								} else if(opcode=="LDR") {
									cmd = 0x00000000;
									cmd|=GET_REGNUM(args[0])<<4|GET_REGNUM(args[1]);
								} else if(opcode=="STR") {
									cmd = 0x00010000;
									cmd|=GET_REGNUM(args[0])<<4|GET_REGNUM(args[1]);
								} else exitError("Unsupported Opcode "+opcode+" <IDF> at Token "+std::to_string(i));
								i += 4;
							} else if(tokens[i+4].type==",") {
								ASSERT_TOKEN(i+5);
								if(tokens[i+5].type!="idf")
									exitError("Expected Text After comma, "+tokens[i].val);
								args[2] = tokens[i+5].val;
								ASSERT_TOKEN(i+6);
								if(tokens[i+6].type!="endl")
									exitError("Expected Lineend after "+tokens[i].val);
								
								//HANDLE EXPRESSION <OPCODE ARG1, ARG2, ARG3>
								if(opcode=="ADD") {
									cmd = 0x01000000;
									cmd |= GET_REGNUM(args[0])<<8;
									cmd |= GET_REGNUM(args[1])<<4 | GET_REGNUM(args[2]);
								} else if(opcode=="SUB") {
									cmd = 0x01010000;
									cmd |= GET_REGNUM(args[0])<<8;
									cmd |= GET_REGNUM(args[1])<<4 | GET_REGNUM(args[2]);
								} else if(opcode=="MUL") {
									cmd = 0x01020000;
									cmd |= GET_REGNUM(args[0])<<8;
									cmd |= GET_REGNUM(args[1])<<4 | GET_REGNUM(args[2]);
								} else if(opcode=="DIV") {
									cmd = 0x01030000;
									cmd |= GET_REGNUM(args[0])<<8;
									cmd |= GET_REGNUM(args[1])<<4 | GET_REGNUM(args[2]);
								} else if(opcode=="MOD") {
									cmd = 0x01040000;
									cmd |= GET_REGNUM(args[0])<<8;
									cmd |= GET_REGNUM(args[1])<<4 | GET_REGNUM(args[2]);
								} else if(opcode=="AND") {
									cmd = 0x01100000;
									cmd |= GET_REGNUM(args[0])<<8;
									cmd |= GET_REGNUM(args[1])<<4 | GET_REGNUM(args[2]);
								} else if(opcode=="OR") {
									cmd = 0x01110000;
									cmd |= GET_REGNUM(args[0])<<8;
									cmd |= GET_REGNUM(args[1])<<4 | GET_REGNUM(args[2]);
								} else if(opcode=="XOR") {
									cmd = 0x01130000;
									cmd |= GET_REGNUM(args[0])<<8;
									cmd |= GET_REGNUM(args[1])<<4 | GET_REGNUM(args[2]);
								} else exitError("Unsupported Opcode "+opcode+" <IDF> <IDF> at Token "+std::to_string(i));

								i += 6;
							} else exitError("Comma or linened exprected after "+tokens[i+3].val);
						} else exitError("Identifier or Number expected after "+tokens[i+2].val);
					}
				}
	
				ofile.write((char*)&cmd, sizeof(uint32_t));
				ofile.flush();
			} //if
		} //if
	} //for i
	ofile.close();
}
