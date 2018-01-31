
#define ENGINE_MAX_INPUT 256 

struct engine_node;

struct engine_node_input {
    struct engine_node *node;
    /* change_handler handles one input change against "old_data" of all
     * other inputs, returns:
     *  - true: if change can be handled
     *  - false: if change cannot be handled (suggesting full recompute)
     */
    bool (*change_handler)(struct engine_node *node);
};

struct engine_node {
    uint64_t run_id;
    char* name;
    size_t n_inputs;
    struct engine_node_input inputs[ENGINE_MAX_INPUT]; 
    void *data;
    bool changed;
    void *context;
    void (*run)(struct engine_node *node);
    /* optional, but if it is implemented, make sure it is efficient */
    void (*reset_old_data)(struct engine_node *node);
};

void
engine_run(struct engine_node *node, uint64_t run_id);

static inline struct engine_node *
engine_get_input(const char *input_name, struct engine_node *node)
{
    size_t i;
    for (i = 0; i < node->n_inputs; i++) {
        if (!strcmp(node->inputs[i].node->name, input_name)) {
            return node->inputs[i].node;
        }
    }
    return NULL;
}

static inline void
engine_add_input(struct engine_node *node, struct engine_node *input,
    bool (*change_handler)(struct engine_node *node))
{
    node->inputs[node->n_inputs].node = input;
    node->inputs[node->n_inputs].change_handler = change_handler;
    node->n_inputs ++;
}

extern bool engine_force_recompute;
static inline void
engine_set_force_recompute(bool val)
{
    engine_force_recompute = val;
}

// TODO: add engine_reset(), which shall be called at the end of main loop,
// to call reset_old_data for each node, because SB DB table tracking must
// be reset at the end of each loop, instead beginning of next processing.

#define ENGINE_NODE(NAME, NAME_STR) \
    struct engine_node en_##NAME = { \
        .name = NAME_STR, \
        .data = &ed_##NAME, \
        .context = &ctx, \
        .run = NAME##_run, \
        .reset_old_data = NAME##_rst \
    };

#define ENGINE_FUNC_OVSDB(DB_NAME, TBL_NAME, IDL) \
static void \
DB_NAME##_##TBL_NAME##_run(struct engine_node *node) \
{ \
    static bool first_run = true; \
    if (first_run) { \
        first_run = false; \
        node->changed = true; \
        return; \
    } \
    struct controller_ctx *ctx = (struct controller_ctx *)node->context; \
    if (DB_NAME##rec_##TBL_NAME##_track_get_first(IDL)) { \
        node->changed = true; \
        return; \
    } \
    node->changed = false; \
} \
 \
static void \
DB_NAME##_##TBL_NAME##_rst(struct engine_node *node OVS_UNUSED) \
{ \
    /* DB_NAME##_node_rst(node); */ \
}

#define ENGINE_FUNC_SB(TBL_NAME) \
    ENGINE_FUNC_OVSDB(sb, TBL_NAME, ctx->ovnsb_idl)

#define ENGINE_FUNC_OVS(TBL_NAME) \
    ENGINE_FUNC_OVSDB(ovs, TBL_NAME, ctx->ovs_idl)

#define ENGINE_NODE_SB(TBL_NAME, TBL_NAME_STR) \
    void *ed_sb_##TBL_NAME; \
    ENGINE_NODE(sb_##TBL_NAME, TBL_NAME_STR)

#define ENGINE_NODE_OVS(TBL_NAME, TBL_NAME_STR) \
    void *ed_ovs_##TBL_NAME; \
    ENGINE_NODE(ovs_##TBL_NAME, TBL_NAME_STR)
