#include "sim_pipe.h"
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <iomanip>
#include <map>

//#define DEBUG

using namespace std;

//used for debugging purposes
static const char *reg_names[NUM_SP_REGISTERS] = {"PC", "NPC", "IR", "A", "B", "IMM", "COND", "ALU_OUTPUT", "LMD"};
static const char *stage_names[NUM_STAGES] = {"IF", "ID", "EX", "MEM", "WB"};
static const char *instr_names[NUM_OPCODES] = {"LW", "SW", "ADD", "ADDI", "SUB", "SUBI", "XOR", "BEQZ", "BNEZ", "BLTZ", "BGTZ", "BLEZ", "BGEZ", "JUMP", "EOP", "NOP"};

/* =============================================================

   HELPER FUNCTIONS

   ============================================================= */


/* converts integer into array of unsigned char - little indian */
inline void int2char(unsigned value, unsigned char *buffer){
	//cout<<"\n int2char buffer: "<<static_cast<void const*>(buffer)<< " Value: "<<value;

	memcpy(buffer, &value, sizeof value);
}

/* converts array of char into integer - little indian */
inline unsigned char2int(unsigned char *buffer){
	//cout<<"\n char2int buffer: "<<static_cast<void const*>(buffer);
	unsigned d;
	memcpy(&d, buffer, sizeof d);
	return d;
}

/* implements the ALU operations */
unsigned alu(unsigned opcode, unsigned a, unsigned b, unsigned imm, unsigned npc){
	switch(opcode){
	case ADD:
		return (a+b);
	case ADDI:
		return(a+imm);
	case SUB:
		return(a-b);
	case SUBI:
		return(a-imm);
	case XOR:
		return(a ^ b);
	case LW:
	case SW:
		return(a + imm);
	case BEQZ:
	case BNEZ:
	case BGTZ:
	case BGEZ:
	case BLTZ:
	case BLEZ:
	case JUMP:
		return(npc+imm);
	default:
		return (-1);
	}
}

/* =============================================================

   CODE PROVIDED - NO NEED TO MODIFY FUNCTIONS BELOW

   ============================================================= */

/* loads the assembly program in file "filename" in instruction memory at the specified address */
void sim_pipe::load_program(const char *filename, unsigned base_address){

	/* initializing the base instruction address */
	instr_base_address = base_address;

	/* Point Program Counter to start address of program*/
	specialP_Reg[IF][PC] = instr_base_address;

	/* creating a map with the valid opcodes and with the valid labels */
	map<string, opcode_t> opcodes; //for opcodes
	map<string, unsigned> labels;  //for branches
	for (int i=0; i<NUM_OPCODES; i++)
		opcodes[string(instr_names[i])]=(opcode_t)i;

	/* opening the assembly file */
	ifstream fin(filename, ios::in | ios::binary);
	if (!fin.is_open()) {
		cerr << "error: open file " << filename << " failed!" << endl;
		exit(-1);
	}

	/* parsing the assembly file line by line */
	string line;
	unsigned instruction_nr = 0;
	while (getline(fin,line)){
		// set the instruction field
		char *str = const_cast<char*>(line.c_str());

		// tokenize the instruction
		char *token = strtok (str," \t");
		map<string, opcode_t>::iterator search = opcodes.find(token);
		if (search == opcodes.end()){
			// this is a label for a branch - extract it and save it in the labels map
			string label = string(token).substr(0, string(token).length() - 1);
			labels[label]=instruction_nr;
			// move to next token, which must be the instruction opcode
			token = strtok (NULL, " \t");
			search = opcodes.find(token);
			if (search == opcodes.end()) cout << "ERROR: invalid opcode: " << token << " !" << endl;
		}
		instr_memory[instruction_nr].opcode = search->second;

		//reading remaining parameters
		char *par1;
		char *par2;
		char *par3;
		switch(instr_memory[instruction_nr].opcode){
		case ADD:
		case SUB:
		case XOR:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			par3 = strtok (NULL, " \t");
			instr_memory[instruction_nr].dest = atoi(strtok(par1, "R"));
			instr_memory[instruction_nr].src1 = atoi(strtok(par2, "R"));
			instr_memory[instruction_nr].src2 = atoi(strtok(par3, "R"));
			break;
		case ADDI:
		case SUBI:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			par3 = strtok (NULL, " \t");
			instr_memory[instruction_nr].dest = atoi(strtok(par1, "R"));
			instr_memory[instruction_nr].src1 = atoi(strtok(par2, "R"));
			instr_memory[instruction_nr].immediate = strtoul (par3, NULL, 0);
			break;
		case LW:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			instr_memory[instruction_nr].dest = atoi(strtok(par1, "R"));
			instr_memory[instruction_nr].immediate = strtoul(strtok(par2, "()"), NULL, 0);
			instr_memory[instruction_nr].src1 = atoi(strtok(NULL, "R"));
			break;
		case SW:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			instr_memory[instruction_nr].src1 = atoi(strtok(par1, "R"));
			instr_memory[instruction_nr].immediate = strtoul(strtok(par2, "()"), NULL, 0);
			instr_memory[instruction_nr].src2 = atoi(strtok(NULL, "R"));
			break;
		case BEQZ:
		case BNEZ:
		case BLTZ:
		case BGTZ:
		case BLEZ:
		case BGEZ:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			instr_memory[instruction_nr].src1 = atoi(strtok(par1, "R"));
			instr_memory[instruction_nr].label = par2;
			break;
		case JUMP:
			par2 = strtok (NULL, " \t");
			instr_memory[instruction_nr].label = par2;
			break;

		default:
			break;

		}

		/* increment instruction number before moving to next line */
		instruction_nr++;
	}
	//reconstructing the labels of the branch operations
	int i = 0;
	while(true){
		instruction_t instr = instr_memory[i];
		if (instr.opcode == EOP) break;
		if (instr.opcode == BLTZ || instr.opcode == BNEZ ||
				instr.opcode == BGTZ || instr.opcode == BEQZ ||
				instr.opcode == BGEZ || instr.opcode == BLEZ ||
				instr.opcode == JUMP
		){
			instr_memory[i].immediate = (labels[instr.label] - i - 1) << 2;
		}
		i++;
	}
	//copy branch-labels into member variable
	std::map<std::string, unsigned>::iterator mapIt = labels.begin();
	labelPCMap.insert( labels.begin(), labels.end());
}

