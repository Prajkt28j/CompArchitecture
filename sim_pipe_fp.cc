#include "sim_pipe_fp.h"
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <iomanip>
#include <map>

//NOTE: structural hazards on MEM/WB stage not handled
//====================================================

//#define DEBUG
//#define DEBUG_MEMORY

using namespace std;

//used for debugging purposes
static const char *reg_names[NUM_SP_REGISTERS] = {"PC", "NPC", "IR", "A", "B", "IMM", "COND", "ALU_OUTPUT", "LMD"};
static const char *stage_names[NUM_STAGES] = {"IF", "ID", "EX", "MEM", "WB"};
static const char *instr_names[NUM_OPCODES] = {"LW", "SW", "ADD", "ADDI", "SUB", "SUBI", "XOR", "BEQZ", "BNEZ", "BLTZ", "BGTZ", "BLEZ", "BGEZ", "JUMP", "EOP", "NOP", "LWS", "SWS", "ADDS", "SUBS", "MULTS", "DIVS"};
static const char *unit_names[4]={"INTEGER", "ADDER", "MULTIPLIER", "DIVIDER"};

/* =============================================================

   HELPER FUNCTIONS

   ============================================================= */

/* convert a float into an unsigned */
inline unsigned float2unsigned(float value){
	unsigned result;
	memcpy(&result, &value, sizeof value);
	return result;
}

/* convert an unsigned into a float */
inline float unsigned2float(unsigned value){
	float result;
	memcpy(&result, &value, sizeof value);
	return result;
}

/* convert integer into array of unsigned char - little indian */
inline void unsigned2char(unsigned value, unsigned char *buffer){
	buffer[0] = value & 0xFF;
	buffer[1] = (value >> 8) & 0xFF;
	buffer[2] = (value >> 16) & 0xFF;
	buffer[3] = (value >> 24) & 0xFF;
}

/* convert array of char into integer - little indian */
inline unsigned char2unsigned(unsigned char *buffer){
	return buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24);
}

/* the following functions return the kind of the considered opcode */

bool is_branch(opcode_t opcode){
	return (opcode == BEQZ || opcode == BNEZ || opcode == BLTZ || opcode == BLEZ || opcode == BGTZ || opcode == BGEZ || opcode == JUMP);
}

bool is_memory(opcode_t opcode){
	return (opcode == LW || opcode == SW || opcode == LWS || opcode == SWS);
}

bool is_int_r(opcode_t opcode){
	return (opcode == ADD || opcode == SUB || opcode == XOR);
}

bool is_int_imm(opcode_t opcode){
	return (opcode == ADDI || opcode == SUBI);
}

bool is_int_alu(opcode_t opcode){
	return (is_int_r(opcode) || is_int_imm(opcode));
}

bool is_fp_alu(opcode_t opcode){
	return (opcode == ADDS || opcode == SUBS || opcode == MULTS || opcode == DIVS);
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
	case LWS:
	case SWS:
		return(a + imm);
	case BEQZ:
	case BNEZ:
	case BGTZ:
	case BGEZ:
	case BLTZ:
	case BLEZ:
	case JUMP:
		return(npc+imm);
	case ADDS:
		return(float2unsigned(unsigned2float(a)+unsigned2float(b)));
		break;
	case SUBS:
		return(float2unsigned(unsigned2float(a)-unsigned2float(b)));
		break;
	case MULTS:
		return(float2unsigned(unsigned2float(a)*unsigned2float(b)));
		break;
	case DIVS:
		return(float2unsigned(unsigned2float(a)/unsigned2float(b)));
		break;
	default:
		return (-1);
	}
}

/* =============================================================

   CODE PROVIDED - NO NEED TO MODIFY FUNCTIONS BELOW

   ============================================================= */

/* ============== primitives to allocate/free the simulator ================== */

sim_pipe_fp::sim_pipe_fp(unsigned mem_size, unsigned mem_latency){
	data_memory_size = mem_size;
	data_memory_latency = mem_latency;
	data_memory = new unsigned char[data_memory_size];
	num_units = 0;
	reset();
}

sim_pipe_fp::~sim_pipe_fp(){
	delete [] data_memory;
}

/* =============   primitives to print out the content of the memory & registers and for writing to memory ============== */ 

void sim_pipe_fp::print_memory(unsigned start_address, unsigned end_address){
	cout << "data_memory[0x" << hex << setw(8) << setfill('0') << start_address << ":0x" << hex << setw(8) << setfill('0') <<  end_address << "]" << endl;
	for (unsigned i=start_address; i<end_address; i++){
		if (i%4 == 0) cout << "0x" << hex << setw(8) << setfill('0') << i << ": "; 
		cout << hex << setw(2) << setfill('0') << int(data_memory[i]) << " ";
		if (i%4 == 3){
#ifdef DEBUG_MEMORY 
			unsigned u = char2unsigned(&data_memory[i-3]);
			cout << " - unsigned=" << u << " - float=" << unsigned2float(u);
#endif
			cout << endl;
		}
	} 
}

void sim_pipe_fp::write_memory(unsigned address, unsigned value){
	unsigned2char(value,data_memory+address);
}


