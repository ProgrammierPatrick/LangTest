#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <inttypes.h>
#include <memory>
#include <algorithm>

using namespace std::literals::string_literals;
void exitError(std::string const& str, int y, int x) {
	std::cout << str << std::endl;
	std::cout << "In Line " << y << " char " << x << std::endl;
	exit(-1);
}

// ====================== LEXING ==============================

struct Token {
	std::string type;
	std::string val;
	int sourceY;
	int sourceX;
	Token() {}
	Token(std::string t, std::string v, int y, int x) { type = t; val = v; sourceY = y; sourceX = x; }
};

std::vector<Token> tokens;

void putToken(std::string const& str, int y, int x) {
	#define IF_EQ(X) if(str == (X)) tokens.push_back(Token((X),"",y,x))
	if(str == "") return;
	IF_EQ("if"); else IF_EQ("else");
	else IF_EQ("while");
	else IF_EQ("var");
	else IF_EQ(":"); else IF_EQ(";"); else IF_EQ("{"); else IF_EQ("}");
	else IF_EQ("+"); else IF_EQ("-"); else IF_EQ("*"); else IF_EQ("/"); else IF_EQ("%");
	else IF_EQ("&"); else IF_EQ("|"); else IF_EQ("!"); else IF_EQ("^");
	else IF_EQ(">"); else IF_EQ("<"); else IF_EQ(">="); else IF_EQ("<=");
	else IF_EQ("!="); else IF_EQ("=="); else IF_EQ("=");
	else IF_EQ("("); else IF_EQ(")");
	else if(str[0] == '$') tokens.push_back(Token("l_var", str.substr(1),y,x));
	else if(str[0] == '*') tokens.push_back(Token("l_ptr", str.substr(1),y,x));
	else if(str[0] == '&') tokens.push_back(Token("l_adr", str.substr(1),y,x));
	else if((str[0] >= '0') && (str[0] <= '9')) tokens.push_back(Token("num", str,y,x));
	else tokens.push_back(Token("name", str, y,x));
}

void lexer(std::ifstream &file) {
	std::string line("");
	std::string str("");

	#define IS_WHITESPACE(X) (((X) == ' ') || ((X) == '\n') || ((X) == '\t'))
	#define IS_NAME(X) (((X) >= 'a') && ((X) <= 'z') || ((X) >= 'A') && ((X) <= 'Z') || ((X) == '_'))
	#define IS_NUMBER(X) (((X) >= '0') && ((X) <= '9') || ((X) >= 'a') && ((X) <= 'f') || ((X) == '_'))
	
	int linenum = 0;
	for(; std::getline(file, line); ++linenum) {
		for(int x = 0; x < line.size(); ++x) {
			if(IS_WHITESPACE(line[x])) { putToken(str, linenum, x); str = ""; }
			else if(line[x] == ':') { putToken(str, linenum, x); putToken(":", linenum, x); str = ""; }
			else if(line[x] == ';') { putToken(str, linenum, x); putToken(";", linenum, x); str = ""; }
			else if(line[x] == '{') { putToken(str, linenum, x); putToken("{", linenum, x); str = ""; }
			else if(line[x] == '}') { putToken(str, linenum, x); putToken("}", linenum, x); str = ""; }
			else if(line[x] == '+') { putToken(str, linenum, x); putToken("+", linenum, x); str = ""; }
			else if(line[x] == '-') { putToken(str, linenum, x); putToken("-", linenum, x); str = ""; }
			else if(line[x] == '*') {
				putToken(str, linenum, x);
				if(!IS_NAME(line[x+1])) {
					putToken("*", linenum, x);
					str = "";
				} else str = "*";
			}
			else if(line[x] == '/') { putToken(str, linenum, x); putToken("/", linenum, x); str = ""; }
			else if(line[x] == '%') { putToken(str, linenum, x); putToken("%", linenum, x); str = ""; }
			else if(line[x] == '&') {
				putToken(str, linenum, x);
				if(!IS_NAME(line[x+1])) {
					putToken("&", linenum, x);
					str = "";
				}
			}
			else if(line[x] == '|') { putToken(str, linenum, x); putToken("|", linenum, x); str = ""; }
			else if(line[x] == '!') {
				putToken(str, linenum, x);
				if(line[x+1] == '=') { putToken("!=", linenum, x); str = ""; x++; }
				else { putToken("!", linenum, x); str = ""; }
			}
			else if(line[x] == '^') { putToken(str, linenum, x); putToken("^", linenum, x); str = ""; }
			else if(line[x] == '>') {
				putToken(str, linenum, x);
				if(line[x+1] == '=') { putToken(">=", linenum, x); str = ""; x++; }
				else { putToken(">", linenum, x); str = ""; }
			}	
			else if(line[x] == '<') {
				putToken(str, linenum, x);
				if(line[x+1] == '=') { putToken("<=", linenum, x); str = ""; x++; }
				else { putToken("<", linenum, x); str = ""; }
			}	
			else if(line[x] == '=') {
				putToken(str, linenum, x);
				if(line[x+1] == '=') { putToken("==", linenum, x); str = ""; x++; }
				else { putToken("=", linenum, x); str = ""; }
			}	
			else if(line[x] == '(') { putToken(str, linenum, x); putToken("(", linenum, x); str = ""; }
			else if(line[x] == ')') { putToken(str, linenum, x); putToken(")", linenum, x); str = ""; }
			else str += line[x];
		}
	}
	putToken(str, linenum-1, 0);

}

