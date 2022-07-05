#pragma once

#include "base_types.h"

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
    node_plug_definition_t* inputs;

    uint32_t output_count;
    node_plug_definition_t* outputs;

    void (*evaluate)(const node_plug_value_t* inputs,
                     node_plug_value_t* outputs);
} node_type_definition_t;

typedef struct node_type_id_t
{
    uint32_t index;
} node_type_id_t;

typedef struct plug_input_connection_t
{
    uint32_t source_node_id;
    uint32_t source_plug_id;
} plug_input_connection_t;

typedef struct node_t
{
    node_type_id_t type;

    plug_input_connection_t* inputs;
} node_t;

typedef struct node_graph_t
{
    uint32_t node_type_count;
    uint32_t node_type_capacity;
    node_type_definition_t* node_types;

    uint32_t node_count;
    uint32_t node_capacity;
    node_t* nodes;
} node_graph_t;

node_graph_t* node_graph_create();

node_type_id_t add_node_type(node_graph_t* graph, node_type_definition_t def);
uint32_t add_node(node_graph_t* graph, node_type_id_t type);
void connect_nodes(node_graph_t* graph,
                   uint32_t input_node_index,
                   uint32_t input_plug_index,
                   uint32_t output_node_index,
                   uint32_t output_plug_index);