void sim_pipe_fp::print_registers(){
	cout << "Special purpose registers:" << endl;
	unsigned i, s;
	for (s=0; s<NUM_STAGES; s++){
		cout << "Stage: " << stage_names[s] << endl;
		for (i=0; i< NUM_SP_REGISTERS; i++)
			if ((sp_register_t)i != IR && (sp_register_t)i != COND && get_sp_register((sp_register_t)i, (stage_t)s)!=UNDEFINED) cout << reg_names[i] << " = " << dec <<  get_sp_register((sp_register_t)i, (stage_t)s) << hex << " / 0x" << get_sp_register((sp_register_t)i, (stage_t)s) << endl;
	}
	cout << "General purpose registers:" << endl;
	for (i=0; i< NUM_GP_REGISTERS; i++)
		if (get_int_register(i)!=(int)UNDEFINED) cout << "R" << dec << i << " = " << get_int_register(i) << hex << " / 0x" << get_int_register(i) << endl;
	for (i=0; i< NUM_GP_REGISTERS; i++)
		if (get_fp_register(i)!=UNDEFINED) cout << "F" << dec << i << " = " << get_fp_register(i) << hex << " / 0x" << float2unsigned(get_fp_register(i)) << endl;
}


/* =============   primitives related to the functional units ============== */ 

/* initializes an execution unit */ 
void sim_pipe_fp::init_exec_unit(exe_unit_t exec_unit, unsigned latency, unsigned instances){
	for (unsigned i=0; i<instances; i++){
		exec_units[num_units].type = exec_unit;
		exec_units[num_units].latency = latency;
		exec_units[num_units].busy = 0;
		exec_units[num_units].instruction.opcode = NOP;
		num_units++;
	}
}

/* returns a free unit for that particular operation or UNDEFINED if no unit is currently available */
unsigned sim_pipe_fp::get_free_unit(opcode_t opcode){
	if (num_units == 0){
		cout << "ERROR:: simulator does not have any execution units!\n";
		exit(-1);
	}
	for (unsigned u=0; u<num_units; u++){
		switch(opcode){
		//Integer unit
		case LW:
		case SW:
		case ADD:
		case ADDI:
		case SUB:
		case SUBI:
		case XOR:
		case BEQZ:
		case BNEZ:
		case BLTZ:
		case BGTZ:
		case BLEZ:
		case BGEZ:
		case JUMP:
		case LWS:
		case SWS:
			if (exec_units[u].type==INTEGER && exec_units[u].busy==0) return u;
			break;
			// FP adder
		case ADDS:
		case SUBS:
			if (exec_units[u].type==ADDER && exec_units[u].busy==0) return u;
			break;
			// Multiplier
		case MULTS:
			if (exec_units[u].type==MULTIPLIER && exec_units[u].busy==0) return u;
			break;
			// Divider
		case DIVS:
			if (exec_units[u].type==DIVIDER && exec_units[u].busy==0) return u;
			break;
		default:
			cout << "ERROR:: operations not requiring exec unit!\n";
			exit(-1);
		}
	}
	return UNDEFINED;
}

/* decrease the amount of clock cycles during which the functional unit will be busy - to be called at each clock cycle  */
void sim_pipe_fp::decrement_units_busy_time(){
	for (unsigned u=0; u<num_units; u++){
		if (exec_units[u].busy > 0) exec_units[u].busy --;
	}
}


/* prints out the status of the functional units */
void sim_pipe_fp::debug_units(){
	for (unsigned u=0; u<num_units; u++){
		cout << " -- unit " << unit_names[exec_units[u].type] << " latency=" << exec_units[u].latency << " busy=" << exec_units[u].busy <<
				" instruction=" << instr_names[exec_units[u].instruction.opcode] << endl;
	}
}

/* ========= end primitives related to functional units ===============*/


/* ========================parser ==================================== */

void sim_pipe_fp::load_program(const char *filename, unsigned base_address){

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
		case ADDS:
		case SUBS:
		case MULTS:
		case DIVS:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			par3 = strtok (NULL, " \t");
			instr_memory[instruction_nr].dest = atoi(strtok(par1, "RF"));
			instr_memory[instruction_nr].src1 = atoi(strtok(par2, "RF"));
			instr_memory[instruction_nr].src2 = atoi(strtok(par3, "RF"));
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
		case LWS:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			instr_memory[instruction_nr].dest = atoi(strtok(par1, "RF"));
			instr_memory[instruction_nr].immediate = strtoul(strtok(par2, "()"), NULL, 0);
			instr_memory[instruction_nr].src1 = atoi(strtok(NULL, "R"));
			break;
		case SW:
		case SWS:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			instr_memory[instruction_nr].src1 = atoi(strtok(par1, "RF"));
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

}

/* =============================================================

   CODE TO BE COMPLETED

   ============================================================= */

/* simulator */
void sim_pipe_fp::run(unsigned cycles){

	if(cycles == 0) fp_runAlways = 1;

	cout<<"\n## START of run, fp_clkIn: "<<fp_clkIn<<" cycles: "<<cycles;

	run:
	do
	{

		switch(fp_clkIn)
		{

		case (IF+1):
					fp_fetch();
		break;

		case (ID+1):
					fp_decode();
		break;

		case (EXE+1):
					fp_execute();
		break;

		case (MEM+1):
					fp_memory();
		break;

		case (WB+1):
					fp_writeBack();
		break;

		default:
			if(fp_clkIn > (WB+1)) fp_writeBack();
		}

	}while(fp_runAlways);//end of while

	if(fp_runAlways){ goto run;}

	return;

}

