#include "misc.h"

#include "network_protocol.h"

// It's really important to keep in mind that most of the functions defined here act as interface for the actual
// protocols implementation and have to be kept inline-able, therefrore small and compact, and must be structured in
// a way that reduce the amount of process memory fetching as much as possible because this can easily affect the
// performances.
// All the interface functions defined here must be defined using the macro NETWORK_PROTOCOL_FUNCTION_INTERFACE, it will
// take care of implementing all the wrappers necessary keeping the code clean and simple.

// NETWORK_PROTOCOL_FUNCTION_INTERFACE_VOID(process_buffer, );