#ifndef SIM_PIPE_FP_H_
#define SIM_PIPE_FP_H_

#include <stdio.h>
#include <string>
#include <map>

using namespace std;

#define PROGRAM_SIZE 50

#define UNDEFINED 0xFFFFFFFF
#define NUM_SP_REGISTERS 9
#define NUM_SP_INT_REGISTERS 15
#define NUM_GP_REGISTERS 32
#define NUM_OPCODES 22
#define NUM_STAGES 5
#define MAX_UNITS 10

typedef enum {PC, NPC, IR, A, B, IMM, COND, ALU_OUTPUT, LMD} sp_register_t;

typedef enum {LW, SW, ADD, ADDI, SUB, SUBI, XOR, BEQZ, BNEZ, BLTZ, BGTZ, BLEZ, BGEZ, JUMP, EOP, NOP, LWS, SWS, ADDS, SUBS, MULTS, DIVS} opcode_t;

typedef enum {IF, ID, EXE, MEM, WB} stage_t;

typedef enum {INTEGER, ADDER, MULTIPLIER, DIVIDER} exe_unit_t;

typedef enum {FIRST, SECOND, THIRD, FORTH} pipelineRegNum;

// instruction
typedef struct
{
	opcode_t opcode; //opcode
	unsigned src1; //first source register
	unsigned src2; //second source register
	unsigned dest; //destination register
	unsigned immediate; //immediate field;
	string label; //in case of branch, label of the target instruction;

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

// execution unit
typedef struct{
	exe_unit_t type;  // execution unit type
	unsigned latency; // execution unit latency
	unsigned busy;    // 0 if execution unit is free, otherwise number of clock cycles during
	// which the execution unit will be busy. It should be initialized
	// to the latency of the unit when the unit becomes busy, and decremented
	// at each clock cycle
	instruction_t instruction; // instruction using the functional unit
} unit_t;


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

static unsigned fp_clkIn = 0;
static pipeline_Registers fp_pipe_reg[NUM_STAGES-1];
static unsigned long fp_inst_count = 0;
static unsigned long fp_totalInstCount = 0;
static bool fp_runAlways = 0;
static unsigned fp_stalls = 0;
static unsigned fp_totalStalls = 0;
static unsigned fp_currentClk = 0;
static std::string fp_branchToLabel = "";
static bool fp_noBranches = true;
static unsigned fp_found = 0;
static unsigned fp_resolved = 0;
static unsigned fp_branchingCount = 0;
static bool fp_branchStall = false;
static bool fp_memoryStall = false;
static unsigned fp_stallMem = 0;
static bool fp_memStallCompleted = false;
static unsigned fp_totalMemStalls = 0;



class sim_pipe_fp{

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

	//execution units
	unit_t exec_units[MAX_UNITS];
	unsigned num_units;

public:

	//instantiates the simulator with a data memory of given size (in bytes) and latency (in clock cycles)
	/* Note: 
           - initialize the registers to UNDEFINED value 
	   - initialize the data memory to all 0xFF values
	 */
	sim_pipe_fp(unsigned data_mem_size, unsigned data_mem_latency);

	//de-allocates the simulator
	~sim_pipe_fp();

	// adds one or more execution units of a given type to the processor
	// - exec_unit: type of execution unit to be added
	// - latency: latency of the execution unit (in clock cycles)
	// - instances: number of execution units of this type to be added
	void init_exec_unit(exe_unit_t exec_unit, unsigned latency, unsigned instances=1);

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

	//returns value of the specified integer general purpose register
	int get_int_register(unsigned reg);

	//set the value of the given integer general purpose register to "value"
	void set_int_register(unsigned reg, int value);

	//returns value of the specified floating point general purpose register
	float get_fp_register(unsigned reg);

	//set the value of the given floating point general purpose register to "value"
	void set_fp_register(unsigned reg, float value);

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

protected:
	void fp_fetch();
	void fp_decode();
	void fp_execute();
	void fp_memory();
	void fp_writeBack();
	void fp_hazardHandler();

private:

	// returns a free exec unit for the particular instruction type
	unsigned get_free_unit(opcode_t opcode);	

	//reduce execution unit busy time (to be invoked at every clock cycle 
	void decrement_units_busy_time();

	//debug units
	void debug_units();

	unsigned generalP_IntReg[NUM_SP_INT_REGISTERS];//R0-R15
	unsigned generalP_FPReg[NUM_GP_REGISTERS];//F0-F31
	unsigned specialP_Reg[NUM_STAGES][NUM_SP_REGISTERS];

	std::map< std::string, unsigned> fp_labelPCMap;


};

#endif /*SIM_PIPE_FP_H_*/
