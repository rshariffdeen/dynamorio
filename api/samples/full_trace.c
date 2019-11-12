#include "dr_api.h"
#include "string.h"
#ifdef WINDOWS
# define DISPLAY_STRING(msg) dr_messagebox(msg)
#else
# define DISPLAY_STRING(msg) dr_printf("%s\n", msg)
#endif

typedef struct bb_counts {
    uint64 blocks;
    uint64 total_size;
} bb_counts;

static bb_counts counts_as_built;
void *as_built_lock;
static bb_counts counts_dynamic;
void *count_lock;
byte* base_address;
const char *look_address;


static void event_exit(void);
static dr_emit_flags_t event_basic_block(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating);
static void clean_call(uint instruction_count);

DR_EXPORT void dr_client_main(client_id_t id, int argc, const char *argv[]) {
    base_address= dr_get_client_base(id);

    /* register events */
    dr_register_exit_event(event_exit);
    dr_register_bb_event(event_basic_block);

    /* initialize lock */
    as_built_lock = dr_mutex_create();
    count_lock = dr_mutex_create();

}
static void event_exit(void) {
    /* Display results - we must first snpritnf the string as on windows
     * dr_printf(), dr_messagebox() and dr_fprintf() can't print floats. */
    char msg[512];
    int len;
    len = snprintf(msg, sizeof(msg)/sizeof(msg[0]),
                   "Number of conditional blocks built : %"UINT64_FORMAT_CODE"\n"
                   "     Total count      :  %"UINT64_FORMAT_CODE"\n"
                   "     Average size      : %5.2lf instructions\n"
                   "Number of blocks executed  : %"UINT64_FORMAT_CODE"\n"
                   "     Average weighted size : %5.2lf instructions\n",
                   counts_as_built.blocks,
                   counts_as_built.total_size,
                   counts_as_built.total_size / (double)counts_as_built.blocks,
                   counts_dynamic.blocks,
                   counts_dynamic.total_size / (double)counts_dynamic.blocks);

    DR_ASSERT(len > 0);
    msg[sizeof(msg)/sizeof(msg[0])-1] = '\0';
    DISPLAY_STRING(msg);
    /* free mutex */
    dr_mutex_destroy(as_built_lock);
    dr_mutex_destroy(count_lock);
}


static void print_instruction(instr_t *instr, void *drcontext){
    const char *opcode;
    char msg[512];
    opcode = decode_opcode_name(instr_get_opcode(instr));
    app_pc instr_addr = instr_get_app_pc(instr);
    snprintf(msg, sizeof(msg)/sizeof(msg[0]), "\n[Instruction-Info]: %s - Address %lx", opcode, (ptr_uint_t)(instr_addr));
    DISPLAY_STRING(msg);

    int length = instr_length(drcontext, instr);
    int index = 0;
    snprintf(msg, sizeof(msg)/sizeof(msg[0]), "[Instruction](%d):", length);
    DISPLAY_STRING(msg);
    for (int i=0; i < length; i++){
        snprintf(msg + i*3, sizeof(msg)/sizeof(msg[0]) - index, "%02x ", instr_get_raw_byte(instr,i));
    }
    DISPLAY_STRING(msg);
}


static void clean_call(uint instruction_count)
{
    dr_mutex_lock(count_lock);
    counts_dynamic.blocks++;
    counts_dynamic.total_size += instruction_count;
    dr_mutex_unlock(count_lock);
}

static dr_emit_flags_t event_basic_block(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating) {
    uint num_instructions = 0, count_block = 0;
    instr_t *instr;

    /* count the number of instructions in this block */
    for (instr = instrlist_first(bb); instr != NULL; instr = instr_get_next(instr)) {
        print_instruction(instr, drcontext);
    }


    /* update the as-built counts */
    dr_mutex_lock(as_built_lock);
    counts_as_built.blocks += count_block;
    counts_as_built.total_size += num_instructions;
    dr_mutex_unlock(as_built_lock);

    /* insert clean call */
    dr_insert_clean_call(drcontext, bb, instrlist_first(bb), clean_call, false, 1, OPND_CREATE_INT32(num_instructions));

    return DR_EMIT_DEFAULT;
}