//reset the state of the sim_pipe_fpulator
void sim_pipe_fp::reset(){
	// init data memory
	for (unsigned i=0; i<data_memory_size; i++) data_memory[i]=0xFF;

	// init instruction memory
	for (int i=0; i<PROGRAM_SIZE;i++){
		instr_memory[i].opcode=(opcode_t)NOP;
		instr_memory[i].src1=UNDEFINED;
		instr_memory[i].src2=UNDEFINED;
		instr_memory[i].dest=UNDEFINED;
		instr_memory[i].immediate=UNDEFINED;
	}

	// Initialize member variables
	std::fill_n(data_memory, data_memory_size, 0xFF);

	std::fill_n(generalP_IntReg, NUM_SP_INT_REGISTERS, UNDEFINED);
	std::fill_n(generalP_FPReg, NUM_GP_REGISTERS, UNDEFINED);

	for(int k = IF; k <= WB ; k++)
		std::fill_n(specialP_Reg[k], NUM_SP_REGISTERS, UNDEFINED);

	fp_pipe_reg[FIRST].reset();
	fp_pipe_reg[SECOND].reset();
	fp_pipe_reg[THIRD].reset();
	fp_pipe_reg[FORTH].reset();

	fp_clkIn = 1;
	fp_inst_count = 0;
	fp_runAlways = 0;

	fp_totalInstCount = 0;

	fp_stalls = 0;
	fp_totalStalls = 0;
	fp_stallMem = 0;
	fp_currentClk = 0;
	fp_branchToLabel = "";

	fp_noBranches = true;
	fp_branchStall = false;
	fp_memoryStall = false;
	fp_memStallCompleted = false;

}

//return value of special purpose register
unsigned sim_pipe_fp::get_sp_register(sp_register_t reg, stage_t s){
	if( (reg >= 0 ) && (reg < NUM_SP_REGISTERS) && (s>=0) && (s<5))
	{
		return specialP_Reg[s][reg];
	}

	return 0;
}

int sim_pipe_fp::get_int_register(unsigned reg){
	if( (reg >= 0 ) && (reg < NUM_GP_REGISTERS))
	{
		return generalP_IntReg[reg];
	}

	return 0;
}

void sim_pipe_fp::set_int_register(unsigned reg, int value){
	if( (reg >= 0 ) && (reg < NUM_GP_REGISTERS))
	{
		generalP_IntReg[reg] = value;
	}
}

float sim_pipe_fp::get_fp_register(unsigned reg){
	if( (reg >= 0 ) && (reg < NUM_GP_REGISTERS))
	{
		return generalP_FPReg[reg];
	}
	return 0.0;
}

void sim_pipe_fp::set_fp_register(unsigned reg, float value){
	if( (reg >= 0 ) && (reg < NUM_GP_REGISTERS))
	{
		generalP_FPReg[reg] = value;
	}
}


float sim_pipe_fp::get_IPC(){
	float IPC = float(fp_totalInstCount)/float(fp_clkIn);
	return IPC;
}

unsigned sim_pipe_fp::get_instructions_executed(){
	return fp_totalInstCount;
}

unsigned sim_pipe_fp::get_clock_cycles(){
	return fp_totalStalls;
}

unsigned sim_pipe_fp::get_stalls(){
	return fp_clkIn;
}


void sim_pipe_fp::fp_fetch()
{
	cout<<"\nIn fetch fp_clkIn: "<<fp_clkIn<<"\t current instruction is: "<<fp_inst_count+1;

	if(fp_memoryStall) return;

	if(fp_stalls)
	{
		if(fp_branchStall) fp_pipe_reg[FIRST].reset();
		return;
	}

	cout<<"\n Fetch input: 1.opcode: "<<instr_memory[fp_inst_count].opcode;
	cout<<"\n Fetch input: instruct: "<<(instr_names[instr_memory[fp_inst_count].opcode]);
	cout<<"\n Fetch input: 2.dest:   "<<instr_memory[fp_inst_count].dest;
	cout<<"\n Fetch input: 3.src1:   "<<instr_memory[fp_inst_count].src1;
	cout<<"\n Fetch input: 4.src2:   "<<instr_memory[fp_inst_count].src2;
	cout<<"\n Fetch input: 5.imm:    "<<instr_memory[fp_inst_count].immediate;
	cout<<"\n Fetch input: 6.label:  "<<instr_memory[fp_inst_count].label<<"\n";

	std::string emptyStr = "";
	if(fp_branchToLabel != emptyStr)
	{
		unsigned jumpToInst = 0; //(fp_labelPCMap.find(fp_branchToLabel))->second;

		fp_inst_count = jumpToInst;
		fp_branchToLabel = emptyStr;
	}

	//update IR register of pipeline reg first
	std::memcpy(&fp_pipe_reg[FIRST].pipe_IR, &instr_memory[fp_inst_count], sizeof (instruction_t) );
	specialP_Reg[IF][IR] = fp_pipe_reg[FIRST].pipe_IR.opcode;
	specialP_Reg[ID][IR] =  specialP_Reg[IF][IR];

	if(specialP_Reg[IF][IR] != EOP)
	{
		//1.update PC, NPC
		if(fp_clkIn == 1){//FIRST instr
			specialP_Reg[IF][PC] = instr_base_address+(4*fp_inst_count) + 4;
			specialP_Reg[ID][NPC] = specialP_Reg[IF][PC];
		}
		else if(fp_clkIn > 1){
			specialP_Reg[ID][NPC] = instr_base_address+(4*fp_inst_count) + 4; //specialP_Reg[IF][PC] + 4;
			specialP_Reg[IF][PC] = specialP_Reg[ID][NPC];
		}

		++fp_inst_count;
		++fp_totalInstCount;
	}

	if(fp_clkIn == (IF+1))
	{
		++fp_clkIn;
		cout<<"\n FETCH: Incremented fp_clkIn: "<<fp_clkIn<<" & fp_inst_count: "<<fp_inst_count<<"\n";
	}

}

