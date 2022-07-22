#pragma once

#include "base_types.h"

#define MAX_PLUG_COUNT 32
#define MAX_PLUG_NAME 256
#define MAX_NODE_TYPE_NAME 256

typedef struct mem_allocator_i mem_allocator_i;

typedef enum node_plug_type_e
{
    PLUG_NONE,
    PLUG_FLOAT,
    PLUG_INTEGER,
} node_plug_type_e;

typedef struct node_plug_definition_t
{
    char name[MAX_PLUG_NAME];
    uint32_t type;
} node_plug_definition_t;

typedef union node_plug_value_t
{
    double floating;
    int64_t integer;
} node_plug_value_t;

typedef struct node_type_definition_t
{
    char name[MAX_NODE_TYPE_NAME];

    uint32_t input_count;
    uint32_t plug_count;
    node_plug_definition_t plugs[MAX_PLUG_COUNT];

    void (*evaluate)(const node_plug_value_t* inputs,
                     node_plug_value_t* outputs);
} node_type_definition_t;

typedef struct plug_state_t
{
    uint32_t input_node;
    uint32_t input_plug;

    node_plug_value_t value;
} plug_state_t;

typedef struct node_t
{
    uint32_t type;

    plug_state_t plugs[MAX_PLUG_COUNT];
    quad_i32_t box;
} node_t;

typedef struct node_graph_t
{
    /* array */ node_type_definition_t* node_types;
    /* array */ node_t* nodes;
} node_graph_t;

node_plug_value_t* get_plug_value(const node_graph_t* graph,
                                  uint32_t node_index,
                                  uint32_t plug_index);

void node_graph_init(mem_allocator_i* alloc, node_graph_t* graph);
uint32_t add_node_type(mem_allocator_i* alloc,
                       node_graph_t* graph,
                       node_type_definition_t def);
uint32_t add_node(mem_allocator_i* alloc, node_graph_t* graph, uint32_t type);

bool can_connect_nodes(node_graph_t* graph,
                       uint32_t src_node,
                       uint32_t src_plug,
                       uint32_t dst_node,
                       uint32_t dst_plug);

void connect_nodes(node_graph_t* graph,
                   uint32_t src_node,
                   uint32_t src_plug,
                   uint32_t dst_node,
                   uint32_t dst_plug);
void stupid_evaluate(node_graph_t* graph,
                     uint32_t node_index,
                     uint32_t plug_index);

bool is_input(const node_graph_t* graph, uint32_t node, uint32_t plug_index);
