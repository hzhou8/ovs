
#define ENGINE_MAX_INPUT 50

struct engine_node;

struct engine_node {
    uint64_t run_id;
    char* name;
    size_t n_inputs;
    struct engine_node *inputs[ENGINE_MAX_INPUT];
    void *data;
    bool changed;
    void *context;
    
    void (*run)(struct engine_node *node);
    /* change_handler handles one input change against "old_data" of all other inputs */
    void (*change_handler[ENGINE_MAX_INPUT])(struct engine_node *input, struct engine_node *node);

    /* optional, but if it is implemented, make sure it is efficient */
    void (*reset_old_data)(struct engine_node *node);
};

void
engine_run(struct engine_node *node, uint64_t run_id);

#define ENGINE_NODE(NAME, N_INPUT, INPUTS, CHG_HLR) \
    struct engine_node en_##NAME = {                \
        .name = "##NAME",                           \
        .n_inputs = N_INPUT,                        \
        .inputs = INPUTS,                           \
        .data = &ed_##NAME,                         \
        .context = &ctx,                            \
        .run = NAME##_run,                          \
        .change_handler = CHG_HLR,                  \
        .reset_old_data = NAME##_rst                \
    };                                              \