void sim_pipe_fp::fp_decode()
{
	cout<<"\n In DECODE fp_clkIn: "<<fp_clkIn;
	cout<<"\n In DECODE fp_stalls: "<<fp_stalls;
	cout<<"\n fp_memoryStall: "<<fp_memoryStall<<"fp_stallMem: "<<fp_stallMem<<"\n";

	//cout<<"\n DEC input: 1.opcode: "<<fp_pipe_reg[FIRST].pipe_IR.opcode;
	cout<<"\n DEC input: instruct: "<<(instr_names[fp_pipe_reg[FIRST].pipe_IR.opcode]);
	cout<<"\n DEC input: 2.dest:   "<<fp_pipe_reg[FIRST].pipe_IR.dest;
	cout<<"\n DEC input: 3.src1:   "<<fp_pipe_reg[FIRST].pipe_IR.src1;
	cout<<"\n DEC input: 4.src2:   "<<fp_pipe_reg[FIRST].pipe_IR.src2;
	cout<<"\n DEC input: 5.imm:    "<<fp_pipe_reg[FIRST].pipe_IR.immediate;
	cout<<"\n DEC input: 6.label:  "<<fp_pipe_reg[FIRST].pipe_IR.label<<"\n";

	if(fp_memoryStall)	return; //140CYCLE, 75 STALLS

	//Find data-hazards & calculate fp_stalls
	//fp_hazardHandler();

	if(fp_memoryStall){
		return;
	}

	if(fp_stalls && (!fp_branchStall))
	{
		cout<<"\n hazard present fp_stalls: "<<fp_stalls;
		cout<<"\n fp_clkIn: "<<fp_clkIn<<"\n";
		fp_pipe_reg[SECOND].reset();
		return;
	}
	else if( fp_pipe_reg[FIRST].pipe_IR.opcode != NOP )
	{
		//over-write index values with actual values of respective registers
		fp_pipe_reg[FIRST].pipe_IR.src1 = get_int_register(fp_pipe_reg[FIRST].pipe_IR.src1);
		fp_pipe_reg[FIRST].pipe_IR.src2 = get_int_register(fp_pipe_reg[FIRST].pipe_IR.src2);
	}

	//2.update NPC
	specialP_Reg[EXE][NPC] = specialP_Reg[ID][NPC];
	specialP_Reg[EXE][IMM] = fp_pipe_reg[FIRST].pipe_IR.immediate;

	if( ( specialP_Reg[ID][IR] != SW) &&
			( (fp_pipe_reg[FIRST].pipe_IR.opcode != NOP) ||
					(fp_pipe_reg[FIRST].pipe_IR.opcode != EOP)  ) )
	{
		specialP_Reg[EXE][A]=fp_pipe_reg[FIRST].pipe_IR.src1; //3

		if(specialP_Reg[ID][IR] != LW ) //LW doesnt have src2
			specialP_Reg[EXE][B]=fp_pipe_reg[FIRST].pipe_IR.src2; //4
	}
	else
		if( specialP_Reg[ID][IR] == SW )
		{
			specialP_Reg[EXE][B] = fp_pipe_reg[FIRST].pipe_IR.src1;
			specialP_Reg[EXE][A] = fp_pipe_reg[FIRST].pipe_IR.src2;
			fp_pipe_reg[FIRST].pipe_IR.src1 = specialP_Reg[EXE][A];
			fp_pipe_reg[FIRST].pipe_IR.src2 = specialP_Reg[EXE][B];
		}

	specialP_Reg[EXE][IR] = specialP_Reg[ID][IR];

	//LOAD PIPE2 WITH PIPE1
	std::memcpy(&fp_pipe_reg[SECOND], &fp_pipe_reg[FIRST], sizeof (pipeline_Registers) );

	if(fp_clkIn == (ID+1))
	{
		fp_fetch();
		++fp_clkIn;
		cout<<"\n---return true from decode---fp_clkIn: "<<fp_clkIn<<"\n";
	}
	cout<<"\n---Done decode---fp_clkIn: "<<fp_clkIn<<"\n";

}

