#ifndef SIM_PIPE_H_
#define SIM_PIPE_H_

#include <stdio.h>
#include <string>
#include <cstdlib>
#include <iostream>
#include <map>

using namespace std;

#define PROGRAM_SIZE 50

#define UNDEFINED 0xFFFFFFFF //used to initialize the registers
#define NUM_SP_REGISTERS 9
#define NUM_GP_REGISTERS 32
#define NUM_OPCODES 16
#define NUM_STAGES 5


typedef enum {PC, NPC, IR, A, B, IMM, COND, ALU_OUTPUT, LMD} sp_register_t;

typedef enum {LW, SW, ADD, ADDI, SUB, SUBI, XOR, BEQZ, BNEZ, BLTZ, BGTZ, BLEZ, BGEZ, JUMP, EOP, NOP} opcode_t;

typedef enum {IF, ID, EXE, MEM, WB} stage_t;

typedef enum {FIRST, SECOND, THIRD, FORTH} pipelineRegNum;

typedef struct{
	opcode_t opcode; //opcode
	unsigned src1; //first source register in the assembly instruction (for SW, register to be written to memory)
	unsigned src2; //second source register in the assembly instruction
	unsigned dest; //destination register
	unsigned immediate; //immediate field
	string label; //for conditional branches, label of the target instruction - used only for parsing/debugging purposes

	void reset()
	{
		opcode = NOP;
		src1 = 0x00000000;
		src2 = 0x00000000;
		dest = 0x00000000;
		immediate = 0x00000000;
		label = "";
	}

} instruction_t;

struct pipeline_Registers
{
	unsigned pipe_PC; //PC
	unsigned pipe_NPC; //NPC
	instruction_t pipe_IR; //IR
	unsigned pipe_COND;
	unsigned pipe_ALU_OUTPUT;
	unsigned pipe_LMD;

	void reset(void)
	{
		pipe_PC = 0x00000000;
		pipe_NPC = 0x00000000;
		pipe_IR.reset();
		pipe_COND = 0x00000000;
		pipe_ALU_OUTPUT = 0x00000000;
		pipe_LMD = 0x00000000;
	}

};


class sim_pipe{

	/* Add the data members required by your simulator's implementation here */

	//instruction memory
	instruction_t instr_memory[PROGRAM_SIZE];

	//base address in the instruction memory where the program is loaded
	unsigned instr_base_address;

	//data memory - should be initialize to all 0xFF
	unsigned char *data_memory;

	//memory size in bytes
	unsigned data_memory_size;

	//memory latency in clock cycles
	unsigned data_memory_latency;

protected:
	void fetch();
	void decode();
	void execute();
	void memory();
	void writeBack();
	void hazardHandler();

public:

	//instantiates the simulator with a data memory of given size (in bytes) and latency (in clock cycles)
	/* Note:
           - initialize the registers to UNDEFINED value
	   - initialize the data memory to all 0xFF values
	 */
	sim_pipe(unsigned data_mem_size, unsigned data_mem_latency);

	//de-allocates the simulator
	~sim_pipe();

	//loads the assembly program in file "filename" in instruction memory at the specified address
	void load_program(const char *filename, unsigned base_address=0x0);

	//runs the simulator for "cycles" clock cycles (run the program to completion if cycles=0)
	void run(unsigned cycles=0);

	//resets the state of the simulator
	/* Note:
	   - registers should be reset to UNDEFINED value
	   - data memory should be reset to all 0xFF values
	 */
	void reset();

	// returns value of the specified special purpose register for a given stage (at the "entrance" of that stage)
	// if that special purpose register is not used in that stage, returns UNDEFINED
	//
	// Examples (refer to page C-37 in the 5th edition textbook, A-32 in 4th edition of textbook)::
	// - get_sp_register(PC, IF) returns the value of PC
	// - get_sp_register(NPC, ID) returns the value of IF/ID.NPC
	// - get_sp_register(NPC, EX) returns the value of ID/EX.NPC
	// - get_sp_register(ALU_OUTPUT, MEM) returns the value of EX/MEM.ALU_OUTPUT
	// - get_sp_register(ALU_OUTPUT, WB) returns the value of MEM/WB.ALU_OUTPUT
	// - get_sp_register(LMD, ID) returns UNDEFINED
	/* Note: you are allowed to use a custom format for the IR register.
           Therefore, the test cases won't check the value of IR using this method.
	   You can add an extra method to retrieve the content of IR */
	unsigned get_sp_register(sp_register_t reg, stage_t stage);

	//returns value of the specified general purpose register
	int get_gp_register(unsigned reg);

	// set the value of the given general purpose register to "value"
	void set_gp_register(unsigned reg, int value);

	//returns the IPC
	float get_IPC();

	//returns the number of instructions fully executed
	unsigned get_instructions_executed();

	//returns the number of clock cycles
	unsigned get_clock_cycles();

	//returns the number of stalls added by processor
	unsigned get_stalls();

	//prints the content of the data memory within the specified address range
	void print_memory(unsigned start_address, unsigned end_address);

	// writes an integer value to data memory at the specified address (use little-endian format: https://en.wikipedia.org/wiki/Endianness)
	void write_memory(unsigned address, unsigned value);

	//prints the values of the registers
	void print_registers();

	unsigned generalP_Reg[NUM_GP_REGISTERS];
	unsigned specialP_Reg[NUM_STAGES][NUM_SP_REGISTERS];
	pipeline_Registers pipe_reg[NUM_STAGES-1];

	unsigned long clkIn;
	bool runAlways;

	unsigned long inst_count;
	unsigned long totalInstCount;

	/* -- Member variables to handle hazards -- */
	unsigned stalls;
	unsigned totalStalls;
	unsigned currentClk;

	std::string branchToLabel;
	bool noBranches; // Indicates whether any branching has to be done
	bool branchStall; // Indicates is stalling for branch instruction going on

	bool memoryStall; // Indicates is stalling for memory-operative instruction going on
	bool memStallCompleted; // Indicates memory-stage stalling is done
	unsigned stallMem; // Counter for memory-stage stalling to serve memory latency

	unsigned branchingCount;


	std::map< std::string, unsigned> labelPCMap;

};


#endif /*SIM_PIPE_H_*/