/* writes an integer value to data memory at the specified address (use little-endian format: https://en.wikipedia.org/wiki/Endianness) */
void sim_pipe::write_memory(unsigned address, unsigned value){

	//cout<<"\n #write_memory address: "<<static_cast<void const*>(data_memory+address)<<" Value: "<<value;

	int2char(value, data_memory+address);
}

/* prints the content of the data memory within the specified address range */
void sim_pipe::print_memory(unsigned start_address, unsigned end_address){

	//	cout<<"\n #print_memory data_memory: "<<static_cast<void const*>(data_memory);

	cout << "data_memory[0x" << hex << setw(8) << setfill('0') << start_address << ":0x" << hex << setw(8) << setfill('0') <<  end_address << "]" << endl;
	for (unsigned i=start_address; i<end_address; i++){
		if (i%4 == 0) cout << "0x" << hex << setw(8) << setfill('0') << i << ": ";
		cout << hex << setw(2) << setfill('0') << int(data_memory[i]) << " ";
		if (i%4 == 3) cout << endl;
	}
}

/* prints the values of the registers */
void sim_pipe::print_registers(){

	cout << "Special purpose registers:" << endl;
	unsigned i, s;
	for (s=0; s<NUM_STAGES; s++){
		cout << "Stage: " << stage_names[s] << endl;
		for (i=0; i< NUM_SP_REGISTERS; i++)
			if ((sp_register_t)i != IR && (sp_register_t)i != COND && get_sp_register((sp_register_t)i, (stage_t)s)!=UNDEFINED) cout << reg_names[i] << " = " << dec <<  get_sp_register((sp_register_t)i, (stage_t)s) << hex << " / 0x" << get_sp_register((sp_register_t)i, (stage_t)s) << endl;
	}
	cout << "General purpose registers:" << endl;
	for (i=0; i< NUM_GP_REGISTERS; i++)
		if (get_gp_register(i)!=(int)UNDEFINED) cout << "R" << dec << i << " = " << get_gp_register(i) << hex << " / 0x" << get_gp_register(i) << endl;
}

/* initializes the pipeline simulator */
sim_pipe::sim_pipe(unsigned mem_size, unsigned mem_latency){
	data_memory_size = mem_size;
	data_memory_latency = mem_latency;
	data_memory = new unsigned char[data_memory_size];

	reset();
}

/* deallocates the pipeline simulator */
sim_pipe::~sim_pipe(){
	delete [] data_memory;
}

/* =============================================================

   CODE TO BE COMPLETED

   ============================================================= */


/* body of the simulator */
void sim_pipe::run(unsigned cycles){

	if(cycles == 0) runAlways = 1;

	cout<<"\n## START of run, clkIn: "<<clkIn<<" cycles: "<<cycles;

	run:
	do
	{

		switch(clkIn)
		{
		cout<<"\n## Switch, clkIn: "<<clkIn<<" cycles: "<<cycles;

		case (IF+1):
			fetch();
		break;

		case (ID+1):
			decode();//c=3
		break;

		case (EXE+1):
			execute();//c=4
		break;

		case (MEM+1):
			memory();//c=5
		break;

		case (WB+1):
			writeBack();
		break;

		default:
			if(clkIn > (WB+1)) writeBack();
		}

	}while(runAlways);//end of while

	if(runAlways){ goto run;}

	cout<<"\n## END of run, clkIn: "<<clkIn<<"\n";

	return;
}

