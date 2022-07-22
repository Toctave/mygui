#include "evaluation_graph.h"

#include "assert.h"
#include "memory.h"
#include "stretchy_buffer.h"
#include <stdint.h>

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

node_type_definition_t* get_node_type(const node_graph_t* graph,
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
        uint32_t input = target_node->plugs[i].connected_node;

        if (input && depends_on_node(graph, input, source))
        {
            return true;
        }
    }

    return false;
}

bool is_input(const node_graph_t* graph, uint32_t node, uint32_t plug_index)
{
    const node_type_definition_t* type = get_node_type(graph, node);
    ASSERT(plug_index < type->plug_count);
    return plug_index < type->input_count;
}

static const node_plug_definition_t*
get_plug_definition(const node_graph_t* graph, uint32_t node, uint32_t plug)
{
    return &get_node_type(graph, node)->plugs[plug];
}

static node_plug_state_t*
get_plug_state(node_graph_t* graph, uint32_t node, uint32_t plug)
{
    return &graph->nodes[node].plugs[plug];
}

bool can_connect_nodes(node_graph_t* graph,
                       uint32_t node1,
                       uint32_t plug1,
                       uint32_t node2,
                       uint32_t plug2)
{
    bool plug1_is_input = is_input(graph, node1, plug1);
    bool plug2_is_input = is_input(graph, node2, plug2);

    uint32_t src_node, src_plug, dst_node, dst_plug;
    if (plug1_is_input)
    {
        if (plug2_is_input)
        {
            return false;
        }
        else
        {
            src_node = node2;
            src_plug = plug2;

            dst_node = node1;
            dst_plug = plug1;
        }
    }
    else
    {
        if (plug2_is_input)
        {
            src_node = node1;
            src_plug = plug1;

            dst_node = node2;
            dst_plug = plug2;
        }
        else
        {
            return false;
        }
    }

    bool compatible_types =
        (get_plug_definition(graph, src_node, src_plug)->type
         == get_plug_definition(graph, dst_node, dst_plug)->type);

    return compatible_types && !depends_on_node(graph, src_node, dst_node);
}

void connect_nodes(node_graph_t* graph,
                   uint32_t src_node,
                   uint32_t src_plug,
                   uint32_t dst_node,
                   uint32_t dst_plug)
{
    ASSERT(can_connect_nodes(graph, src_node, src_plug, dst_node, dst_plug));
    if (is_input(graph, src_node, src_plug))
    {
        node_plug_state_t* plug = get_plug_state(graph, src_node, src_plug);
        plug->connected_node = dst_node;
        plug->connected_plug = dst_plug;
    }
    else
    {
        node_plug_state_t* plug = get_plug_state(graph, dst_node, dst_plug);
        plug->connected_node = src_node;
        plug->connected_plug = src_plug;
    }
}

void disconnect_node(node_graph_t* graph, uint32_t dst_node, uint32_t dst_plug)
{
    ASSERT(is_input(graph, dst_node, dst_plug));

    node_plug_state_t* plug = get_plug_state(graph, dst_node, dst_plug);
    plug->connected_node = 0;
    plug->connected_plug = 0;
}

void evaluate_schedule(node_graph_t* graph)
{
    for (uint32_t i = 1; i < array_count(graph->nodes); i++)
    {
        uint32_t node_index = graph->schedule[i];

        node_t* node = array_safe_get(graph->nodes, node_index);
        node_type_definition_t* type = get_node_type(graph, node_index);

        node_plug_value_t inputs[MAX_PLUG_COUNT];
        node_plug_value_t outputs[MAX_PLUG_COUNT];

        // get inputs from connected nodes, if any
        for (uint32_t plug_index = 0; plug_index < type->input_count;
             plug_index++)
        {
            node_plug_state_t* plug =
                get_plug_state(graph, node_index, plug_index);
            if (plug->connected_node)
            {
                plug->value = get_plug_state(graph,
                                             plug->connected_node,
                                             plug->connected_plug)
                                  ->value;
            }

            inputs[plug_index] = plug->value;
        }

        // call node evaluation function
        type->evaluate(inputs, outputs);

        // collect the results back into the node's plugs
        for (uint32_t plug_index = type->input_count;
             plug_index < type->plug_count;
             plug_index++)
        {
            node->plugs[plug_index].value =
                outputs[plug_index - type->input_count];
        }
    }
}

node_plug_value_t*
get_plug_value(node_graph_t* graph, uint32_t node_index, uint32_t plug_index)
{
    node_plug_state_t* plug =
        &array_safe_get(graph->nodes, node_index)->plugs[plug_index];

    return &plug->value;
}

static void insert_into_schedule(node_graph_t* graph,
                                 bool* scheduled,
                                 uint32_t node_index,
                                 uint32_t* scheduled_count)
{
    if (scheduled[node_index])
    {
        return;
    }

    node_type_definition_t* type = get_node_type(graph, node_index);

    for (uint32_t plug_index = 0; plug_index < type->plug_count; plug_index++)
    {
        node_plug_state_t* plug = get_plug_state(graph, node_index, plug_index);

        if (plug->connected_node)
        {
            insert_into_schedule(graph,
                                 scheduled,
                                 plug->connected_node,
                                 scheduled_count);
        }
    }

    graph->schedule[*scheduled_count] = node_index;
    (*scheduled_count)++;

    scheduled[node_index] = true;
}

void build_schedule(mem_allocator_i* alloc, node_graph_t* graph)
{
    uint32_t node_count = array_count(graph->nodes);
    array_reserve(alloc, graph->schedule, node_count);

    // TODO(octave) : could use a bitfield
    bool* scheduled = mem_alloc(alloc, sizeof(bool) * node_count);

    for (uint32_t i = 1; i < node_count; i++)
    {
        scheduled[i] = false;
    }

    uint32_t scheduled_count = 1;
    for (uint32_t i = 1; i < node_count; i++)
    {
        insert_into_schedule(graph, scheduled, i, &scheduled_count);
    }

    ASSERT(scheduled_count == node_count);

    mem_free(alloc, scheduled, sizeof(bool) * node_count);
}
