#include "evaluation_graph.h"

#include "assert.h"

node_type_id_t add_node_type(node_graph_t* graph, node_type_definition_t def)
{
    ASSERT(graph->node_type_count < graph->node_type_capacity);
    graph->node_types[graph->node_type_count++] = def;

    return (node_type_id_t){graph->node_type_count - 1};
}

uint32_t add_node(node_graph_t* graph, node_type_id_t type)
{
    ASSERT(graph->node_count < graph->node_capacity);
    graph->nodes[graph->node_count++] = (node_t){.type = type};

    return graph->node_count - 1;
}

void connect_nodes(node_graph_t* graph,
                   uint32_t input_node_index,
                   uint32_t input_plug_index,
                   uint32_t output_node_index,
                   uint32_t output_plug_index)
{
}