/* reset the state of the pipeline simulator */
void sim_pipe::reset(){

	std::fill_n(data_memory, data_memory_size, 0xFF);

	/* Reset object's member variables */

	std::fill_n(generalP_Reg, NUM_GP_REGISTERS, UNDEFINED);

	for(int k = IF; k <= WB ; k++)
		std::fill_n(specialP_Reg[k], NUM_SP_REGISTERS, UNDEFINED);

	pipe_reg[FIRST].reset();
	pipe_reg[SECOND].reset();
	pipe_reg[THIRD].reset();
	pipe_reg[FORTH].reset();

	clkIn = 1;
	runAlways = 0;
	inst_count = 0;
	totalInstCount = 0;

	stalls = 0;
	totalStalls = 0;
	stallMem = 0;
	currentClk = 0;
	branchToLabel = "";

	noBranches = true;
	branchStall = false;
	memoryStall = false;
	memStallCompleted = false;

}

//return value of special purpose register
unsigned sim_pipe::get_sp_register(sp_register_t reg, stage_t s)
{
	if( (reg >= 0 ) && (reg < NUM_SP_REGISTERS) && (s>=0) && (s<5))
	{
		//cout<<"value of register: reg-%d"<<reg<<"value @ reg-%X"<<specialP_Reg[reg]<<"stage s-%d"<<s;
		return specialP_Reg[s][reg];
	}

	return 0;
}

//returns value of general purpose register
int sim_pipe::get_gp_register(unsigned reg)
{
	if( (reg >= 0 ) && (reg < NUM_GP_REGISTERS))
	{
		//cout<<"value of register: reg-%d value @ reg-%X", reg, generalP_Reg[reg];
		return generalP_Reg[reg];
	}

	return 0;
}

void sim_pipe::set_gp_register(unsigned reg, int value){

	if( (reg >= 0 ) && (reg < NUM_GP_REGISTERS))
	{
		generalP_Reg[reg] = value;
	}
}

float sim_pipe::get_IPC(){
	float IPC = float(totalInstCount)/float(clkIn);
	return IPC;
}

unsigned sim_pipe::get_instructions_executed(){
	return totalInstCount;
}

unsigned sim_pipe::get_stalls(){
	return totalStalls;
}

unsigned sim_pipe::get_clock_cycles(){
	return clkIn;
}


void sim_pipe::fetch()
{
	cout<<"\nIn fetch clkIn: "<<clkIn<<"\t current instruction is: "<<inst_count+1;
	cout<<"\n memoryStall: "<<memoryStall<<"stallMem: "<<stallMem<<"\n";

	cout<<"\n stalls: "<<stalls;
	cout<<"\n branchStall: "<<branchStall;

	if(inst_count == 8)
	{
		cout<<"\n 8insttotalStalls: "<<totalStalls;
	}
	if(memoryStall) return;

	if(stalls)
	{
		if(branchStall) pipe_reg[FIRST].reset();

		cout<<"\n Need stalling, skipping fetching new inst";
		cout<<"\n branchStall:    "<<branchStall;

		cout<<"\n inst_count:     "<<inst_count;
		cout<<"\n totalInstCount: "<<totalInstCount<<"\n";
		return;
	}

	cout<<"\n Fetch input: 1.opcode: "<<instr_memory[inst_count].opcode;
	cout<<"\n Fetch input: instruct: "<<(instr_names[instr_memory[inst_count].opcode]);
	cout<<"\n Fetch input: 2.dest:   "<<instr_memory[inst_count].dest;
	cout<<"\n Fetch input: 3.src1:   "<<instr_memory[inst_count].src1;
	cout<<"\n Fetch input: 4.src2:   "<<instr_memory[inst_count].src2;
	cout<<"\n Fetch input: 5.imm:    "<<instr_memory[inst_count].immediate;
	cout<<"\n Fetch input: 6.label:  "<<instr_memory[inst_count].label<<"\n";

	std::string emptyStr = "";
	if(branchToLabel != emptyStr)
	{
		unsigned jumpToInst = (labelPCMap.find(branchToLabel))->second;

		cout<<"\n save labels & PCs";
		cout<<"\n pipe_reg[FIRST].pipe_IR.label: "<<pipe_reg[FIRST].pipe_IR.label;
		cout<<"\n specialP_Reg[IF][PC]: "<<specialP_Reg[IF][PC];
		cout<<"\n &instr_memory[inst_count]: "<<(&instr_memory[inst_count]);
		cout<<"\n inst_count: "<<inst_count;
		cout<<"\n jumpToInst: "<<jumpToInst;

		inst_count = jumpToInst;
		branchToLabel = emptyStr;
	}

	//update IR register of pipeline reg first
	std::memcpy(&pipe_reg[FIRST].pipe_IR, &instr_memory[inst_count], sizeof (instruction_t) );
	specialP_Reg[IF][IR] = pipe_reg[FIRST].pipe_IR.opcode;
	specialP_Reg[ID][IR] =  specialP_Reg[IF][IR];

	if(specialP_Reg[IF][IR] != EOP)
	{
		//1.update PC, NPC
		if(clkIn == 1){//FIRST instr
			specialP_Reg[IF][PC] = instr_base_address+(4*inst_count) + 4;
			specialP_Reg[ID][NPC] = specialP_Reg[IF][PC];
		}
		else if(clkIn > 1){
			specialP_Reg[ID][NPC] = instr_base_address+(4*inst_count) + 4; //specialP_Reg[IF][PC] + 4;
			specialP_Reg[IF][PC] = specialP_Reg[ID][NPC];
		}

		++inst_count;
		++totalInstCount;
	}

	if(clkIn == (IF+1))
	{
		++clkIn;
		cout<<"\n FETCH: Incremented clkIn: "<<clkIn<<" & inst_count: "<<inst_count<<"\n";
	}

}

