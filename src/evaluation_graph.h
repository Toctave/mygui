#pragma once

#include "base_types.h"

#define MAX_INPUT_COUNT 32
#define MAX_OUTPUT_COUNT 32

typedef struct mem_allocator_i mem_allocator_i;

typedef enum node_plug_type_e
{
    PLUG_NONE,
    PLUG_FLOAT,
    PLUG_INTEGER,
} node_plug_type_e;

typedef struct node_plug_definition_t
{
    const char* name;
    node_plug_type_e type;
} node_plug_definition_t;

typedef union node_plug_value_t
{
    double floating;
    int64_t integer;
} node_plug_value_t;

typedef struct node_type_definition_t
{
    const char* name;

    uint32_t input_count;
    node_plug_definition_t inputs[MAX_INPUT_COUNT];

    uint32_t output_count;
    node_plug_definition_t outputs[MAX_OUTPUT_COUNT];

    void (*evaluate)(const node_plug_value_t* inputs,
                     node_plug_value_t* outputs);
} node_type_definition_t;

typedef struct plug_input_t
{
    uint32_t node_index;
    uint32_t plug_index;

    node_plug_value_t value;
} plug_input_t;

typedef struct node_t
{
    uint32_t type;

    plug_input_t inputs[MAX_INPUT_COUNT];
} node_t;

typedef struct node_graph_t
{
    /* array */ node_type_definition_t* node_types;
    /* array */ node_t* nodes;
} node_graph_t;

node_plug_value_t* get_input_value(const node_graph_t* graph,
                                   uint32_t node_index,
                                   uint32_t plug_index);

void node_graph_init(mem_allocator_i* alloc, node_graph_t* graph);
uint32_t add_node_type(mem_allocator_i* alloc,
                       node_graph_t* graph,
                       node_type_definition_t def);
uint32_t add_node(mem_allocator_i* alloc, node_graph_t* graph, uint32_t type);
bool connect_nodes(node_graph_t* graph,
                   uint32_t input_node_index,
                   uint32_t input_plug_index,
                   uint32_t output_node_index,
                   uint32_t output_plug_index);
node_plug_value_t
stupid_evaluate(node_graph_t* graph, uint32_t node_index, uint32_t plug_index);