void sim_pipe_fp::fp_execute()
{
	cout<<"\nIn exe fp_clkIn: "<<fp_clkIn<<"\n";
	//	cout<<"\n fp_branchToLabel: "<<fp_branchToLabel;
	cout<<"\n fp_memoryStall: "<<fp_memoryStall<<"fp_stallMem: "<<fp_stallMem<<"\n";

	cout<<"\n execute input instruct: "<<(instr_names[fp_pipe_reg[SECOND].pipe_IR.opcode]);
	//cout<<"\n execute input 1.opcode: "<<fp_pipe_reg[SECOND].pipe_IR.opcode;
	cout<<"\n execute input 2.dest:   "<<fp_pipe_reg[SECOND].pipe_IR.dest;
	cout<<"\n execute input 3.src1:   "<<fp_pipe_reg[SECOND].pipe_IR.src1;
	cout<<"\n execute input 4.src2:   "<<fp_pipe_reg[SECOND].pipe_IR.src2;
	cout<<"\n execute input 5.imm:    "<<fp_pipe_reg[SECOND].pipe_IR.immediate;
	cout<<"\n execute input 6.label:  "<<fp_pipe_reg[SECOND].pipe_IR.label<<"\n";

	if(fp_memoryStall) return;

	//exe: call alu()
	fp_pipe_reg[SECOND].pipe_ALU_OUTPUT = alu(fp_pipe_reg[SECOND].pipe_IR.opcode, fp_pipe_reg[SECOND].pipe_IR.src1, fp_pipe_reg[SECOND].pipe_IR.src2, fp_pipe_reg[SECOND].pipe_IR.immediate, fp_pipe_reg[SECOND].pipe_NPC);

	//	if(fp_pipe_reg[SECOND].pipe_IR.opcode == NOP)
	//		fp_pipe_reg[SECOND].pipe_ALU_OUTPUT = 0;

	switch(fp_pipe_reg[SECOND].pipe_IR.opcode)
	{
	cout<<"\n Is branch Instruct: "<<(instr_names[fp_pipe_reg[SECOND].pipe_IR.opcode]);

	fp_noBranches = true;
	fp_branchToLabel = "";

	case BNEZ:
		if(fp_pipe_reg[SECOND].pipe_IR.src1 != 0)
		{
			fp_branchToLabel = fp_pipe_reg[SECOND].pipe_IR.label;
			fp_noBranches = false;
		}
		break;

	case BEQZ:
		if(fp_pipe_reg[SECOND].pipe_IR.src1 == 0)
		{
			fp_branchToLabel = fp_pipe_reg[SECOND].pipe_IR.label;
			fp_noBranches = false;
		}
		break;

	case BLTZ:
		if(fp_pipe_reg[SECOND].pipe_IR.src1 < 0)
		{
			fp_branchToLabel = fp_pipe_reg[SECOND].pipe_IR.label;
			fp_noBranches = false;
		}
		break;

	case BGTZ:
		if(fp_pipe_reg[SECOND].pipe_IR.src1 > 0)
		{
			fp_branchToLabel = fp_pipe_reg[SECOND].pipe_IR.label;
			fp_noBranches = false;
		}
		break;

	case BLEZ:
		if(fp_pipe_reg[SECOND].pipe_IR.src1 <= 0)
		{
			fp_branchToLabel = fp_pipe_reg[SECOND].pipe_IR.label;
			fp_noBranches = false;
		}
		break;

	case BGEZ:
		if(fp_pipe_reg[SECOND].pipe_IR.src1 >= 0)
		{
			fp_branchToLabel = fp_pipe_reg[SECOND].pipe_IR.label;
			fp_noBranches = false;
		}
		break;

	default:
		fp_noBranches = true;
	}

	if(fp_pipe_reg[SECOND].pipe_IR.opcode ==  NOP)
	{
		specialP_Reg[MEM][IR] = NOP;
		specialP_Reg[MEM][ALU_OUTPUT] = 0;
		specialP_Reg[MEM][B] = UNDEFINED;
	}
	else{
		specialP_Reg[MEM][ALU_OUTPUT] = fp_pipe_reg[SECOND].pipe_ALU_OUTPUT;
		specialP_Reg[MEM][B] =  specialP_Reg[EXE][B]; //For SW, B holds data
		specialP_Reg[MEM][IR] =  specialP_Reg[EXE][IR];
	}
	//LOAD PIPE3 WITH PIPE2
	std::memcpy(&fp_pipe_reg[THIRD], &fp_pipe_reg[SECOND], sizeof (pipeline_Registers) );

	if(fp_clkIn == (EXE+1))
	{
		fp_decode();
		fp_fetch();
		++fp_clkIn;
		cout<<"\n---return true from execute---fp_clkIn: "<<fp_clkIn<<"\n";
	}

}

