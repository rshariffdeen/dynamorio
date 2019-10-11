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

long base_address;

static void
event_exit(void);
static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating);
static void
clean_call(uint instruction_count);
DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    char msg[512];
    snprintf(msg, sizeof(msg)/sizeof(msg[0]), "\nNumber of arguments : %d", argc);
    DISPLAY_STRING(msg);

    if (argc < 2){
        snprintf(msg, sizeof(msg)/sizeof(msg[0]), "Insufficient arguments\n");
        DISPLAY_STRING(msg);
        return;
    }

    const char *address = argv[1];
    snprintf(msg, sizeof(msg)/sizeof(msg[0]), "Looking for address %s", address);
    DISPLAY_STRING(msg);

    byte* base_address_array = dr_get_client_base(id);
    base_address = (base_address_array[0]*10 + base_address_array[1]);
    for (int i=0; i<32; i++){
        snprintf(msg, sizeof(msg)/sizeof(msg[0]), "%hhn", base_address_array[i]);
        DISPLAY_STRING(msg);
    }
    snprintf(msg, sizeof(msg)/sizeof(msg[0]), "Base Address %x%x", base_address_array[0], base_address_array[1]);
    DISPLAY_STRING(msg);
    snprintf(msg, sizeof(msg)/sizeof(msg[0]), "Base Address %hhn", base_address_array);
    DISPLAY_STRING(msg);
    snprintf(msg, sizeof(msg)/sizeof(msg[0]), "Base Address %lx", base_address);
    DISPLAY_STRING(msg);

    /* register events */
    dr_register_exit_event(event_exit);
    dr_register_bb_event(event_basic_block);
    /* initialize lock */
    as_built_lock = dr_mutex_create();
        count_lock = dr_mutex_create();

}
static void
event_exit(void)
{
    /* Display results - we must first snpritnf the string as on windows
     * dr_printf(), dr_messagebox() and dr_fprintf() can't print floats. */
    char msg[512];
    int len;
    len = snprintf(msg, sizeof(msg)/sizeof(msg[0]),
                   "Number of blocks built : %"UINT64_FORMAT_CODE"\n"
                                                                 "     Average size      : %5.2lf instructions\n"
                                      "Number of blocks executed  : %"UINT64_FORMAT_CODE"\n"
                                      "     Average weighted size : %5.2lf instructions\n",
                   counts_as_built.blocks,
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

///* If instr is unsigned division, return true and set *opnd to divisor. */
//static bool
//instr_is_interestnig(instr_t *instr, OUT opnd_t *opnd)
//{
//
//
//    int opc = instr_get_opcode(instr);
//
//    if (opc == OP_div) {
//        *opnd = instr_get_src(instr, 0); /* divisor is 1st src */
//        return true;
//    }
//    return false;
//}


static void
clean_call(uint instruction_count)
{
    dr_mutex_lock(count_lock);
    counts_dynamic.blocks++;
    counts_dynamic.total_size += instruction_count;
    dr_mutex_unlock(count_lock);
}

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating)
{
    uint num_instructions = 0;
    instr_t *instr;
    const char *opcode;
//    app_pc pc;
    char msg[512];



    /* count the number of instructions in this block */
    for (instr = instrlist_first(bb); instr != NULL; instr = instr_get_next(instr)) {
        opcode = decode_opcode_name(instr_get_opcode(instr));
//        pc = instr_get_app_pc(instr);
//        snprintf(msg, sizeof(msg)/sizeof(msg[0]), "\nInstruction Address: %lx", (ptr_int_t)pc);
//        DISPLAY_STRING(msg);
        if (strcmp(opcode, "yufyuf") == 0){
            snprintf(msg, sizeof(msg)/sizeof(msg[0]), "\nInstruction: %s", opcode);
            DISPLAY_STRING(msg);
            num_instructions++;
        }

    }
    /* update the as-built counts */
    dr_mutex_lock(as_built_lock);
    counts_as_built.blocks++;
    counts_as_built.total_size += num_instructions;
    dr_mutex_unlock(as_built_lock);
        /* insert clean call */
                dr_insert_clean_call(drcontext, bb, instrlist_first(bb), clean_call, false, 1,
                                                                OPND_CREATE_INT32(num_instructions));

    return DR_EMIT_DEFAULT;
}