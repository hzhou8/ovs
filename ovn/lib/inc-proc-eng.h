
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
    
    void (*compute)(struct engine_node *node);
    /* change_handler handles one input change against "old_data" of all other inputs */
    void (*change_handler[ENGINE_MAX_INPUT])(struct engine_node *input, struct engine_node *node);

    /* optional, but if they are implemented, they must be efficient */
    void (*evaluate_change)(struct engine_node *node);
    void (*reset_old_data)(struct engine_node *node);
};

void
engine_run(struct engine_node *node, uint64_t run_id);