void sim_pipe_fp::fp_memory()
{
	cout<<"\n In memory fp_clkIn: "<<fp_clkIn;
	cout<<"\n fp_memoryStall: "<<fp_memoryStall<<"fp_stallMem: "<<fp_stallMem<<"\n";

	cout<<"\n memory input instruct: "<<(instr_names[fp_pipe_reg[THIRD].pipe_IR.opcode]);
	cout<<"\n memory input 2.dest:   "<<fp_pipe_reg[THIRD].pipe_IR.dest;
	cout<<"\n memory input 3.src1:   "<<fp_pipe_reg[THIRD].pipe_IR.src1;
	cout<<"\n memory input 4.src2:   "<<fp_pipe_reg[THIRD].pipe_IR.src2;
	cout<<"\n memory input 5.imm:    "<<fp_pipe_reg[THIRD].pipe_IR.immediate;
	cout<<"\n memory input 6.label:  "<<fp_pipe_reg[THIRD].pipe_IR.label<<"\n";

	cout<<"\n specialP_Reg[MEM][IR]:  "<<specialP_Reg[MEM][IR]<<"\n";
	cout<<"\n give specialP_Reg[MEM][ALU_OUTPUT]: "<<specialP_Reg[MEM][ALU_OUTPUT]<<"\n";

	if(!data_memory_latency)
		fp_memoryStall = false;

	if(fp_stallMem < data_memory_latency)
	{
		if( ( fp_pipe_reg[THIRD].pipe_IR.opcode == LW ) ||
				( fp_pipe_reg[THIRD].pipe_IR.opcode == SW )  )
		{
			if(!fp_stallMem)
			{
				cout<<"\n Memory latency required for inst: "<<(instr_names[fp_pipe_reg[THIRD].pipe_IR.opcode]);
			}
			fp_memoryStall = true;
			fp_totalStalls += 1;
			fp_stallMem += 1;
			fp_memStallCompleted = false;
		}

	}else
		if((data_memory_latency) && (fp_stallMem == data_memory_latency))
		{
			fp_memoryStall = false;
			fp_stallMem = 0;
			fp_memStallCompleted = true;
		}

	if(fp_memoryStall) return;

	if(fp_pipe_reg[THIRD].pipe_IR.opcode ==  NOP)
		specialP_Reg[MEM][IR] = NOP;

	if( specialP_Reg[MEM][IR] == LW) //  LW R1, 4(R2)
	{
		fp_pipe_reg[FORTH].pipe_LMD = data_memory[specialP_Reg[MEM][ALU_OUTPUT]]; // index, value

		unsigned dataFromMem = *(data_memory + specialP_Reg[MEM][ALU_OUTPUT]);
		//unsigned char *address = static_cast<void const*>(&data_memory[specialP_Reg[MEM][ALU_OUTPUT]]);
		//write_memory(specialP_Reg[MEM][ALU_OUTPUT],0x34);

		//unsigned da = char2int(data_memory + specialP_Reg[MEM][ALU_OUTPUT]);

		specialP_Reg[WB][LMD] = dataFromMem;
	}
	else
		if( specialP_Reg[MEM][IR] == SW) //SW reg1, srcreg2:  SW R1, R3 or SW R1, 4(R2)
		{
			unsigned long dataMemAddr = 0;
			unsigned long data = 0;

			data = specialP_Reg[MEM][B];

			dataMemAddr = specialP_Reg[MEM][ALU_OUTPUT];

			cout<<"\n SW: dataMemAddr: "<<dataMemAddr<<"\t"<<"data: "<<data<<"\n";

			//Store register value into data memory
			write_memory(dataMemAddr, data);
		}
		else
		{
			specialP_Reg[WB][ALU_OUTPUT]=specialP_Reg[MEM][ALU_OUTPUT];
			//specialP_Reg[MEM][IMM]=fp_pipe_reg[SECOND].pipe_IR.immediate;
		}

	specialP_Reg[WB][IR] = specialP_Reg[MEM][IR];

	//LOAD PIPE4 WITH PIPE3
	std::memcpy(&fp_pipe_reg[FORTH], &fp_pipe_reg[THIRD], sizeof (pipeline_Registers) );

	if(fp_clkIn == (MEM+1))
	{
		fp_execute();
		fp_decode();
		fp_fetch();
		++fp_clkIn;
		cout<<"\n---return true from memory---fp_clkIn: "<<fp_clkIn<<"\n";
	}
}

void sim_pipe_fp::fp_writeBack()
{
	cout<<"\n WB fp_clkIn: "<<fp_clkIn<<"\n";
	cout<<"\n fp_memoryStall: "<<fp_memoryStall;

	cout<<"\n WB input instruct: "<<(instr_names[fp_pipe_reg[FORTH].pipe_IR.opcode]);
	//	cout<<"\n WB input 1.opcode: "<<fp_pipe_reg[FORTH].pipe_IR.opcode;
	cout<<"\n WB input 2.dest:   "<<fp_pipe_reg[FORTH].pipe_IR.dest;
	cout<<"\n WB input 3.src1:   "<<fp_pipe_reg[FORTH].pipe_IR.src1;
	cout<<"\n WB input 4.src2:   "<<fp_pipe_reg[FORTH].pipe_IR.src2;
	cout<<"\n WB input 5.imm:   "<<fp_pipe_reg[FORTH].pipe_IR.immediate;
	cout<<"\n WB input 6.label: "<<fp_pipe_reg[FORTH].pipe_IR.label<<"\n";

	if(fp_pipe_reg[FORTH].pipe_IR.opcode ==  NOP)
		specialP_Reg[WB][IR] = NOP;

	if(     (specialP_Reg[WB][IR] == ADD)  ||
			(specialP_Reg[WB][IR] == ADDI) ||
			(specialP_Reg[WB][IR] == SUB)  ||
			(specialP_Reg[WB][IR] == SUBI) ||
			(specialP_Reg[WB][IR] == XOR)   )
	{
		set_int_register(fp_pipe_reg[FORTH].pipe_IR.dest, specialP_Reg[WB][ALU_OUTPUT]); // index, value
	}
	else
		if( specialP_Reg[WB][IR] == LW) //LW
		{
			set_int_register(fp_pipe_reg[FORTH].pipe_IR.dest, specialP_Reg[WB][LMD]); // index, value
		}

	if(fp_clkIn >= (WB+1))
	{
		fp_memory();
		fp_execute();
		fp_decode();
		fp_fetch();

		if((specialP_Reg[WB][IR] == EOP) && (fp_noBranches))
		{
			fp_runAlways = false;
			cout<<"\n---EOP Detected---fp_clkIn: "<<fp_clkIn<<"\n";
			return;
		}

		/*	cout<<"\n WB--> fp_clkIn: "<<fp_clkIn;
		cout<<"\n fp_totalStalls: "<<fp_totalStalls;
		cout<<"\n fp_found:       "<<fp_found;
		cout<<"\n fp_resolved:    "<<fp_resolved;
		//	if(specialP_Reg[WB][IR] != 14)
		 */

		++fp_clkIn;

		cout<<"\n--- write back cycle done---fp_clkIn: "<<fp_clkIn<<"\n";
	}
}