void sim_pipe::decode()
{
	cout<<"\n In DECODE clkIn: "<<clkIn;
	cout<<"\n In DECODE stalls: "<<stalls;
	cout<<"\n memoryStall: "<<memoryStall<<"stallMem: "<<stallMem<<"\n";

	//cout<<"\n DEC input: 1.opcode: "<<pipe_reg[FIRST].pipe_IR.opcode;
	cout<<"\n DEC input: instruct: "<<(instr_names[pipe_reg[FIRST].pipe_IR.opcode]);
	cout<<"\n DEC input: 2.dest:   "<<pipe_reg[FIRST].pipe_IR.dest;
	cout<<"\n DEC input: 3.src1:   "<<pipe_reg[FIRST].pipe_IR.src1;
	cout<<"\n DEC input: 4.src2:   "<<pipe_reg[FIRST].pipe_IR.src2;
	cout<<"\n DEC input: 5.imm:    "<<pipe_reg[FIRST].pipe_IR.immediate;
	cout<<"\n DEC input: 6.label:  "<<pipe_reg[FIRST].pipe_IR.label<<"\n";

	//if(memoryStall)	return; //140CYCLE, 75 STALLS

	//Find data-hazards & calculate stalls
	hazardHandler();

	if(memoryStall){
		return;
	}

	if(stalls && (!branchStall))
	{
		cout<<"\n hazard present stalls: "<<stalls;
		cout<<"\n clkIn: "<<clkIn<<"\n";
		pipe_reg[SECOND].reset();
		return;
	}
	else if( pipe_reg[FIRST].pipe_IR.opcode != NOP )
	{
		//over-write index values with actual values of respective registers
		pipe_reg[FIRST].pipe_IR.src1 = get_gp_register(pipe_reg[FIRST].pipe_IR.src1);
		pipe_reg[FIRST].pipe_IR.src2 = get_gp_register(pipe_reg[FIRST].pipe_IR.src2);
	}

	//2.update NPC
	specialP_Reg[EXE][NPC] = specialP_Reg[ID][NPC];
	specialP_Reg[EXE][IMM] = pipe_reg[FIRST].pipe_IR.immediate;

	if( ( specialP_Reg[ID][IR] != SW) &&
	    ( (pipe_reg[FIRST].pipe_IR.opcode != NOP) ||
	      (pipe_reg[FIRST].pipe_IR.opcode != EOP)  ) )
	{
		specialP_Reg[EXE][A]=pipe_reg[FIRST].pipe_IR.src1; //3

		if(specialP_Reg[ID][IR] != LW ) //LW doesnt have src2
			specialP_Reg[EXE][B]=pipe_reg[FIRST].pipe_IR.src2; //4
	}
	else
	if( specialP_Reg[ID][IR] == SW )
	{
		specialP_Reg[EXE][B] = pipe_reg[FIRST].pipe_IR.src1;
		specialP_Reg[EXE][A] = pipe_reg[FIRST].pipe_IR.src2;
		pipe_reg[FIRST].pipe_IR.src1 = specialP_Reg[EXE][A];
		pipe_reg[FIRST].pipe_IR.src2 = specialP_Reg[EXE][B];
	}

	specialP_Reg[EXE][IR] = specialP_Reg[ID][IR];

	//LOAD PIPE2 WITH PIPE1
	std::memcpy(&pipe_reg[SECOND], &pipe_reg[FIRST], sizeof (pipeline_Registers) );

	if(clkIn == (ID+1))
	{
		fetch();
		++clkIn;
		cout<<"\n---return true from decode---clkIn: "<<clkIn<<"\n";
	}
	cout<<"\n---Done decode---clkIn: "<<clkIn<<"\n";

}

