#include "evaluation_graph.h"

#include "assert.h"
#include "stretchy_buffer.h"

void node_graph_init(mem_allocator_i* alloc, node_graph_t* graph)
{
    *graph = (node_graph_t){0};

    array_push(alloc, graph->node_types, (node_type_definition_t){0});
    array_push(alloc, graph->nodes, (node_t){0});
}

uint32_t add_node_type(mem_allocator_i* alloc,
                       node_graph_t* graph,
                       node_type_definition_t def)
{
    // NOTE(octave) : for now, plug_definitions must be kept
    // around. Might want to copy them into the node_graph. That being
    // said, they'll often be statically defined in C code.
    array_push(alloc, graph->node_types, def);
    return array_count(graph->node_types) - 1;
}

uint32_t add_node(mem_allocator_i* alloc, node_graph_t* graph, uint32_t type)
{
    array_push(alloc, graph->nodes, (node_t){.type = type});
    return array_count(graph->nodes) - 1;
}

static node_type_definition_t* get_node_type(node_graph_t* graph,
                                             uint32_t node_index)
{
    ASSERT(node_index < array_count(graph->nodes));

    return &graph->node_types[graph->nodes[node_index].type];
}

// returns whether target depends on source
static bool
depends_on_node(node_graph_t* graph, uint32_t target, uint32_t source)
{
    ASSERT(source && target);

    if (target == source)
    {
        return true;
    }

    node_t* target_node = array_safe_get(graph->nodes, target);
    node_type_definition_t* target_type = get_node_type(graph, target);
    for (uint32_t i = 0; i < target_type->input_count; i++)
    {
        uint32_t input = target_node->inputs[i].node_index;

        if (input && depends_on_node(graph, input, source))
        {
            return true;
        }
    }

    return false;
}

bool connect_nodes(node_graph_t* graph,
                   uint32_t input_node_index,
                   uint32_t input_plug_index,
                   uint32_t output_node_index,
                   uint32_t output_plug_index)
{
    node_t* out = array_safe_get(graph->nodes, output_node_index);

    ASSERT(input_plug_index
           < get_node_type(graph, input_node_index)->output_count);
    ASSERT(output_plug_index
           < get_node_type(graph, output_node_index)->input_count);

    // TODO(octave) : type-checking

    if (depends_on_node(graph, input_node_index, output_node_index))
    {
        return false;
    }
    else
    {
        out->inputs[output_plug_index].node_index = input_node_index;
        out->inputs[output_plug_index].plug_index = input_plug_index;

        return true;
    }
}

node_plug_value_t
stupid_evaluate(node_graph_t* graph, uint32_t node_index, uint32_t plug_index)
{
    node_plug_value_t inputs[MAX_INPUT_COUNT];
    node_plug_value_t outputs[MAX_OUTPUT_COUNT];

    node_t* node = array_safe_get(graph->nodes, node_index);
    node_type_definition_t* type = get_node_type(graph, node_index);

    for (uint32_t i = 0; i < type->input_count; i++)
    {
        if (node->inputs[i].node_index)
        {
            inputs[i] = stupid_evaluate(graph,
                                        node->inputs[i].node_index,
                                        node->inputs[i].plug_index);
        }
        else
        {
            inputs[i] = node->inputs[i].value;
        }
    }

    type->evaluate(inputs, outputs);

    return outputs[plug_index];
}

node_plug_value_t* get_input_value(const node_graph_t* graph,
                                   uint32_t node_index,
                                   uint32_t plug_index)
{
    plug_input_t* input =
        &array_safe_get(graph->nodes, node_index)->inputs[plug_index];
    ASSERT(!input->node_index);

    return &input->value;
}