// ======================= PARSING =========================================
enum NodeType { BLOCK, INSTRUCTION, R_VAL };

struct ParseNode {
	NodeType type;
	std::string operation;
	std::vector<std::string> args;
	std::vector<ParseNode*> childs;
	ParseNode *parent;
};

ParseNode parseTreeRoot;
ParseNode *parseTreeLast = &parseTreeRoot;

//DEBUG: forward declarations
void printParseNode(ParseNode *node, int level = 0);
std::string parseNodeTypeToString(NodeType type);

std::string operators[] = {"&","|","^","*","/","%","+","-",">","<",">=","<=","!=","=="};
//0: strongest
int operatorRating(std::string op) {
	if(op == "(") // ) is never on stack
		return 0;
	if(op == "&")
		return 1;
	if(op == "|")
		return 2;
	if(op == "^")
		return 3;
	if((op == "*") || (op == "/") || (op == "%"))
		return 4;
	if((op == "+") || (op == "-"))
		return 5;
	if((op == ">") || (op == "<") || (op == ">=") || (op == "<=") || (op == "!=") || (op == "=="))
		return 6;
	exitError("Unknown Operator"s + op + "in operatorRating",-1,-1);
}

//return: first token after comsumed tokens
int parse(int pos, NodeType type, ParseNode *parent) {
	printParseNode(&parseTreeRoot);
	std::cout << "Now to " << parseNodeTypeToString(type) << std::endl;
	
	#define ASSERT_TYPE(P, T) if(tokens[P].type != (T)) \
		exitError("Parse Error: Sould be of type "s \
			+ (T), tokens[P].sourceY, tokens[P].sourceX)

	//link new parse tree node into tree
	ParseNode *newNode = new ParseNode();
	newNode->parent = parent;
	newNode->type = type;
	parent->childs.push_back(newNode);

	switch (type) {
	case INSTRUCTION:
		if(tokens[pos].type == "var") { //Declaration
			newNode->operation = "declare";
			ASSERT_TYPE(pos+1, "name");
			newNode->args.push_back(tokens[pos+1].val);
			ASSERT_TYPE(pos+2, ";");
			return pos+3;
		} else if(tokens[pos].type[0] == 'l' && tokens[pos].type[1] == '_') { //Assignment
			newNode->operation = "assign";
			if(tokens[pos].type == "l_var") newNode->args.push_back("var");
			if(tokens[pos].type == "l_ptr") newNode->args.push_back("adr");
			if(tokens[pos].type == "l_adr") newNode->args.push_back("ptr");
			newNode->args.push_back(tokens[pos].val);
			ASSERT_TYPE(pos+1, "=");
			pos = parse(pos+2, R_VAL, newNode);
			ASSERT_TYPE(pos, ";");
			return pos+1;
		} else if(tokens[pos].type == "if") {
			newNode->operation = "if";
			pos = parse(pos+1, R_VAL, newNode); // parse Condition
			ASSERT_TYPE(pos, ":");
			pos = parse(pos+1, INSTRUCTION, newNode); // parse THEN-Part
			if(tokens[pos].type == "else") {
				pos = parse(pos+1, INSTRUCTION, newNode); // parse ELSE-Part
				return pos;
			}
			else return pos;
		} else if(tokens[pos].type == "while") {
			newNode->operation = "while";
			pos = parse(pos+1, R_VAL, newNode);
			ASSERT_TYPE(pos, ":");
			pos = parse(pos+1, INSTRUCTION, newNode);
			return pos;
		} else if(tokens[pos].type == "{") {
			newNode->type = BLOCK;
			pos++;
			do {
				pos = parse(pos, INSTRUCTION, newNode);
			} while(tokens[pos].type != "}");

			return pos+1;
		}
		exitError("Token [" + tokens[pos].type + "," + tokens[pos].val + "] not expected.", tokens[pos].sourceY, tokens[pos].sourceX);

	case R_VAL:
		// Use Dijkstras Shunting Yard Algorithm to convert Infix Notation to Postfix
		std::vector<std::string> operatorStack;

		for(;;++pos) {
			bool isOperator = false;
			for(std::string s : operators)
				if(s == tokens[pos].type)
					isOperator = true;

			if(isOperator) {
				if(!operatorStack.empty() &&
						operatorRating(tokens[pos].type) <= operatorRating(operatorStack.back())) {
					newNode->args.push_back(operatorStack.back());
					operatorStack.pop_back();
				}
				operatorStack.push_back(tokens[pos].type);
			}
			else if(tokens[pos].type == "(") {
				operatorStack.push_back("(");
			}
			else if(tokens[pos].type == ")") {
				while(operatorStack.back() != "(") {
					newNode->args.push_back(operatorStack.back());
					operatorStack.pop_back();
					if(operatorStack.empty())
						exitError("No matching ) for ( found", tokens[pos].sourceY, tokens[pos].sourceX);
				}
				//opStack.back() is now "("
				operatorStack.pop_back();
			}
			else if((tokens[pos].type == ";") || (tokens[pos].type == ":")) {
				if(!operatorStack.empty()) newNode->args.push_back(operatorStack.back());
				operatorStack.pop_back();
				if(!operatorStack.empty()) exitError("Error while doing Shunting Yard Algorithmus",-1,-1);
				return pos;
			}
			else {
				newNode->args.push_back(tokens[pos].type);
				newNode->args.push_back(tokens[pos].val);
			}
		}
		if(operatorStack.size() > 1) exitError("Too many operators", tokens[pos].sourceY, tokens[pos].sourceX);
		if(operatorStack.size() == 1) newNode->args.push_back(operatorStack[0]);
		return pos;
	}
}

