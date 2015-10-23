/* Common inline function */

struct namespace_list {
        int namespace_counter;
        namespace_node_data_t *pNamespace[NKN_MAX_NAMESPACES];
} ;
extern struct namespace_list g_lstNamespace;

static inline namespace_node_data_t *
get_namespace(int n_index);

/* Get the namespace pointer for the given index */
static inline namespace_node_data_t *
get_namespace(int n_index)
{
        if ((n_index < 0) && (n_index >= g_lstNamespace.namespace_counter))
                return NULL;
        return g_lstNamespace.pNamespace[n_index];
}