void sim_pipe::execute()
{
	cout<<"\nIn exe clkIn: "<<clkIn<<"\n";
	//	cout<<"\n branchToLabel: "<<branchToLabel;
	cout<<"\n memoryStall: "<<memoryStall<<"stallMem: "<<stallMem<<"\n";

	cout<<"\n execute input instruct: "<<(instr_names[pipe_reg[SECOND].pipe_IR.opcode]);
	//cout<<"\n execute input 1.opcode: "<<pipe_reg[SECOND].pipe_IR.opcode;
	cout<<"\n execute input 2.dest:   "<<pipe_reg[SECOND].pipe_IR.dest;
	cout<<"\n execute input 3.src1:   "<<pipe_reg[SECOND].pipe_IR.src1;
	cout<<"\n execute input 4.src2:   "<<pipe_reg[SECOND].pipe_IR.src2;
	cout<<"\n execute input 5.imm:    "<<pipe_reg[SECOND].pipe_IR.immediate;
	cout<<"\n execute input 6.label:  "<<pipe_reg[SECOND].pipe_IR.label<<"\n";

	if(memoryStall) return;

	//exe: call alu()
	pipe_reg[SECOND].pipe_ALU_OUTPUT = alu(pipe_reg[SECOND].pipe_IR.opcode, pipe_reg[SECOND].pipe_IR.src1, pipe_reg[SECOND].pipe_IR.src2, pipe_reg[SECOND].pipe_IR.immediate, pipe_reg[SECOND].pipe_NPC);

	switch(pipe_reg[SECOND].pipe_IR.opcode)
	{
		cout<<"\n Is branch Instruct: "<<(instr_names[pipe_reg[SECOND].pipe_IR.opcode]);

		noBranches = true;
		branchToLabel = "";

	case BNEZ:
		if(pipe_reg[SECOND].pipe_IR.src1 != 0)
		{
			branchToLabel = pipe_reg[SECOND].pipe_IR.label;
			noBranches = false;
		}
		break;

	case BEQZ:
		if(pipe_reg[SECOND].pipe_IR.src1 == 0)
		{
			branchToLabel = pipe_reg[SECOND].pipe_IR.label;
			noBranches = false;
		}
		break;

	case BLTZ:
		if(pipe_reg[SECOND].pipe_IR.src1 < 0)
		{
			branchToLabel = pipe_reg[SECOND].pipe_IR.label;
			noBranches = false;
		}
		break;

	case BGTZ:
		if(pipe_reg[SECOND].pipe_IR.src1 > 0)
		{
			branchToLabel = pipe_reg[SECOND].pipe_IR.label;
			noBranches = false;
		}
		break;

	case BLEZ:
		if(pipe_reg[SECOND].pipe_IR.src1 <= 0)
		{
			branchToLabel = pipe_reg[SECOND].pipe_IR.label;
			noBranches = false;
		}
		break;

	case BGEZ:
		if(pipe_reg[SECOND].pipe_IR.src1 >= 0)
		{
			branchToLabel = pipe_reg[SECOND].pipe_IR.label;
			noBranches = false;
		}
		break;

	default:
		noBranches = true;
	}

	if(pipe_reg[SECOND].pipe_IR.opcode ==  NOP)
	{
		specialP_Reg[MEM][IR] = NOP;
		specialP_Reg[MEM][ALU_OUTPUT] = 0;
		specialP_Reg[MEM][B] = UNDEFINED;
	}
	else{
		specialP_Reg[MEM][ALU_OUTPUT] = pipe_reg[SECOND].pipe_ALU_OUTPUT;
		specialP_Reg[MEM][B] =  specialP_Reg[EXE][B]; //For SW, B holds data
		specialP_Reg[MEM][IR] =  specialP_Reg[EXE][IR];
	}
	//LOAD PIPE3 WITH PIPE2
	std::memcpy(&pipe_reg[THIRD], &pipe_reg[SECOND], sizeof (pipeline_Registers) );

	if(clkIn == (EXE+1))
	{
		decode();
		fetch();
		++clkIn;
		cout<<"\n---return true from execute---clkIn: "<<clkIn<<"\n";
	}

}