void parse() {
	parseTreeRoot.type = BLOCK;
	parseTreeRoot.parent = nullptr;

	int i = 0;
	do {
		i = parse(i, INSTRUCTION, &parseTreeRoot);
		std::cout << i << std::endl;
	} while(i < tokens.size());
}

std::string parseNodeTypeToString(NodeType type) {
	if(type == BLOCK) return "BLOCK";
	if(type == INSTRUCTION) return "INSTRUCTION";
	if(type == R_VAL) return "R_VAL";
}

void printParseNode(ParseNode *node, int level/* = 0*/) {
	for(int i=0;i<level;++i) std::cout << "  ";
	std::cout << "[ " << parseNodeTypeToString(node->type) << ", " << node->operation;
	for(auto &a : node->args) std::cout << ", " << a;
	std::cout << "]" << std::endl;
	for(ParseNode *c : node->childs) printParseNode(c, level+1);
}

// ========================= CODE GENERATION ================================
struct StackFrame;

std::string ass;
std::vector<StackFrame> stackFrames;
int nextJumpMark = 0;

struct Var {
	std::string name;
	int address;
	Var(std::string n, int a) { name = n; address = a; }
};

const int stackBase = 0x00100000;

struct StackFrame {
	int base;
	std::vector<Var> variables;
	int length() { return variables.size(); }
	int nextAddr() { return base + length(); }
	StackFrame(int b) { base = b; }
};


Var *getVar(std::string &name) {
	for(int i = stackFrames.size()-1;i>=0;i--) {
		StackFrame &frame = stackFrames[i];
		for(Var &v : frame.variables) {
			if(v.name == name) return &v;
		}
	}
	return NULL;
}

bool existsVar(std::string &name) {
	return getVar(name) != NULL;
}

void addVar(std::string &name) {
    stackFrames.back().variables.push_back(Var(name, stackFrames.back().nextAddr()));
}


//template <class T>
//bool contains(std::vector<T> &vec, T &el) {
//	return std::find(vec.begin(), vec.end(), el) != vec.end();
//}

void generateNode(ParseNode *node = &parseTreeRoot) {
	if(node->type == BLOCK) {
		//new scope
		if(stackFrames.empty()) stackFrames.push_back(StackFrame(stackBase));
		else stackFrames.push_back(StackFrame(stackFrames.back().nextAddr()));
		for(ParseNode *child : node->childs)
			generateNode(child);
		stackFrames.pop_back();
	} else if (node->type == INSTRUCTION) {
		if(node->operation == "declare") {
			if(existsVar(node->args[0]))
				exitError("Variable "s+node->args[0]+" already declared.",-1,-1);
			addVar(node->args[0]);
		} else if(node->operation == "assign") {
			if(!existsVar(node->args[1])) exitError("Variable "s+node->args[1]+" does not exists.", -1,-1);
			Var *var = getVar(node->args[1]);

            generateNode(node->childs[0]);

			if(node->args[0] == "var") {
				ass += "POP R2\n"s
				+ "SET R3, " + std::to_string(var->address) + "\n"
				+ "STR R2, R3\n\n";
			}
		} else ass += "Unsupported Instruction:"s+node->operation+"\n";
	} else if (node->type == R_VAL) {
		ass += "R-Val\n\n";
	} else exitError("Unsupported Node type.",-1,-1);
}

// ========================= MAIN ===========================================
int main(int argc, char **argv) {
	if(argc < 3) {
		exitError("too few arguments.\nSyntax: "s+argv[0]+" [assembly] [source]", 0, 0);
	}

	std::ifstream file(argv[2]);
	if(!file.is_open()) exitError("Could not open file "s+argv[2], 0, 0);

	//LEXER
	lexer(file);

	//print tokens
	std::cout << "Lexing done.\nresults:\n";
	for(Token t : tokens) {
		if(t.val != "")
			std::cout << "[ " << t.type << ", " << t.val << " ]" << std::endl;
		else
			std::cout << "[ " << t.type << " ]" << std::endl;
	}

	//PARSER
	parse();

	std::cout << "\nParsing node.\nResults:\n";
	printParseNode(&parseTreeRoot);

	//CODE GENERATOR
	generateNode();
	
	std::cout << "\nGode generated.\nResults:\n" << ass << std::endl;
	
}