void sim_pipe_fp::fp_hazardHandler()
{
	cout<<"\n hazardHandler, fp_clkIn: "<<fp_clkIn;
	cout<<"\n hazardHandler, fp_stalls: "<<fp_stalls<<" fp_totalStalls: "<<fp_totalStalls;
	cout<<"\n hazardHandler, fp_memoryStall: "<<fp_memoryStall;
	cout<<"\n hazardHandler, fp_stallMem: "<<fp_stallMem;

	if(fp_memoryStall)
		return;

	bool nopInst = false;
	cout<<"\n fp_pipe_reg[FIRST].pipe_IR.instru :  "<<instr_names[fp_pipe_reg[FIRST].pipe_IR.opcode];
	cout<<"\n fp_pipe_reg[SECOND].pipe_IR.instru : "<<instr_names[fp_pipe_reg[SECOND].pipe_IR.opcode];
	cout<<"\n fp_pipe_reg[THIRD].pipe_IR.instru :  "<<instr_names[fp_pipe_reg[THIRD].pipe_IR.opcode];
	cout<<"\n fp_pipe_reg[FORTH].pipe_IR.instru :  "<<instr_names[fp_pipe_reg[FORTH].pipe_IR.opcode];


	if( (fp_pipe_reg[FIRST].pipe_IR.opcode == NOP)  ||
			(fp_pipe_reg[SECOND].pipe_IR.opcode == NOP) ||
			(fp_pipe_reg[THIRD].pipe_IR.opcode == NOP)   )
	{
		nopInst = true;
	}

	if((!fp_stalls) && (!nopInst))
	{
		if(specialP_Reg[ID][IR] == SW)
		{
			specialP_Reg[ID][B] = fp_pipe_reg[FIRST].pipe_IR.src1;
			specialP_Reg[ID][A] = fp_pipe_reg[FIRST].pipe_IR.src2;
		}else
		{
			specialP_Reg[ID][A] = fp_pipe_reg[FIRST].pipe_IR.src1; //3
			specialP_Reg[ID][B] = fp_pipe_reg[FIRST].pipe_IR.src2; //4
		}

		if(fp_pipe_reg[FIRST].pipe_IR.opcode == SW)
		{
			if( ( specialP_Reg[ID][A] == fp_pipe_reg[SECOND].pipe_IR.dest) ||
					( specialP_Reg[ID][B] == fp_pipe_reg[SECOND].pipe_IR.dest)  )
			{
				cout<<"\n SW hazard detected ";
				cout<<"\n for instruct: "<<(instr_names[fp_pipe_reg[FIRST].pipe_IR.opcode]);
				cout<<"\n with instruct: "<<(instr_names[fp_pipe_reg[SECOND].pipe_IR.opcode]);

				fp_stalls = 2;
				cout<<"\n fp_stalls 2 req fp_clkIn: "<<fp_clkIn<<"fp_currentClk: "<<fp_currentClk;
				fp_found +=1;
				fp_currentClk = fp_clkIn;

			}
			else
				if( ( (fp_pipe_reg[FORTH].pipe_IR.opcode != NOP) &&
						(fp_pipe_reg[FORTH].pipe_IR.opcode != SW)  &&
						(fp_pipe_reg[FORTH].pipe_IR.opcode != BNEZ)&&
						(fp_pipe_reg[FORTH].pipe_IR.opcode != BLTZ)  ) &&	(
								( specialP_Reg[ID][A] == fp_pipe_reg[FORTH].pipe_IR.dest) ||
								( specialP_Reg[ID][B] == fp_pipe_reg[FORTH].pipe_IR.dest)  ) )
				{
					cout<<"\n SW hazard detected ";
					cout<<"\n for instruct: "<<(instr_names[fp_pipe_reg[FIRST].pipe_IR.opcode]);
					cout<<"\n with instruct: "<<(instr_names[fp_pipe_reg[FORTH].pipe_IR.opcode]);

					fp_stalls = 1;
					cout<<"\n fp_stalls 1 req fp_clkIn: "<<fp_clkIn<<"fp_currentClk: "<<fp_currentClk;
					fp_found += 1;
					fp_currentClk = fp_clkIn;
				}
		}
		else
			if( (specialP_Reg[ID][A] == fp_pipe_reg[SECOND].pipe_IR.dest) ||
					(specialP_Reg[ID][B] == fp_pipe_reg[SECOND].pipe_IR.dest) )
			{
				if( 	(fp_pipe_reg[SECOND].pipe_IR.opcode == ADD)  || //ADD
						(fp_pipe_reg[SECOND].pipe_IR.opcode == ADDI) || //ADDI
						(fp_pipe_reg[SECOND].pipe_IR.opcode == SUB)  || //SUB
						(fp_pipe_reg[SECOND].pipe_IR.opcode == SUBI) || //SUBI
						(fp_pipe_reg[SECOND].pipe_IR.opcode == XOR)  || //XOR
						(fp_pipe_reg[SECOND].pipe_IR.opcode == LW)    ) //LW
				{
					cout<<"\n RAW Hazard detected";
					cout<<"\n for instruct: "<<(instr_names[fp_pipe_reg[FIRST].pipe_IR.opcode]);
					cout<<"\n with instruct: "<<(instr_names[fp_pipe_reg[SECOND].pipe_IR.opcode]);

					fp_stalls = 2;
					fp_currentClk = fp_clkIn;
					fp_found +=1;
					cout<<"\n fp_pipe_reg[SECOND].pipe_IR.opcode: "<<fp_pipe_reg[SECOND].pipe_IR.opcode;
					cout<<"\n fp_stalls 2 req fp_clkIn: "<<fp_clkIn<<" fp_currentClk: "<<fp_currentClk;
				}
			}
			else
				if(( ( fp_pipe_reg[THIRD].pipe_IR.opcode != SW)    &&
						( fp_pipe_reg[FIRST].pipe_IR.opcode != BNEZ)  &&
						( fp_pipe_reg[THIRD].pipe_IR.opcode != NOP)   &&
						( fp_pipe_reg[THIRD].pipe_IR.opcode != BNEZ)   )  &&
						( ( specialP_Reg[ID][A] == fp_pipe_reg[THIRD].pipe_IR.dest) ||
								( specialP_Reg[ID][B] == fp_pipe_reg[THIRD].pipe_IR.dest) ) )
				{
					cout<<"\n Hazard detected, third";
					cout<<"\n for instruct: "<<(instr_names[fp_pipe_reg[FIRST].pipe_IR.opcode]);
					cout<<"\n with instruct: "<<(instr_names[fp_pipe_reg[THIRD].pipe_IR.opcode]);

					fp_stalls = 1;
					fp_currentClk = fp_clkIn;
					cout<<"\n stall 1 required";
				}
				else
					if( ( ( fp_pipe_reg[FORTH].pipe_IR.opcode != SW)    &&
							( fp_pipe_reg[FORTH].pipe_IR.opcode != BNEZ)  &&
							( fp_pipe_reg[FIRST].pipe_IR.opcode != BNEZ)  &&
							( fp_pipe_reg[FORTH].pipe_IR.opcode != NOP)   &&
							( fp_pipe_reg[FORTH].pipe_IR.opcode != BLTZ)   ) &&
							( ( specialP_Reg[ID][A] == fp_pipe_reg[FORTH].pipe_IR.dest) ||
									( specialP_Reg[ID][B] == fp_pipe_reg[FORTH].pipe_IR.dest)  ) )
					{
						cout<<"\n Hazard detected, forth";
						cout<<"\n for instruct: "<<(instr_names[fp_pipe_reg[FIRST].pipe_IR.opcode]);
						cout<<"\n with instruct: "<<(instr_names[fp_pipe_reg[FORTH].pipe_IR.opcode]);
						cout<<"\n fp_pipe_reg[FORTH].pipe_IR.dest: "<<fp_pipe_reg[FORTH].pipe_IR.dest<<"\n";

						fp_stalls = 1;
						fp_currentClk = fp_clkIn;
						fp_found +=1;
						cout<<"\n stall 1 required";
					}
					else
						if(     (fp_pipe_reg[FIRST].pipe_IR.opcode == BNEZ) ||
								(fp_pipe_reg[FIRST].pipe_IR.opcode == BEQZ) ||
								(fp_pipe_reg[FIRST].pipe_IR.opcode == BLTZ) ||
								(fp_pipe_reg[FIRST].pipe_IR.opcode == BGTZ) ||
								(fp_pipe_reg[FIRST].pipe_IR.opcode == BLEZ) ||
								(fp_pipe_reg[FIRST].pipe_IR.opcode == BGEZ)  )
						{
							cout<<"\n Branching detected";
							cout<<"\n for instruct: "<<(instr_names[fp_pipe_reg[FIRST].pipe_IR.opcode]);

							fp_stalls = 2;
							fp_currentClk = fp_clkIn;
							fp_found +=1;
							fp_branchStall = true;
							cout<<"\n fp_stalls 2 req fp_clkIn: "<<fp_clkIn<<" fp_currentClk: "<<fp_currentClk;

						}

		if(fp_stalls){
			cout<<"\n Hazard detected, required fp_stalls: "<<fp_stalls;
		}else
			fp_memStallCompleted = false;

	}

	unsigned memS = 0;
	if(fp_memStallCompleted && (!fp_branchStall))
	{
		memS = 4;
	}

	cout<<"\n fp_memStallCompleted: "<<fp_memStallCompleted;

	cout<<"\n fp_clkIn: "<<fp_clkIn;

	cout<<"\n fp_currentClk: "<<fp_currentClk;
	cout<<"\n fp_stalls: "<<fp_stalls;
	cout<<"\n memS: "<<memS;

	cout<<"\n fp_currentClk+fp_stalls+memS: "<<(fp_currentClk+fp_stalls+memS);

	if((fp_stalls) && (fp_clkIn == (fp_currentClk+fp_stalls+memS)) )
	{
		cout<<"\n fp_pipe_reg[SECOND].pipe_ALU_OUTPUT: "<<fp_pipe_reg[SECOND].pipe_ALU_OUTPUT;

		cout<<"\n------ fp_stalls done---------\n";

		fp_totalStalls += fp_stalls;
		fp_stalls = 0;
		fp_resolved += 1;

		if(fp_branchStall)	fp_branchStall = false;
	}
	/*
	if(fp_totalStalls == 32)
	{
		cout<<"\n 33stalls done----";
	}
	 */
	cout<<"\n check--> fp_clkIn: "<<fp_clkIn;
	cout<<"\n fp_totalStalls: "<<fp_totalStalls;
	/*	cout<<"\n fp_found:       "<<fp_found<<" fp_stalls: "<<fp_stalls;
	cout<<"\n fp_resolved:    "<<fp_resolved;

	cout<<"\n out fp_pipe_reg[FIRST].pipe_IR.src1: "<<fp_pipe_reg[FIRST].pipe_IR.src1;
	cout<<"\n out fp_pipe_reg[FIRST].pipe_IR.src2: "<<fp_pipe_reg[FIRST].pipe_IR.src2;
	 */
}
