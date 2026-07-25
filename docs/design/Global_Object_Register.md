# Global Object Register

## GOR-STARTUP-001

| Field | Value |
|---|---|
| Global Object ID | `GOR-STARTUP-001` |
| Symbol | `g_startup_vector_table` |
| Type | `const startup_vector_entry_t[TOTAL_VECTOR_COUNT]` |
| Owner | Startup integration module |
| Writer | Link-time initialization only; no runtime writer |
| Reader | Processor exception-entry hardware and `startup_reset_handler()` |
| Synchronization | Not required because the object is immutable after image creation |
| Initialization | Compile-time initializer in `.isr_vector` |
| Lifetime | Entire firmware image lifetime |
| Reason | STM32F446 interrupt-vector and reset integration requires a globally addressable table |
| Verification | Compile/link, vector-table size static assertion, linker placement, and hardware bring-up |

The source reference is located immediately before the object definition in
`Core/Src/startup_stm32f446xx.c`.