void sim_pipe::memory()
{
	cout<<"\n In memory clkIn: "<<clkIn;
	cout<<"\n memoryStall: "<<memoryStall<<"stallMem: "<<stallMem<<"\n";

	cout<<"\n memory input instruct: "<<(instr_names[pipe_reg[THIRD].pipe_IR.opcode]);
	cout<<"\n memory input 2.dest:   "<<pipe_reg[THIRD].pipe_IR.dest;
	cout<<"\n memory input 3.src1:   "<<pipe_reg[THIRD].pipe_IR.src1;
	cout<<"\n memory input 4.src2:   "<<pipe_reg[THIRD].pipe_IR.src2;
	cout<<"\n memory input 5.imm:    "<<pipe_reg[THIRD].pipe_IR.immediate;
	cout<<"\n memory input 6.label:  "<<pipe_reg[THIRD].pipe_IR.label<<"\n";

	cout<<"\n specialP_Reg[MEM][IR]:  "<<specialP_Reg[MEM][IR]<<"\n";
	cout<<"\n give specialP_Reg[MEM][ALU_OUTPUT]: "<<specialP_Reg[MEM][ALU_OUTPUT]<<"\n";

	if(!data_memory_latency)
		memoryStall = false;

	if(stallMem < data_memory_latency)
	{
		if( ( pipe_reg[THIRD].pipe_IR.opcode == LW ) ||
			( pipe_reg[THIRD].pipe_IR.opcode == SW )  )
		{
			if(!stallMem)
			{
				cout<<"\n Memory latency required for inst: "<<(instr_names[pipe_reg[THIRD].pipe_IR.opcode]);
			}
			memoryStall = true;
			totalStalls += 1;
			stallMem += 1;
			memStallCompleted = false;
		}

	}else
		if((data_memory_latency) && (stallMem == data_memory_latency))
		{
			memoryStall = false;
			stallMem = 0;
			memStallCompleted = true;
		}

	if(memoryStall) return;

	if(pipe_reg[THIRD].pipe_IR.opcode ==  NOP)
		specialP_Reg[MEM][IR] = NOP;

	if( specialP_Reg[MEM][IR] == LW) //  LW R1, 4(R2)
	{
		pipe_reg[FORTH].pipe_LMD = data_memory[specialP_Reg[MEM][ALU_OUTPUT]]; // index, value

		//unsigned dataFromMem = *(data_memory + specialP_Reg[MEM][ALU_OUTPUT]);
		//unsigned char *address = static_cast<void const*>(&data_memory[specialP_Reg[MEM][ALU_OUTPUT]]);
		//write_memory(specialP_Reg[MEM][ALU_OUTPUT],0x34);

		unsigned da = char2int(data_memory + specialP_Reg[MEM][ALU_OUTPUT]);

		specialP_Reg[WB][LMD]=da;
	}
	else
	if( specialP_Reg[MEM][IR] == SW) //SW reg1, srcreg2:  SW R1, R3 or SW R1, 4(R2)
	{
		unsigned long dataMemAddr = 0;
		unsigned long data = 0;

		data = specialP_Reg[MEM][B];
		dataMemAddr = specialP_Reg[MEM][ALU_OUTPUT];

		//Store register value into data memory
		write_memory(dataMemAddr, data);
	}
	else
	{
		specialP_Reg[WB][ALU_OUTPUT]=specialP_Reg[MEM][ALU_OUTPUT];
	}

	specialP_Reg[WB][IR] = specialP_Reg[MEM][IR];

	//LOAD PIPE4 WITH PIPE3
	std::memcpy(&pipe_reg[FORTH], &pipe_reg[THIRD], sizeof (pipeline_Registers) );

	if(clkIn == (MEM+1))
	{
		execute();
		decode();
		fetch();
		++clkIn;
		cout<<"\n---return true from memory---clkIn: "<<clkIn<<"\n";
	}
}

void sim_pipe::writeBack()
{
	cout<<"\n WB clkIn: "<<clkIn<<"\n";
	cout<<"\n memoryStall: "<<memoryStall;

	cout<<"\n WB input instruct: "<<(instr_names[pipe_reg[FORTH].pipe_IR.opcode]);
	cout<<"\n WB input 2.dest:   "<<pipe_reg[FORTH].pipe_IR.dest;
	cout<<"\n WB input 3.src1:   "<<pipe_reg[FORTH].pipe_IR.src1;
	cout<<"\n WB input 4.src2:   "<<pipe_reg[FORTH].pipe_IR.src2;
	cout<<"\n WB input 5.imm:   "<<pipe_reg[FORTH].pipe_IR.immediate;
	cout<<"\n WB input 6.label: "<<pipe_reg[FORTH].pipe_IR.label<<"\n";

	if(pipe_reg[FORTH].pipe_IR.opcode ==  NOP)
		specialP_Reg[WB][IR] = NOP;

	if(     (specialP_Reg[WB][IR] == ADD)  ||
			(specialP_Reg[WB][IR] == ADDI) ||
			(specialP_Reg[WB][IR] == SUB)  ||
			(specialP_Reg[WB][IR] == SUBI) ||
			(specialP_Reg[WB][IR] == XOR)   )
	{
		set_gp_register(pipe_reg[FORTH].pipe_IR.dest, specialP_Reg[WB][ALU_OUTPUT]); // index, value
	}
	else
		if( specialP_Reg[WB][IR] == LW) //LW
	{
		set_gp_register(pipe_reg[FORTH].pipe_IR.dest, specialP_Reg[WB][LMD]); // index, value
	}

	if(clkIn >= (WB+1))
	{
		memory();
		execute();
		decode();
		fetch();

		if((specialP_Reg[WB][IR] == EOP) && (noBranches))
		{
			runAlways = false;
			cout<<"\n --- End Of Program Detected ---, clkIn: "<<clkIn<<"\n";
			return;
		}

		++clkIn;

		cout<<"\n--- write back cycle done---clkIn: "<<clkIn<<"\n";
	}
}

void sim_pipe::hazardHandler()
{
	cout<<"\n hazardHandler, clkIn: "<<clkIn;
	cout<<"\n hazardHandler, stalls: "<<stalls<<" totalStalls: "<<totalStalls;
	cout<<"\n hazardHandler, memoryStall: "<<memoryStall;
	cout<<"\n hazardHandler, stallMem: "<<stallMem;

	if(memoryStall)
		return;

	bool nopInst = false;
	cout<<"\n pipe_reg[FIRST].pipe_IR.instru :  "<<instr_names[pipe_reg[FIRST].pipe_IR.opcode];
	cout<<"\n pipe_reg[SECOND].pipe_IR.instru : "<<instr_names[pipe_reg[SECOND].pipe_IR.opcode];
	cout<<"\n pipe_reg[THIRD].pipe_IR.instru :  "<<instr_names[pipe_reg[THIRD].pipe_IR.opcode];
	cout<<"\n pipe_reg[FORTH].pipe_IR.instru :  "<<instr_names[pipe_reg[FORTH].pipe_IR.opcode];


	if( (pipe_reg[FIRST].pipe_IR.opcode == NOP)  ||
    	(pipe_reg[SECOND].pipe_IR.opcode == NOP) ||
		(pipe_reg[THIRD].pipe_IR.opcode == NOP)   )
	{
		nopInst = true;
	}

	if((!stalls) && (!nopInst))
	{
		if(specialP_Reg[ID][IR] == SW)
		{
			specialP_Reg[ID][B] = pipe_reg[FIRST].pipe_IR.src1;
			specialP_Reg[ID][A] = pipe_reg[FIRST].pipe_IR.src2;
		}else
		{
			specialP_Reg[ID][A] = pipe_reg[FIRST].pipe_IR.src1; //3
			specialP_Reg[ID][B] = pipe_reg[FIRST].pipe_IR.src2; //4
		}

		if(pipe_reg[FIRST].pipe_IR.opcode == SW)
		{
			if( ( specialP_Reg[ID][A] == pipe_reg[SECOND].pipe_IR.dest) ||
				( specialP_Reg[ID][B] == pipe_reg[SECOND].pipe_IR.dest)  )
			{
				cout<<"\n for instruct: "<<(instr_names[pipe_reg[FIRST].pipe_IR.opcode]);
				cout<<"\n with instruct: "<<(instr_names[pipe_reg[SECOND].pipe_IR.opcode]);

				stalls = 2;
				currentClk = clkIn;

			}
			else
			if( ( (pipe_reg[FORTH].pipe_IR.opcode != NOP) &&
				  (pipe_reg[FORTH].pipe_IR.opcode != SW)  &&
				  (pipe_reg[FORTH].pipe_IR.opcode != BNEZ)&&
				  (pipe_reg[FORTH].pipe_IR.opcode != BLTZ)  ) &&	(
				  ( specialP_Reg[ID][A] == pipe_reg[FORTH].pipe_IR.dest) ||
				  ( specialP_Reg[ID][B] == pipe_reg[FORTH].pipe_IR.dest)  ) )
			{
				cout<<"\n for instruct: "<<(instr_names[pipe_reg[FIRST].pipe_IR.opcode]);
				cout<<"\n with instruct: "<<(instr_names[pipe_reg[FORTH].pipe_IR.opcode]);

					stalls = 1;
					currentClk = clkIn;
			}
		}
		else
		if( (specialP_Reg[ID][A] == pipe_reg[SECOND].pipe_IR.dest) ||
			(specialP_Reg[ID][B] == pipe_reg[SECOND].pipe_IR.dest) )
		{
			if( 	(pipe_reg[SECOND].pipe_IR.opcode == ADD)  || //ADD
					(pipe_reg[SECOND].pipe_IR.opcode == ADDI) || //ADDI
					(pipe_reg[SECOND].pipe_IR.opcode == SUB)  || //SUB
					(pipe_reg[SECOND].pipe_IR.opcode == SUBI) || //SUBI
					(pipe_reg[SECOND].pipe_IR.opcode == XOR)  || //XOR
					(pipe_reg[SECOND].pipe_IR.opcode == LW)    ) //LW
			{
				cout<<"\n for instruct: "<<(instr_names[pipe_reg[FIRST].pipe_IR.opcode]);
				cout<<"\n with instruct: "<<(instr_names[pipe_reg[SECOND].pipe_IR.opcode]);

				stalls = 2;
				currentClk = clkIn;
			}
		}
		else
		if(( ( pipe_reg[THIRD].pipe_IR.opcode != SW)    &&
			 ( pipe_reg[FIRST].pipe_IR.opcode != BNEZ)  &&
	         ( pipe_reg[THIRD].pipe_IR.opcode != NOP)   &&
			 ( pipe_reg[THIRD].pipe_IR.opcode != BNEZ)   )  &&
			 ( ( specialP_Reg[ID][A] == pipe_reg[THIRD].pipe_IR.dest) ||
			   ( specialP_Reg[ID][B] == pipe_reg[THIRD].pipe_IR.dest) ) )
		{
			cout<<"\n for instruct: "<<(instr_names[pipe_reg[FIRST].pipe_IR.opcode]);
			cout<<"\n with instruct: "<<(instr_names[pipe_reg[THIRD].pipe_IR.opcode]);

			stalls = 1;
			currentClk = clkIn;
		}
		else
		if( ( ( pipe_reg[FORTH].pipe_IR.opcode != SW)    &&
			  ( pipe_reg[FORTH].pipe_IR.opcode != BNEZ)  &&
			  ( pipe_reg[FIRST].pipe_IR.opcode != BNEZ)  &&
			  ( pipe_reg[FORTH].pipe_IR.opcode != NOP)   &&
			  ( pipe_reg[FORTH].pipe_IR.opcode != BLTZ)   ) &&
			  ( ( specialP_Reg[ID][A] == pipe_reg[FORTH].pipe_IR.dest) ||
			    ( specialP_Reg[ID][B] == pipe_reg[FORTH].pipe_IR.dest)  ) )
		{
			cout<<"\n for instruct: "<<(instr_names[pipe_reg[FIRST].pipe_IR.opcode]);
			cout<<"\n with instruct: "<<(instr_names[pipe_reg[FORTH].pipe_IR.opcode]);

			stalls = 1;
			currentClk = clkIn;
			cout<<"\n stall 1 required";
		}
		else
			if(     (pipe_reg[FIRST].pipe_IR.opcode == BNEZ) ||
					(pipe_reg[FIRST].pipe_IR.opcode == BEQZ) ||
					(pipe_reg[FIRST].pipe_IR.opcode == BLTZ) ||
					(pipe_reg[FIRST].pipe_IR.opcode == BGTZ) ||
					(pipe_reg[FIRST].pipe_IR.opcode == BLEZ) ||
					(pipe_reg[FIRST].pipe_IR.opcode == BGEZ)  )
			{
				cout<<"\n Branching detected";
				cout<<"\n for instruct: "<<(instr_names[pipe_reg[FIRST].pipe_IR.opcode]);

				stalls = 2;
				currentClk = clkIn;
				branchStall = true;

			}

		if(stalls){

			cout<<"\n--------HAZARD PRESENT-- so far, totalstalls: "<<totalStalls;
			cout<<"\n more required stalls: "<<stalls<<"\n";

		}else
			memStallCompleted = false;

	}

	unsigned memS = 0;
	if(memStallCompleted && (!branchStall))
	{
		memS = 4;
	}

	if((stalls) && (clkIn == (currentClk+stalls+memS)) )
	{
		totalStalls += stalls;
		stalls = 0;

		if(branchStall)	branchStall = false;

		if(     (pipe_reg[FIRST].pipe_IR.opcode == BNEZ) ||
				(pipe_reg[FIRST].pipe_IR.opcode == BEQZ) ||
				(pipe_reg[FIRST].pipe_IR.opcode == BLTZ) ||
				(pipe_reg[FIRST].pipe_IR.opcode == BGTZ) ||
				(pipe_reg[FIRST].pipe_IR.opcode == BLEZ) ||
				(pipe_reg[FIRST].pipe_IR.opcode == BGEZ)  )
		{
			stalls = 2;
			currentClk = clkIn;
			branchStall = true;
		}

	}
}
